#include "net/wifi.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_random.h>

#include "app/display.h"
#include "app/hw.h"
#include "app/store.h"
#include "config.h"
#include "log.h"
#include "render/system.h"

namespace wifi {

String mac() {
  String m = WiFi.macAddress();
  m.toLowerCase();
  return m;
}

String shortId() {
  String m = WiFi.macAddress();
  m.replace(":", "");
  return m.substring(m.length() - 4);
}

String ssid() { return WiFi.SSID(); }
int rssi() { return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0; }

void off() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

static bool join(const store::Network& n) {
  LOGF("Joining %s\n", n.ssid.c_str());
  WiFi.begin(n.ssid.c_str(), n.password.c_str());
  const unsigned long start = millis();
  bool blink = false;
  while (WiFi.status() != WL_CONNECTED) {
    blink = !blink;
    hw::led(blink);
    delay(100);
    if (millis() - start > WIFI_JOIN_TIMEOUT_MS) {
      hw::led(false);
      WiFi.disconnect();
      return false;
    }
  }
  hw::led(false);
  LOGF("Joined %s, IP %s\n", n.ssid.c_str(), WiFi.localIP().toString().c_str());
  return true;
}

bool connect() {
  std::vector<store::Network> saved = store::networks();
  if (saved.empty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setHostname((String("switchboard-viewport-") + shortId()).c_str());
  // One saved network: just join it (no scan, as the panel did). Several:
  // try the ones in range first, strongest first.
  if (saved.size() > 1) {
    const int found = WiFi.scanNetworks();
    std::vector<store::Network> inRange, rest;
    std::vector<int> strength;
    for (const store::Network& n : saved) {
      int best = -1000;
      for (int i = 0; i < found; ++i)
        if (WiFi.SSID(i) == n.ssid && WiFi.RSSI(i) > best) best = WiFi.RSSI(i);
      if (best > -1000) {
        size_t at = 0;
        while (at < strength.size() && strength[at] >= best) ++at;
        inRange.insert(inRange.begin() + at, n);
        strength.insert(strength.begin() + at, best);
      } else {
        rest.push_back(n);
      }
    }
    WiFi.scanDelete();
    saved = inRange;
    saved.insert(saved.end(), rest.begin(), rest.end());
  }
  for (const store::Network& n : saved) {
    if (join(n)) {
      store::putInt("joined", 1);
      return true;
    }
  }
  return false;
}

// --- Setup ---------------------------------------------------------------------

namespace {

WebServer* g_web = nullptr;
DNSServer* g_dns = nullptr;
bool g_saved = false;
String g_list;  // the networks it can see, as <option>s

String escape(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); ++i) {
    const char c = s[i];
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

void scan() {
  const int n = WiFi.scanNetworks();
  g_list = "";
  for (int i = 0; i < n; ++i) {
    const String s = WiFi.SSID(i);
    if (!s.length() || g_list.indexOf(">" + escape(s) + " (") >= 0) continue;
    g_list += "<option value=\"" + escape(s) + "\">" + escape(s) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
  }
  WiFi.scanDelete();
}

const char* PAGE_HEAD =
    "<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Switchboard Viewport</title><style>"
    "body{font-family:system-ui,sans-serif;margin:0;background:#f4f4f4;color:#111}"
    "main{max-width:420px;margin:0 auto;padding:24px}h1{font-size:22px;letter-spacing:.08em}"
    "label{display:block;margin:16px 0 6px;font-weight:600}select,input{width:100%;box-sizing:border-box;font-size:17px;padding:10px;"
    "border:1px solid #999;border-radius:8px;background:#fff}button{margin-top:22px;width:100%;font-size:18px;padding:12px;"
    "border:0;border-radius:8px;background:#111;color:#fff}p{color:#444}small{color:#666}</style></head><body><main>"
    "<h1>SWITCHBOARD</h1>";

void page() {
  String html = PAGE_HEAD;
  html += "<p>Choose the Wi-Fi network this display should join.</p><form method=post action=/save>";
  html += "<label>Network</label><select name=pick><option value=''>Type one below</option>" + g_list + "</select>";
  html += "<label>Or its name</label><input name=ssid autocomplete=off>";
  html += "<label>Password</label><input name=pass type=password>";
  html += "<label>Switchboard Server (optional)</label><input name=server placeholder='found automatically' value='" +
          escape(store::get("server", "")) + "'>";
  html += "<small>Only if it isn't found on your network: its address and port, e.g. 192.168.1.20:45678</small>";
  html += "<button>Save and connect</button></form><p><a href=/rescan>Look for networks again</a></p></main></body></html>";
  g_web->send(200, "text/html", html);
}

void save() {
  String ssid = g_web->arg("ssid");
  ssid.trim();
  if (!ssid.length()) ssid = g_web->arg("pick");
  if (!ssid.length()) {
    g_web->send(400, "text/html", String(PAGE_HEAD) + "<p>Choose a network or type its name.</p><p><a href=/>Back</a></p></main></body></html>");
    return;
  }
  store::addNetwork(ssid, g_web->arg("pass"));
  String server = g_web->arg("server");
  server.trim();
  if (server.length()) store::put("server", server);
  else store::remove("server");
  g_web->send(200, "text/html",
              String(PAGE_HEAD) + "<p>Saved. The display is joining <b>" + escape(ssid) +
                  "</b> now and will show its screens in a minute.</p><p>If it can't join it, it opens this setup again.</p></main></body></html>");
  g_saved = true;
}

String password() {
  // Eight characters, no look-alikes.
  static const char A[] = "abcdefghjkmnpqrstuvwxyz23456789";
  String p;
  for (int i = 0; i < 8; ++i) p += A[esp_random() % (sizeof(A) - 1)];
  return p;
}

}  // namespace

void setup(const char* reason) {
  const String apName = String(SETUP_AP_PREFIX) + shortId();
  const String apPass = password();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName.c_str(), apPass.c_str());
  delay(200);
  const IPAddress ip = WiFi.softAPIP();
  const String url = "http://" + ip.toString();
  LOGF("Setup: %s (%s) at %s\n", apName.c_str(), apPass.c_str(), url.c_str());
  scan();

  display::show([&](draw::Gfx& g) { sys::setup(g, {apName.c_str(), apPass.c_str(), url.c_str(), reason}); });

  DNSServer dns;
  WebServer web(80);
  g_dns = &dns;
  g_web = &web;
  g_saved = false;
  dns.start(53, "*", ip);
  web.on("/", HTTP_GET, page);
  web.on("/save", HTTP_POST, save);
  web.on("/rescan", HTTP_GET, [] {
    scan();
    g_web->sendHeader("Location", "/");
    g_web->send(302, "text/plain", "");
  });
  // Every other address (a phone checking for a captive portal) lands here.
  web.onNotFound([url] {
    g_web->sendHeader("Location", url + "/");
    g_web->send(302, "text/plain", "");
  });
  web.begin();

  const unsigned long start = millis();
  unsigned long savedAt = 0;
  while (millis() - start < SETUP_TIMEOUT_MS) {
    dns.processNextRequest();
    web.handleClient();
    if (g_saved && !savedAt) savedAt = millis();
    // Let the "Saved" page reach the phone, then start over on the new network.
    if (savedAt && millis() - savedAt > 1500) {
      web.stop();
      dns.stop();
      ESP.restart();
    }
    // The green button (the one that started it) gives up on setup.
    if (millis() - start > 3000 && digitalRead(BTN_KEY0) == LOW) break;
    delay(5);
  }
  web.stop();
  dns.stop();
  g_web = nullptr;
  g_dns = nullptr;
  WiFi.softAPdisconnect(true);
  off();
}

}  // namespace wifi
