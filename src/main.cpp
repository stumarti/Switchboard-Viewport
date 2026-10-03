// =============================================================================
// Switchboard Viewport — a Switchboard wall display on the reTerminal E1002.
//
// Every wake (a timer the server set, or a button):
//   1. which button, if any (held = its long press)
//   2. the battery: at 2% or less, "CHARGE ME" and back to sleep, no Wi-Fi
//   3. Wi-Fi (setup by QR code and the phone if it has none, or on a held
//      middle button), the clock, Switchboard Server (found by mDNS)
//   4. pairing: until it's approved on the server, a screen saying so
//   5. the bundle (its layout, Wi-Fi list, clock, firmware offer; 304 when
//      unchanged), the theme packs, an update if one is due
//   6. the carousel: which screen (right/left step, middle refreshes, the
//      layout's carousel mode on a timer)
//   7. that screen's state (304 when unchanged: the panel isn't refreshed),
//      every icon it needs, then one full refresh
//   8. deep sleep for as long as the server says (sooner if the carousel
//      has something due), a button waking it early
//
// Nothing about the house is configured here: it's all Switchboard Server's.
// =============================================================================
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

#include <algorithm>
#include <vector>

#include "app/carousel.h"
#include "app/display.h"
#include "app/hw.h"
#include "app/ota.h"
#include "app/ota_policy.h"
#include "app/store.h"
#include "app/theme.h"
#include "config.h"
#include "log.h"
#include "net/server.h"
#include "net/wifi.h"
#include "render/icons.h"
#include "render/screens.h"
#include "render/system.h"

// The image says what it is, for the server's update checks.
__attribute__((used)) static const char kMarker[] = "SWITCHBOARD_FW:" SWITCHBOARD_BOARD ":" FIRMWARE_VERSION;

// New firmware stays "pending" until ota::confirm(): if it can't reach the
// server, the bootloader rolls back to the previous one.
extern "C" bool verifyRollbackLater() { return true; }

namespace {

// What's on the panel, kept through deep sleep, so an unchanged screen
// isn't refreshed again.
enum class Panel : uint8_t { None, Screen, Error, Setup, Pairing, NotSetUp, Info, Clear, Charge, Splash };
constexpr uint32_t MAGIC = 0x53425631;  // "SBV1"

RTC_DATA_ATTR uint32_t rtcMagic = 0;
RTC_DATA_ATTR uint32_t rtcBoots = 0;
RTC_DATA_ATTR Panel rtcPanel = Panel::None;
RTC_DATA_ATTR char rtcPanelKey[64] = "";  // which error / pairing status
RTC_DATA_ATTR char rtcScreen[64] = "";    // the screen showing
RTC_DATA_ATTR char rtcEtag[48] = "";      // its state's ETag
RTC_DATA_ATTR bool rtcQuiet = false;
RTC_DATA_ATTR int64_t rtcLastPress = 0;
RTC_DATA_ATTR int64_t rtcLastChange = 0;
RTC_DATA_ATTR uint32_t rtcSleep = 0;  // the last plan, for wakes that can't ask
// The layout's refresh interval and quiet hours, from the last bundle: how
// long an error or the charge screen sleeps (the panel's 30 minutes by day,
// 60 at night), even when the server can't be asked.
RTC_DATA_ATTR uint32_t rtcPlanSec = 0;
RTC_DATA_ATTR uint32_t rtcQuietSec = 0;
RTC_DATA_ATTR int8_t rtcQuietFrom = -1, rtcQuietTo = -1;

int localHour();

uint32_t planSleep() {
  const int h = localHour();
  if (rtcQuietSec && otapolicy::inWindow(h, rtcQuietFrom, rtcQuietTo)) return rtcQuietSec;
  return rtcPlanSec ? rtcPlanSec : SLEEP_ERROR_SEC;
}

int g_batt = -1;
float g_volts = NAN;

int64_t nowEpoch() {
  const time_t t = time(nullptr);
  return t > 1600000000 ? static_cast<int64_t>(t) : 0;
}

void localTime(char* out, size_t cap, const char* fmt, const char* fallback) {
  struct tm tm;
  if (nowEpoch() && getLocalTime(&tm, 50)) strftime(out, cap, fmt, &tm);
  else snprintf(out, cap, "%s", fallback);
}

int localHour() {
  struct tm tm;
  return nowEpoch() && getLocalTime(&tm, 50) ? tm.tm_hour : -1;
}

void setClock(int utcOffsetMin, const String& ntp) {
  configTime(static_cast<long>(utcOffsetMin) * 60, 0, ntp.length() ? ntp.c_str() : "pool.ntp.org", "pool.ntp.org");
  // Wait for the time only if there isn't one yet (the RTC keeps it through
  // deep sleep; SNTP corrects it in the background).
  for (int i = 0; i < 30 && !nowEpoch(); ++i) delay(100);
}

void remember(Panel p, const char* key) {
  rtcPanel = p;
  snprintf(rtcPanelKey, sizeof(rtcPanelKey), "%s", key ? key : "");
  if (p != Panel::Screen) rtcEtag[0] = 0;
}

bool alreadyShowing(Panel p, const char* key) { return rtcPanel == p && !strcmp(rtcPanelKey, key ? key : ""); }

[[noreturn]] void sleepNow(uint32_t sec) {
  wifi::off();
  rtcSleep = sec;
  hw::sleepFor(sec);
}

// The kitchen panel's error screen. Like the panel, it's drawn on every
// wake that fails, so its date line says when it last tried; then sleep for
// the usual interval.
[[noreturn]] void fail(const char* icon, const char* title, const char* detail) {
  LOGF("Error: %s (%s)\n", title, detail);
  char when[40];
  localTime(when, sizeof(when), "%a %d %b %Y  %H:%M", "--:--");
  display::show([&](draw::Gfx& g) { sys::error(g, icon, title, detail, when); });
  remember(Panel::Error, title);
  sleepNow(planSleep());
}

[[noreturn]] void serverDown() {
  char detail[96];
  snprintf(detail, sizeof(detail), "Can't reach Switchboard Server at %s", server::address().c_str());
  fail("vx_server_off", "Not connected to Switchboard", detail);
}

String pageUrl() { return server::base() + "/#/viewports/" + server::urlEncode(wifi::mac()); }

[[noreturn]] void waitForApproval(const String& status) {
  if (!alreadyShowing(Panel::Pairing, status.c_str())) {
    const String name = wifi::mac(), base = server::base(), page = pageUrl();
    display::show([&](draw::Gfx& g) { sys::pairing(g, {name.c_str(), base.c_str(), page.c_str(), status.c_str()}); });
    remember(Panel::Pairing, status.c_str());
  }
  sleepNow(SLEEP_PENDING_SEC);
}

// Paired (a token kept), or a screen saying it's waiting.
void ensurePaired() {
  LOGF("[boot] check pairing: token=%s\n", server::token().length() ? "present" : "missing");
  if (server::token().length()) return;
  String status;
  const server::Pairing p = server::pair(status);
  LOGF("[boot] pairing result=%d status=%s\n", static_cast<int>(p), status.c_str());
  if (p == server::Pairing::Unreachable) serverDown();
  if (p != server::Pairing::Approved) waitForApproval(status);
}

// The bundle: from the server (304 = the cached one still holds). If the cache
// is missing or unreadable, retry once without the ETag so it can repull.
bool fetchBundle(JsonDocument& bundle) {
  const String etag = store::get("bundleEtag", "");
  const bool hasCache = etag.length() && store::exists("/bundle.json");
  server::Response r = server::get("/api/viewports/me/bundle", hasCache ? etag.c_str() : nullptr);
  if (r.code == 401 || r.code == 403 || r.code == 404) {
    // The server doesn't know this token (re-installed, or the display was
    // removed): register again.
    server::forgetToken();
    ensurePaired();
    r = server::get("/api/viewports/me/bundle");
  }

  String text;
  if (r.code == 304) {
    text = store::readText("/bundle.json");
    if (text.length() == 0) {
      LOGF("[bundle] 304 cache missing/empty, retrying without ETag\n");
      r = server::get("/api/viewports/me/bundle");
      if (r.code == 200) {
        text = r.body;
        store::writeText("/bundle.json", text);
        store::put("bundleEtag", r.etag);
      }
    }
  }
  if (r.code == 200) {
    text = r.body;
    store::writeText("/bundle.json", text);
    store::put("bundleEtag", r.etag);
  }
  if (r.code != 200 && r.code != 304) return false;
  if (text.length() == 0) {
    LOGF("[bundle] no bundle body from server or cache\n");
    return false;
  }
  if (deserializeJson(bundle, text)) {
    LOGF("[bundle] failed to parse bundle JSON; retrying without ETag\n");
    if (etag.length()) {
      r = server::get("/api/viewports/me/bundle");
      if (r.code == 200) {
        text = r.body;
        store::writeText("/bundle.json", text);
        store::put("bundleEtag", r.etag);
        if (!deserializeJson(bundle, text)) return true;
      }
    }
    return false;
  }
  LOGF("[bundle] loaded bundle OK from %s\n", r.code == 304 ? "cache" : "server");
  return true;
}

[[noreturn]] void showInfo() {
  JsonDocument bundle;
  deserializeJson(bundle, store::readText("/bundle.json"));
  char time[8];
  localTime(time, sizeof(time), "%H:%M", "--:--");
  const String srv = store::get("server", store::get("srvcache", "not found yet").c_str());
  const std::vector<store::Network> nets = store::networks();
  const String wifiLine = nets.empty() ? String("No Wi-Fi saved") : "Wi-Fi " + nets[0].ssid;
  const String mac = "MAC " + WiFi.macAddress();
  const char* name = bundle["name"] | "Not paired yet";
  const char* layout = bundle["dashboard"]["name"] | "";
  const int refresh = bundle["layout"]["refreshIntervalMin"] | 30;
  // With direct buttons, each says which screen it shows.
  const bool direct = !strcmp(bundle["layout"]["carousel"]["buttons"] | "step", "direct");
  std::vector<String> titles;
  for (JsonObjectConst s : bundle["layout"]["screens"].as<JsonArrayConst>())
    if (s["enabled"] | true) titles.push_back(s["title"] | "");
  auto title = [&](int i) { return titles.empty() ? "" : titles[i < 0 ? titles.size() - 1 : std::min<size_t>(i, titles.size() - 1)].c_str(); };
  display::show([&](draw::Gfx& g) {
    sys::Info in{refresh, time, g_batt, srv.c_str(), name, layout, FIRMWARE_VERSION, mac.c_str(), wifiLine.c_str()};
    in.direct = direct;
    in.left = title(-1);
    in.middle = title(0);
    in.right = title(1);
    sys::info(g, in);
  });
  remember(Panel::Info, "");
  sleepNow(rtcSleep ? rtcSleep : SLEEP_DEFAULT_SEC);
}

carousel::Wake carouselWake(hw::Wake w) {
  switch (w) {
    case hw::Wake::Timer: return carousel::Wake::Timer;
    case hw::Wake::Next: return carousel::Wake::Next;
    case hw::Wake::Prev: return carousel::Wake::Prev;
    case hw::Wake::PowerOn: return carousel::Wake::Boot;
    default: return carousel::Wake::Refresh;
  }
}

bool buttonWake(hw::Wake w) { return w != hw::Wake::Timer && w != hw::Wake::PowerOn; }

}  // namespace

void setup() {
  Serial1.begin(115200, SERIAL_8N1, SERIAL_RX, SERIAL_TX);
  if (rtcMagic != MAGIC) {
    rtcMagic = MAGIC;
    rtcBoots = 0;
    rtcPanel = Panel::None;
    rtcPanelKey[0] = rtcScreen[0] = rtcEtag[0] = 0;
    rtcQuiet = false;
    rtcLastPress = rtcLastChange = 0;
    rtcSleep = 0;
  }
  ++rtcBoots;
  hw::begin();
  const hw::Wake wake = hw::wakeReason();
  LOGF("\n=== %s %s, wake #%u: %s ===\n", kMarker, WiFi.macAddress().c_str(), static_cast<unsigned>(rtcBoots), hw::wakeName(wake));

  store::begin();
  theme::load();
  display::begin();

  // First power-on: something on the panel during the start-up, rather than
  // whatever it last showed.
  if (wake == hw::Wake::PowerOn && rtcPanel == Panel::None) {
    display::show([](draw::Gfx& g) { sys::splash(g, "starting up"); });
    remember(Panel::Splash, "");
  }

  // ---- Critical battery: no Wi-Fi, no refresh ----
  g_volts = hw::batteryVoltage();
  g_batt = hw::batteryPercent(g_volts);
  if (g_batt <= BATT_CRITICAL_PCT) {
    LOGF("Battery critical (%d%%)\n", g_batt);
    if (rtcPanel != Panel::Charge) {
      display::show([](draw::Gfx& g) { sys::charge(g, g_batt); });
      remember(Panel::Charge, "");
    }
    sleepNow(planSleep());
  }

  // ---- Held buttons that need no network ----
  if (wake == hw::Wake::ClearHold) {
    display::clear();
    remember(Panel::Clear, "");
    sleepNow(rtcSleep ? rtcSleep : SLEEP_DEFAULT_SEC);
  }
  if (wake == hw::Wake::InfoHold) showInfo();

  // ---- Wi-Fi ----
  if (wake == hw::Wake::SetupHold || store::networks().empty()) {
    remember(Panel::Setup, "");
    wifi::setup(store::networks().empty() ? "No Wi-Fi saved yet" : "");
    // Timed out (or cancelled): with nothing saved, wait for a button.
    if (store::networks().empty()) sleepNow(SLEEP_MAX_SEC);
  }
  {
    float t = NAN, h = NAN;
    hw::climate(t, h);
    server::setHealth({g_batt, t, h, g_volts});
  }
  if (!wifi::connect()) {
    // Never joined at all: the saved network must be wrong, so set up again.
    if (!store::getInt("joined", 0)) {
      remember(Panel::Setup, "");
      wifi::setup("Couldn't join the saved Wi-Fi");
      sleepNow(SLEEP_MAX_SEC);
    }
    fail("vx_wifi_off", "No WiFi connection", "Check network or router");
  }
  setClock(store::getInt("utcoff", 0), store::get("ntp", ""));

  // ---- Switchboard Server ----
  if (!server::resolve()) fail("vx_server_off", "Not connected to Switchboard", "Can't find Switchboard Server on this network");
  ensurePaired();
  JsonDocument bundle;
  if (!fetchBundle(bundle)) serverDown();
  ota::confirm();

  // The clock and Wi-Fi the server keeps for every device.
  {
    const int off = bundle["utcOffsetMin"] | 0;
    const String ntp = bundle["ntpServer"] | "pool.ntp.org";
    if (off != store::getInt("utcoff", 0) || ntp != store::get("ntp", "")) {
      store::putInt("utcoff", off);
      store::put("ntp", ntp);
      setClock(off, ntp);
    }
    std::vector<store::Network> nets;
    for (JsonObjectConst n : bundle["wifiNetworks"].as<JsonArrayConst>()) {
      const String ssid = n["name"] | "";
      if (ssid.length()) nets.push_back({ssid, n["password"] | ""});
    }
    store::setServerNetworks(nets);
  }

  JsonObjectConst layout = bundle["layout"];
  {
    rtcPlanSec = (layout["refreshIntervalMin"] | 30) * 60u;
    JsonObjectConst q = layout["quietHours"];
    const bool on = (q["enabled"] | false) && (q["start"] | 0) != (q["end"] | 0);
    rtcQuietSec = on ? (q["intervalMin"] | 60) * 60u : 0;
    rtcQuietFrom = on ? static_cast<int8_t>(q["start"] | 23) : -1;
    rtcQuietTo = on ? static_cast<int8_t>(q["end"] | 6) : -1;
  }
  if (!(bundle["assigned"] | false)) {
    if (!alreadyShowing(Panel::NotSetUp, "")) {
      const String page = pageUrl();
      display::show([&](draw::Gfx& g) { sys::notSetUp(g, page.c_str()); });
      remember(Panel::NotSetUp, "");
    }
    sleepNow(SLEEP_DEFAULT_SEC);
  }

  theme::sync();
  ota::maybeInstall(bundle["firmware"], g_batt, localHour(), wake == hw::Wake::Timer);

  // ---- The carousel ----
  std::vector<String> ids;
  screens::Ctx ctx;
  std::vector<String> marks;
  for (JsonObjectConst s : layout["screens"].as<JsonArrayConst>()) {
    if (!(s["enabled"] | true)) continue;
    ids.push_back(s["id"] | "");
    marks.push_back(s["icon"] | "");
  }
  if (ids.empty()) fail("vx_not_set_up", "Nothing to show", "This display's layout has no screens switched on");
  int current = 0;
  for (size_t i = 0; i < ids.size(); ++i)
    if (ids[i] == rtcScreen) current = static_cast<int>(i);
  carousel::Plan plan;
  plan.mode = carousel::modeOf(layout["carousel"]["mode"] | "stay");
  plan.everyMin = layout["carousel"]["everyMin"] | 30;
  plan.direct = !strcmp(layout["carousel"]["buttons"] | "step", "direct");
  const int64_t now = nowEpoch();
  const int index = carousel::pick(static_cast<int>(ids.size()), current, carouselWake(wake), plan, now, rtcLastPress, rtcLastChange);
  if (buttonWake(wake)) rtcLastPress = now;
  if (index != current || strcmp(rtcScreen, ids[index].c_str()) != 0) rtcLastChange = now;
  const String screenId = ids[index];

  // ---- The screen's state ----
  // A button press, another screen, or something else on the panel: draw
  // whatever comes. Otherwise only when it changed.
  const bool force = buttonWake(wake) || wake == hw::Wake::PowerOn || rtcPanel != Panel::Screen || screenId != rtcScreen;
  const String cache = "/state/" + store::safeName(screenId.c_str()) + ".json";
  server::Response r = server::get("/api/viewports/me/state?screen=" + server::urlEncode(screenId), force ? nullptr : rtcEtag);
  JsonDocument state;
  uint32_t refreshIn = r.refreshIn;
  bool quiet = r.quiet;
  if (r.code == 200) {
    if (deserializeJson(state, r.body)) serverDown();
    store::writeText(cache.c_str(), r.body);
    refreshIn = state["refreshInSec"] | refreshIn;
    quiet = state["quiet"] | quiet;
  } else if (r.code == 304) {
    // Unchanged. Only quiet hours starting or ending (the footer's icon)
    // still needs a redraw, from the copy kept.
    if (quiet == rtcQuiet) {
      LOGF("Screen %s unchanged\n", screenId.c_str());
      sleepNow(carousel::sleepSec(refreshIn, index, plan, now, rtcLastPress, rtcLastChange, SLEEP_MIN_SEC, SLEEP_MAX_SEC));
    }
    if (deserializeJson(state, store::readText(cache.c_str()))) {
      rtcEtag[0] = 0;  // no copy: fetch it whole next time
      sleepNow(SLEEP_MIN_SEC);
    }
  } else if (r.code == 409 || r.code == 502) {
    JsonDocument err;
    deserializeJson(err, r.body);
    // The panel's words: a rejected token, or no answer.
    const String why = err["error"] | "";
    fail("vx_cloud_off", "Can't reach Home Assistant",
         why.indexOf("401") >= 0 ? "Authentication failed - check token" : "No response from Home Assistant");
  } else if (r.code == 404) {
    // The layout lost that screen: start from the first next time.
    rtcScreen[0] = 0;
    sleepNow(SLEEP_MIN_SEC);
  } else {
    serverDown();
  }

  // ---- Draw: fetch what the screen needs, then one refresh ----
  localTime(ctx.time, sizeof(ctx.time), "%H:%M", "--:--");
  ctx.battPct = g_batt;
  ctx.quiet = quiet;
  ctx.markCount = static_cast<int>(marks.size() < 12 ? marks.size() : 12);
  for (int i = 0; i < ctx.markCount; ++i) ctx.marks[i] = marks[i].c_str();
  ctx.current = index;
  JsonObjectConst data = state["data"];
  {
    screens::NullCanvas none;
    icons::beginCollect();
    screens::drawScreen(none, data, ctx);
    icons::endCollect();
    theme::fetchNeeds();
  }
  wifi::off();  // nothing more to fetch: save the battery while the panel refreshes
  LOGF("Drawing %s\n", screenId.c_str());
  display::show([&](draw::Gfx& g) { screens::drawScreen(g, data, ctx); });
  remember(Panel::Screen, "");
  snprintf(rtcScreen, sizeof(rtcScreen), "%s", screenId.c_str());
  snprintf(rtcEtag, sizeof(rtcEtag), "%s", (state["etag"] | r.etag.c_str()));
  rtcQuiet = quiet;
  sleepNow(carousel::sleepSec(refreshIn, index, plan, now, rtcLastPress, rtcLastChange, SLEEP_MIN_SEC, SLEEP_MAX_SEC));
}

void loop() {
  // Never reached: setup() always ends in deep sleep.
}
