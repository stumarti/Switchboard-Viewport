#include "app/ota.h"

#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>

#include "app/display.h"
#include "app/ota_policy.h"
#include "app/store.h"
#include "config.h"
#include "log.h"
#include "net/server.h"
#include "render/system.h"

namespace ota {

namespace {

// mbedtls 2 (Arduino-ESP32 2.x) names these *_ret; 3 doesn't.
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
#define SHA_START(c) mbedtls_sha256_starts((c), 0)
#define SHA_UPDATE(c, b, n) mbedtls_sha256_update((c), (b), (n))
#define SHA_FINISH(c, o) mbedtls_sha256_finish((c), (o))
#else
#define SHA_START(c) mbedtls_sha256_starts_ret((c), 0)
#define SHA_UPDATE(c, b, n) mbedtls_sha256_update_ret((c), (b), (n))
#define SHA_FINISH(c, o) mbedtls_sha256_finish_ret((c), (o))
#endif

void report(const char* version, const char* from, bool ok, const char* error) {
  JsonDocument body;
  body["version"] = version;
  body["from"] = from;
  body["ok"] = ok;
  body["board"] = SWITCHBOARD_BOARD;
  body["error"] = error ? error : "";
  String json;
  serializeJson(body, json);
  server::post("/api/firmware/report", json);
}

uint8_t triesAt(const char* version) { return store::get("failVer", "") == version ? store::getInt("failN", 0) : 0; }

void noteFailure(const char* version) {
  const int tries = triesAt(version);
  store::put("failVer", version);
  store::putInt("failN", tries < 250 ? tries + 1 : tries);
}

void screen(const char* version, const char* status) {
  display::show([&](draw::Gfx& g) { sys::updating(g, version, status); });
}

bool install(const char* version, uint32_t size, const char* sha256) {
  auto fail = [&](const char* why) {
    LOGF("Update to %s failed: %s\n", version, why);
    report(version, FIRMWARE_VERSION, false, why);
    noteFailure(version);
    return false;
  };
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  if (!next || size > next->size) return fail("The firmware doesn't fit this display");

  screen(version, "Downloading from Switchboard Server");
  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(server::base() + "/api/firmware/image/" + server::urlEncode(version))) return fail("Bad server address");
  http.addHeader("Authorization", "Bearer " + server::token());
  http.addHeader("X-Firmware", FIRMWARE_VERSION);
  http.addHeader("X-Board", SWITCHBOARD_BOARD);
  const int code = http.GET();
  if (code != 200) {
    http.end();
    char why[48];
    snprintf(why, sizeof(why), "Download failed (HTTP %d)", code);
    return fail(why);
  }
  if (http.getSize() != static_cast<int>(size)) {
    http.end();
    return fail("The download isn't the size the server announced");
  }
  if (!Update.begin(size, U_FLASH)) {
    http.end();
    return fail(Update.errorString());
  }
  store::put("pendVer", version);
  store::put("pendFrom", FIRMWARE_VERSION);

  otapolicy::MarkerScan marker;
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  SHA_START(&sha);
  WiFiClient* in = http.getStreamPtr();
  static uint8_t buf[4096];
  uint32_t done = 0;
  unsigned long lastByte = millis();
  bool ok = true;
  const char* why = "";
  while (done < size) {
    const size_t avail = in->available();
    if (!avail) {
      if (!http.connected() || millis() - lastByte > 15000) {
        ok = false;
        why = "The download stopped";
        break;
      }
      delay(2);
      continue;
    }
    const size_t want = avail < sizeof(buf) ? avail : sizeof(buf);
    const size_t n = in->readBytes(buf, want < size - done ? want : size - done);
    if (!n) continue;
    lastByte = millis();
    SHA_UPDATE(&sha, buf, n);
    marker.feed(buf, n);
    if (Update.write(buf, n) != n) {
      ok = false;
      why = Update.errorString();
      break;
    }
    done += n;
  }
  http.end();
  uint8_t digest[32];
  SHA_FINISH(&sha, digest);
  mbedtls_sha256_free(&sha);
  if (ok) {
    char hex[65];
    for (int i = 0; i < 32; ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    if (strcasecmp(hex, sha256) != 0) {
      ok = false;
      why = "Checksum mismatch: the download was damaged";
    }
  }
  if (ok && !otapolicy::boardMatches(marker, SWITCHBOARD_BOARD)) {
    ok = false;
    why = "That firmware is for another kind of device";
  }
  if (!ok) {
    Update.abort();
    store::remove("pendVer");
    store::remove("pendFrom");
    return fail(why);
  }
  if (!Update.end(true)) {
    store::remove("pendVer");
    store::remove("pendFrom");
    return fail(Update.errorString());
  }
  screen(version, "Installed: restarting");
  LOGF("Update to %s installed, restarting\n", version);
  return true;
}

}  // namespace

void confirm() {
  esp_ota_mark_app_valid_cancel_rollback();
  const String pend = store::get("pendVer", "");
  if (!pend.length()) return;
  const String from = store::get("pendFrom", "");
  const bool ok = pend == FIRMWARE_VERSION;
  report(pend.c_str(), from.c_str(), ok, ok ? "" : "It started the old firmware again");
  if (!ok) noteFailure(pend.c_str());
  store::remove("pendVer");
  store::remove("pendFrom");
}

void maybeInstall(JsonObjectConst fw, int battPct, int localHour, bool timerWake) {
  const bool enabled = fw["enabled"] | false;
  JsonObjectConst offer = fw["offer"];
  if (!enabled || offer.isNull()) return;
  const char* version = offer["version"] | "";
  const char* sha = offer["sha256"] | "";
  const uint32_t size = offer["size"] | 0u;
  if (!*version || strlen(sha) != 64 || !size) return;
  const uint8_t minBatt = fw["minBattery"] | 30;
  const uint8_t batt = battPct < 0 ? 0 : static_cast<uint8_t>(battPct);
  const bool now = otapolicy::nowDue(enabled, fw["now"] | false, version, batt, minBatt);
  JsonObjectConst sched = fw["schedule"];
  const bool scheduled = timerWake && !sched.isNull() &&
                         otapolicy::scheduledDue(enabled, version, localHour, sched["fromHour"] | -1, sched["toHour"] | -1, batt, minBatt,
                                                 triesAt(version));
  if (!now && !scheduled) return;
  if (install(version, size, sha)) {
    delay(500);
    ESP.restart();
  }
}

}  // namespace ota
