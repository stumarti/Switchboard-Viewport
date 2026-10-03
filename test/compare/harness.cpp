// The kitchen panel's own firmware against Switchboard Server + this
// firmware, on one captured Home Assistant (house.json), pixel for pixel.
//
//   test/compare/compare.sh
//
// Left: the panel's main.cpp fetching from Home Assistant (answered from
// house.json) and its screen_*.cpp drawing. Right: server-<screen>.json
// (what Switchboard Server computed from the same house) drawn by
// src/render. Both onto the same canvas type; every differing pixel counted
// and marked in diff-<screen>.ppm.
#include <stdio.h>
#include <functional>
#include <string>

#include "HTTPClient.h"
#include "WiFi.h"
#include "Wire.h"
#include "shared_state.h"  // the panel's: its display and draw functions
#include "render/screens.h"
#include "render/system.h"

HostSerial Serial1;
HostWiFi WiFi;
HostWire Wire;
time_t g_now = 0;
int g_batteryMilliVolts = 1950;  // a 3.90 V battery

bool fetchAllAndCompare();
void fetchHeatingPage();

static JsonDocument g_house;

static std::string slurp(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) return "";
  std::string s;
  char b[8192];
  size_t n;
  while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n);
  fclose(f);
  return s;
}

// --- The captured Home Assistant -------------------------------------------------
static time_t isoTime(const char* s) {
  struct tm t = {};
  int y, mo, d, h = 0, mi = 0, se = 0;
  if (sscanf(s, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) < 3) return 0;
  t.tm_year = y - 1900;
  t.tm_mon = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = mi;
  t.tm_sec = se;
  time_t utc = timegm(&t);
  // An explicit offset ("+01:00"); none = local (all-day dates).
  const char* tz = strlen(s) > 19 ? s + 19 : "";
  while (*tz == '.' || (*tz >= '0' && *tz <= '9')) ++tz;
  if (*tz == '+' || *tz == '-') {
    int oh = 0, om = 0;
    sscanf(tz + 1, "%d:%d", &oh, &om);
    utc -= (*tz == '+' ? 1 : -1) * (oh * 3600 + om * 60);
  } else if (*tz != 'Z' && strlen(s) <= 10) {
    struct tm l = t;
    l.tm_isdst = -1;
    utc = mktime(&l);
  }
  return utc;
}

int HTTPClient::GET() {
  const std::string url = url_.std();
  const size_t api = url.find("/api/");
  const std::string path = url.substr(api);
  body_ = "";
  if (path.rfind("/api/states/", 0) == 0) {
    const std::string id = path.substr(12);
    for (JsonObjectConst st : g_house["states"].as<JsonArrayConst>()) {
      if (id == (st["entity_id"] | "")) {
        std::string out;
        serializeJson(st, out);
        body_ = out;
        return 200;
      }
    }
    return 404;
  }
  if (path.rfind("/api/calendars/", 0) == 0) {
    const size_t q = path.find('?');
    const std::string id = path.substr(15, q - 15);
    const std::string qs = path.substr(q + 1);
    const std::string start = qs.substr(6, qs.find('&') - 6), end = qs.substr(qs.find("end=") + 4);
    const time_t a = isoTime(start.c_str()), b = isoTime(end.c_str());
    JsonDocument out;
    JsonArray arr = out.to<JsonArray>();
    for (JsonObjectConst ev : g_house["calendars"][id].as<JsonArrayConst>()) {
      const char* s = ev["start"]["dateTime"] | (ev["start"]["date"] | "");
      const char* e = ev["end"]["dateTime"] | (ev["end"]["date"] | "");
      if (isoTime(s) <= b && isoTime(e) > a) arr.add(ev);
    }
    std::string text;
    serializeJson(out, text);
    body_ = text;
    return 200;
  }
  return 404;
}

int HTTPClient::POST(const String& body) {
  JsonDocument req;
  deserializeJson(req, body.c_str());
  const std::string entity = req["entity_id"] | "", type = req["type"] | "";
  JsonDocument out;
  out["service_response"][entity]["forecast"] = g_house["forecasts"][type][entity];
  std::string text;
  serializeJson(out, text);
  body_ = text;
  return 200;
}

// --- The comparison ----------------------------------------------------------------
static int compare(const char* name, const Canvas& panel, const std::string& outDir) {
  JsonDocument doc;
  deserializeJson(doc, slurp((std::string("test/compare/server-") + name + ".json").c_str()));
  screens::Ctx ctx;
  struct tm lt;
  localtime_r(&g_now, &lt);
  strftime(ctx.time, sizeof(ctx.time), "%H:%M", &lt);
  ctx.battPct = batteryPercent(readBatteryVoltage());
  ctx.markCount = 0;  // the panel had no carousel marks
  Canvas ours;
  screens::drawScreen(ours, doc["data"], ctx);

  Canvas diff;
  int n = 0;
  for (size_t i = 0; i < panel.px.size(); ++i) {
    const bool same = panel.px[i] == ours.px[i];
    diff.px[i] = same ? (panel.px[i] ? 1 : 0) : 2;
    if (!same) ++n;
  }
  panel.writePpm((outDir + "/panel-" + name + ".ppm").c_str());
  ours.writePpm((outDir + "/ours-" + name + ".ppm").c_str());
  diff.writePpm((outDir + "/diff-" + name + ".ppm").c_str());
  printf("%-9s %6d pixels differ\n", name, n);
  return n;
}

int main(int argc, char** argv) {
  const std::string out = argc > 1 ? argv[1] : "test/compare/out";
  if (deserializeJson(g_house, slurp("test/compare/house.json"))) {
    fprintf(stderr, "house.json missing or bad\n");
    return 2;
  }
  setenv("TZ", TZ_INFO, 1);
  tzset();
  g_now = isoTime(g_house["now"] | "");

  fetchAllAndCompare();
  drawDashboard();
  int total = compare("status", display, out);
  fetchHeatingPage();
  drawHeatingPage();
  total += compare("heating", display, out);
  drawSecurityPage();
  total += compare("security", display, out);

  // The error and charge screens the firmware keeps as they were.
  auto same = [&](const char* name, const std::function<void(Canvas&)>& ours) {
    Canvas c;
    ours(c);
    int n = 0;
    Canvas diff;
    for (size_t i = 0; i < c.px.size(); ++i) {
      const bool eq = c.px[i] == display.px[i];
      diff.px[i] = eq ? (c.px[i] ? 1 : 0) : 2;
      n += !eq;
    }
    display.writePpm((out + "/panel-" + name + ".ppm").c_str());
    c.writePpm((out + "/ours-" + name + ".ppm").c_str());
    diff.writePpm((out + "/diff-" + name + ".ppm").c_str());
    printf("%-9s %6d pixels differ\n", name, n);
    return n;
  };
  char when[40];
  struct tm lt;
  localtime_r(&g_now, &lt);
  strftime(when, sizeof(when), "%a %d %b %Y  %H:%M", &lt);
  drawErrorScreen("No WiFi connection", "Check network or router");
  total += same("error", [&](Canvas& c) { sys::error(c, "vx_wifi_off", "No WiFi connection", "Check network or router", when); });
  drawErrorScreen("Can't reach Home Assistant", "No response from Home Assistant");
  total += same("error-ha", [&](Canvas& c) { sys::error(c, "vx_cloud_off", "Can't reach Home Assistant", "No response from Home Assistant", when); });
  drawChargeScreen(2);
  total += same("charge", [&](Canvas& c) { sys::charge(c, 2); });
  return total ? 1 : 0;
}
