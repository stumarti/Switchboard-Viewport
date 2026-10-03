#include "net/server.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "app/store.h"
#include "config.h"
#include "log.h"
#include "net/wifi.h"

namespace server {

namespace {
Health g_health;
String g_host;
uint16_t g_port = SERVER_DEFAULT_PORT;
bool g_mdns = false;

bool parseAddress(const String& s, String& host, uint16_t& port) {
  String a = s;
  a.trim();
  a.replace("http://", "");
  while (a.endsWith("/")) a.remove(a.length() - 1);
  if (!a.length()) return false;
  const int colon = a.lastIndexOf(':');
  host = colon > 0 ? a.substring(0, colon) : a;
  port = colon > 0 ? static_cast<uint16_t>(a.substring(colon + 1).toInt()) : SERVER_DEFAULT_PORT;
  if (!port) port = SERVER_DEFAULT_PORT;
  return host.length() > 0;
}

bool viaMdns() {
  if (!g_mdns) g_mdns = MDNS.begin((String("switchboard-viewport-") + wifi::shortId()).c_str());
  if (!g_mdns) return false;
  const int n = MDNS.queryService(SERVER_MDNS_NAME, "tcp");
  if (n > 0) {
    g_host = MDNS.IP(0).toString();
    g_port = MDNS.port(0);
    return true;
  }
  const IPAddress ip = MDNS.queryHost(SERVER_MDNS_NAME, 3000);
  if (ip == IPAddress(0, 0, 0, 0)) return false;
  g_host = ip.toString();
  g_port = SERVER_DEFAULT_PORT;
  return true;
}

bool find(bool fresh) {
  // An address typed in at setup always wins.
  if (parseAddress(store::get("server", ""), g_host, g_port)) return true;
  if (!fresh && parseAddress(store::get("srvcache", ""), g_host, g_port)) return true;
  if (!viaMdns()) return false;
  store::put("srvcache", address());
  return true;
}

void headers(HTTPClient& http) {
  const String t = token();
  if (t.length()) http.addHeader("Authorization", "Bearer " + t);
  if (g_health.battery >= 0) http.addHeader("X-Battery", String(g_health.battery));
  if (!isnan(g_health.temperature)) http.addHeader("X-Temperature", String(g_health.temperature, 1));
  if (!isnan(g_health.humidity)) http.addHeader("X-Humidity", String(g_health.humidity, 0));
  if (!isnan(g_health.voltage)) http.addHeader("X-Voltage", String(g_health.voltage, 2));
  if (WiFi.status() == WL_CONNECTED) http.addHeader("X-RSSI", String(WiFi.RSSI()));
  http.addHeader("X-Firmware", FIRMWARE_VERSION);
  http.addHeader("X-Board", SWITCHBOARD_BOARD);
}

Response request(const String& path, const char* method, const String* body, const char* ifNoneMatch, bool asBytes,
                 uint32_t timeoutMs) {
  Response r;
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (!g_host.length() && !find(attempt == 1)) {
      r.code = -100;
      return r;
    }
    HTTPClient http;
    http.setTimeout(timeoutMs);
    http.setConnectTimeout(4000);
    if (!http.begin(base() + path)) {
      r.code = -101;
      return r;
    }
    headers(http);
    if (ifNoneMatch && *ifNoneMatch) http.addHeader("If-None-Match", ifNoneMatch);
    if (body) http.addHeader("Content-Type", "application/json");
    const char* keep[] = {"ETag", "X-Refresh-In", "X-Quiet"};
    http.collectHeaders(keep, 3);
    r.code = body ? http.sendRequest(method, *body) : http.GET();
    if (r.code < 0 && attempt == 0) {
      // Not there any more: find it again (unless it was typed in).
      http.end();
      LOGF("%s: %s, looking for the server again\n", path.c_str(), HTTPClient::errorToString(r.code).c_str());
      g_host = "";
      store::remove("srvcache");
      continue;
    }
    r.etag = http.header("ETag");
    r.refreshIn = static_cast<uint32_t>(http.header("X-Refresh-In").toInt());
    r.quiet = http.header("X-Quiet") == "1";
    if (r.code >= 200 && r.code < 300) {
      if (asBytes) {
        const int size = http.getSize();
        WiFiClient* in = http.getStreamPtr();
        if (size > 0) {
          r.bytes.resize(size);
          size_t got = 0;
          const unsigned long start = millis();
          while (got < static_cast<size_t>(size) && millis() - start < timeoutMs) {
            const size_t n = in->readBytes(r.bytes.data() + got, size - got);
            got += n;
            if (!n) delay(2);
          }
          if (got != static_cast<size_t>(size)) {
            r.bytes.clear();
            r.code = -102;
          }
        }
      } else {
        r.body = http.getString();
      }
    }
    http.end();
    return r;
  }
  return r;
}
}  // namespace

void setHealth(const Health& h) { g_health = h; }

bool resolve() {
  if (g_host.length()) return true;
  return find(false);
}

String address() { return g_host + ":" + String(g_port); }
String base() { return "http://" + address(); }

String token() { return store::get("token", ""); }
void setToken(const String& t) { store::put("token", t); }
void forgetToken() { store::remove("token"); }

Pairing pair(String& status) {
  JsonDocument body;
  body["mac"] = wifi::mac();
  body["type"] = "viewport";
  String json;
  serializeJson(body, json);
  // Registering needs no token (and a stale one mustn't be sent).
  const String had = token();
  if (had.length()) forgetToken();
  const Response r = request("/api/pairing/register", "POST", &json, nullptr, false, 8000);
  if (r.code < 200 || r.code >= 300) {
    if (had.length()) setToken(had);
    status = r.code < 0 ? "Can't reach the server" : "The server answered " + String(r.code);
    return Pairing::Unreachable;
  }
  JsonDocument doc;
  deserializeJson(doc, r.body);
  const String st = doc["status"] | "pending";
  if (st == "approved") {
    const String t = doc["token"] | "";
    if (t.length()) setToken(t);
    status = "Approved";
    return Pairing::Approved;
  }
  status = st == "revoked" ? "Removed on the server: approve it again" : "Waiting for approval";
  return st == "revoked" ? Pairing::Revoked : Pairing::Pending;
}

Response get(const String& path, const char* ifNoneMatch, bool asBytes, uint32_t timeoutMs) {
  return request(path, "GET", nullptr, ifNoneMatch, asBytes, timeoutMs);
}

Response post(const String& path, const String& json) { return request(path, "POST", &json, nullptr, false, 10000); }

String urlEncode(const String& s) {
  static const char* hex = "0123456789ABCDEF";
  String o;
  for (size_t i = 0; i < s.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(s[i]);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      o += static_cast<char>(c);
    } else {
      o += '%';
      o += hex[c >> 4];
      o += hex[c & 15];
    }
  }
  return o;
}

}  // namespace server
