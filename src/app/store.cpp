#include "app/store.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>

#include "config.h"
#include "log.h"

namespace store {

namespace {
const char* NS = "sbview";
bool g_fs = false;

void ensureDirs(const char* path) {
  // LittleFS on this core creates files only in existing folders.
  String p(path);
  for (int i = 1; i < static_cast<int>(p.length()); ++i) {
    if (p[i] != '/') continue;
    const String dir = p.substring(0, i);
    if (!LittleFS.exists(dir)) LittleFS.mkdir(dir);
  }
}

std::vector<Network> parse(const String& json) {
  std::vector<Network> out;
  JsonDocument doc;
  if (deserializeJson(doc, json)) return out;
  for (JsonObjectConst n : doc.as<JsonArrayConst>()) {
    Network w{n["s"] | "", n["p"] | ""};
    if (w.ssid.length()) out.push_back(w);
  }
  return out;
}

String serialize(const std::vector<Network>& list) {
  JsonDocument doc;
  JsonArray a = doc.to<JsonArray>();
  for (const Network& n : list) {
    JsonObject o = a.add<JsonObject>();
    o["s"] = n.ssid;
    o["p"] = n.password;
  }
  String s;
  serializeJson(doc, s);
  return s;
}
}  // namespace

bool begin() {
  g_fs = LittleFS.begin(true);
  if (!g_fs) LOGF("LittleFS didn't mount: no cache this wake\n");
  return g_fs;
}

String get(const char* key, const char* fallback) {
  Preferences p;
  if (!p.begin(NS, true)) return fallback;
  const String v = p.getString(key, fallback);
  p.end();
  return v;
}

void put(const char* key, const String& value) {
  Preferences p;
  if (!p.begin(NS, false)) return;
  p.putString(key, value);
  p.end();
}

int32_t getInt(const char* key, int32_t fallback) {
  Preferences p;
  if (!p.begin(NS, true)) return fallback;
  const int32_t v = p.getInt(key, fallback);
  p.end();
  return v;
}

void putInt(const char* key, int32_t value) {
  Preferences p;
  if (!p.begin(NS, false)) return;
  p.putInt(key, value);
  p.end();
}

void remove(const char* key) {
  Preferences p;
  if (!p.begin(NS, false)) return;
  p.remove(key);
  p.end();
}

bool exists(const char* path) { return g_fs && LittleFS.exists(path); }

bool read(const char* path, std::vector<uint8_t>& out) {
  out.clear();
  if (!g_fs || !LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  out.resize(f.size());
  const size_t n = out.empty() ? 0 : f.read(out.data(), out.size());
  f.close();
  if (n != out.size()) {
    out.clear();
    return false;
  }
  return true;
}

String readText(const char* path) {
  std::vector<uint8_t> b;
  if (!read(path, b)) return "";
  String s;
  s.reserve(b.size());
  for (uint8_t c : b) s += static_cast<char>(c);
  return s;
}

bool write(const char* path, const uint8_t* data, size_t len) {
  if (!g_fs) return false;
  ensureDirs(path);
  const String tmp = String(path) + ".tmp";
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  const size_t n = f.write(data, len);
  f.close();
  if (n != len) {
    LittleFS.remove(tmp);
    return false;
  }
  LittleFS.remove(path);
  return LittleFS.rename(tmp, path);
}

bool writeText(const char* path, const String& text) {
  return write(path, reinterpret_cast<const uint8_t*>(text.c_str()), text.length());
}

void erase(const char* path) {
  if (g_fs && LittleFS.exists(path)) LittleFS.remove(path);
}

size_t freeBytes() { return g_fs ? LittleFS.totalBytes() - LittleFS.usedBytes() : 0; }

String safeName(const char* s) {
  String out;
  for (const char* p = s; p && *p && out.length() < 64; ++p) {
    const char c = *p;
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    out += ok ? c : '_';
  }
  return out.length() ? out : String("_");
}

std::vector<Network> networks() {
  std::vector<Network> out = parse(get("wifi", "[]"));
  for (const Network& n : parse(get("swifi", "[]"))) {
    bool dup = false;
    for (const Network& m : out) dup = dup || m.ssid == n.ssid;
    if (!dup) out.push_back(n);
  }
  return out;
}

void addNetwork(const String& ssid, const String& password) {
  std::vector<Network> list = parse(get("wifi", "[]"));
  std::vector<Network> next{{ssid, password}};
  for (const Network& n : list)
    if (n.ssid != ssid && next.size() < WIFI_MAX_NETWORKS) next.push_back(n);
  put("wifi", serialize(next));
}

void setServerNetworks(const std::vector<Network>& list) {
  std::vector<Network> keep(list.begin(), list.begin() + (list.size() < WIFI_MAX_NETWORKS ? list.size() : WIFI_MAX_NETWORKS));
  const String s = serialize(keep);
  if (s != get("swifi", "[]")) put("swifi", s);
}

}  // namespace store
