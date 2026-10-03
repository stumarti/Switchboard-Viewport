#include "app/theme.h"

#include <ArduinoJson.h>
#include <map>
#include <string>
#include <vector>

#include "app/store.h"
#include "log.h"
#include "net/server.h"
#include "render/draw.h"
#include "render/icons.h"

namespace theme {

namespace {

const char* ICONS_PACK = "/theme/icons.pack";
const char* FONTS_PACK = "/theme/fonts.pack";

uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
int16_t i16(const uint8_t* p) { return static_cast<int16_t>(u16(p)); }
uint32_t u32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }

// --- Icon pack (SBI1) ------------------------------------------------------------
// header: "SBI1", count:u16, reserved:u16
// entry:  name[24], w:u16, h:u16, opticalCenterY:i16, bitsOffset:u32, bitsLength:u32
std::vector<uint8_t> g_iconPack;
std::map<std::string, draw::Icon> g_slots;

void parseIcons() {
  g_slots.clear();
  const std::vector<uint8_t>& b = g_iconPack;
  if (b.size() < 8 || memcmp(b.data(), "SBI1", 4) != 0) return;
  const uint16_t n = u16(&b[4]);
  for (uint16_t i = 0; i < n; ++i) {
    const size_t e = 8 + static_cast<size_t>(i) * 38;
    if (e + 38 > b.size()) break;
    char name[25];
    memcpy(name, &b[e], 24);
    name[24] = 0;
    draw::Icon ic;
    ic.w = u16(&b[e + 24]);
    ic.h = u16(&b[e + 26]);
    const uint32_t off = u32(&b[e + 30]), len = u32(&b[e + 34]);
    if (off + len > b.size() || len < static_cast<uint32_t>((ic.w + 7) / 8) * ic.h) continue;
    ic.bits = &b[off];
    ic.fourBit = false;
    g_slots[name] = ic;
  }
  LOGF("Icon pack: %u icons\n", static_cast<unsigned>(g_slots.size()));
}

// --- Font pack (SBF1) ------------------------------------------------------------
// header: "SBF1", faces:u8, reserved[3]
// face:   slot[16], first:u16, last:u16, yAdvance:u8, ascent:u8, maxW:u8,
//         maxH:u8, bpp:u8, glyphsOffset:u32, glyphsCount:u16,
//         bitmapOffset:u32, bitmapLength:u32
// glyph:  bitmapOffset:u16, width:u8, height:u8, xAdvance:u8, xOffset:i8, yOffset:i8
std::vector<uint8_t> g_fontPack;
std::vector<GFXglyph> g_glyphs[draw::FACE_COUNT];
GFXfont g_fonts[draw::FACE_COUNT];

void parseFonts() {
  for (int f = 0; f < draw::FACE_COUNT; ++f) draw::setFont(static_cast<draw::Face>(f), nullptr);
  const std::vector<uint8_t>& b = g_fontPack;
  if (b.size() < 8 || memcmp(b.data(), "SBF1", 4) != 0) return;
  const uint8_t n = b[4];
  int loaded = 0;
  for (uint8_t i = 0; i < n; ++i) {
    const size_t e = 8 + static_cast<size_t>(i) * 39;
    if (e + 39 > b.size()) break;
    char slot[17];
    memcpy(slot, &b[e], 16);
    slot[16] = 0;
    draw::Face face;
    if (!strcmp(slot, "regular18")) face = draw::REG18;
    else if (!strcmp(slot, "bold18")) face = draw::BOLD18;
    else if (!strcmp(slot, "bold24")) face = draw::BOLD24;
    else if (!strcmp(slot, "bold82")) face = draw::BOLD82;
    else continue;
    const uint16_t first = u16(&b[e + 16]), last = u16(&b[e + 18]);
    const uint8_t yAdvance = b[e + 20], bpp = b[e + 24];
    const uint32_t gOff = u32(&b[e + 25]);
    const uint16_t gCount = u16(&b[e + 29]);
    const uint32_t bmOff = u32(&b[e + 31]), bmLen = u32(&b[e + 35]);
    if (bpp != 1 || gCount != last - first + 1 || gOff + gCount * 7u > b.size() || bmOff + bmLen > b.size()) continue;
    std::vector<GFXglyph>& gl = g_glyphs[face];
    gl.resize(gCount);
    for (uint16_t k = 0; k < gCount; ++k) {
      const uint8_t* p = &b[gOff + k * 7u];
      gl[k] = GFXglyph{u16(p), p[2], p[3], p[4], static_cast<int8_t>(p[5]), static_cast<int8_t>(p[6])};
    }
    g_fonts[face] = GFXfont{const_cast<uint8_t*>(&b[bmOff]), gl.data(), first, last, yAdvance};
    draw::setFont(face, &g_fonts[face]);
    ++loaded;
  }
  LOGF("Font pack: %d faces\n", loaded);
}

// --- Layout icons and pictures ---------------------------------------------------
struct Blob {
  std::vector<uint8_t> bytes;
  draw::Icon icon;
};
std::map<std::string, Blob> g_named;
std::map<std::string, std::vector<uint8_t>> g_pictures;

std::string namedKey(const char* name, int size) { return std::string(name) + "_" + std::to_string(size); }
String namedPath(const char* name, int size) { return "/mdi/" + store::safeName(name) + "_" + String(size) + ".icon"; }

// {w:u16, h:u16, opticalCenterY:i16} then the 1-bit rows.
bool parseSingle(Blob& b) {
  if (b.bytes.size() < 6) return false;
  b.icon.w = u16(&b.bytes[0]);
  b.icon.h = u16(&b.bytes[2]);
  if (static_cast<size_t>((b.icon.w + 7) / 8) * b.icon.h + 6 > b.bytes.size()) return false;
  b.icon.bits = &b.bytes[6];
  b.icon.fourBit = false;
  return true;
}

bool packLookup(const char* key, draw::Icon& out) {
  auto it = g_slots.find(key);
  if (it == g_slots.end()) return false;
  out = it->second;
  return true;
}

bool namedLookup(const char* name, int size, draw::Icon& out) {
  const std::string k = namedKey(name, size);
  auto it = g_named.find(k);
  if (it == g_named.end()) {
    // On flash from an earlier wake: load it (no network here).
    Blob b;
    if (!store::read(namedPath(name, size).c_str(), b.bytes) || !parseSingle(b)) return false;
    it = g_named.emplace(k, std::move(b)).first;
    parseSingle(it->second);  // the bytes moved: point at their new home
  }
  out = it->second.icon;
  return true;
}

String pictureKey(const char* src, int w, int h) {
  // A short stable name for the cache: FNV-1a of the source.
  uint32_t hash = 2166136261u;
  for (const char* p = src; *p; ++p) hash = (hash ^ static_cast<uint8_t>(*p)) * 16777619u;
  char buf[40];
  snprintf(buf, sizeof(buf), "/art/%08x_%dx%d.bin", static_cast<unsigned>(hash), w, h);
  return buf;
}

const uint8_t* pictureLookup(const char* src, int w, int h) {
  const String key = pictureKey(src, w, h);
  auto it = g_pictures.find(key.c_str());
  if (it == g_pictures.end()) {
    std::vector<uint8_t> b;
    if (!store::read(key.c_str(), b) || b.size() < static_cast<size_t>((w + 1) / 2) * h) return nullptr;
    it = g_pictures.emplace(key.c_str(), std::move(b)).first;
  }
  return it->second.data();
}

bool download(const String& path, const char* file, const char* versionKey, const String& version) {
  if (!version.length()) {
    // Back to the built-in look.
    store::erase(file);
    store::remove(versionKey);
    return true;
  }
  server::Response r = server::get(path, nullptr, true, 30000);
  if (r.code != 200 || r.bytes.empty()) {
    LOGF("%s: HTTP %d\n", path.c_str(), r.code);
    return false;
  }
  if (!store::write(file, r.bytes.data(), r.bytes.size())) return false;
  store::put(versionKey, version);
  LOGF("%s %s: %u bytes\n", file, version.c_str(), static_cast<unsigned>(r.bytes.size()));
  return true;
}

}  // namespace

void load() {
  store::read(ICONS_PACK, g_iconPack);
  parseIcons();
  store::read(FONTS_PACK, g_fontPack);
  parseFonts();
  icons::setLookups(packLookup, namedLookup);
  icons::setPictureLookup(pictureLookup);
}

void sync() {
  server::Response r = server::get("/api/theme");
  if (r.code != 200) return;
  JsonDocument doc;
  if (deserializeJson(doc, r.body)) return;
  const String iv = doc["iconsVersion"] | "", fv = doc["fontsVersion"] | "";
  bool changed = false;
  if (iv != store::get("iconsVer", "") || (iv.length() && !store::exists(ICONS_PACK)))
    changed |= download("/api/theme/icons.pack", ICONS_PACK, "iconsVer", iv);
  if (fv != store::get("fontsVer", "") || (fv.length() && !store::exists(FONTS_PACK)))
    changed |= download("/api/theme/fonts.pack", FONTS_PACK, "fontsVer", fv);
  if (changed) load();
}

void fetchNeeds() {
  for (int i = 0; i < icons::needCount(); ++i) {
    const icons::Need& n = icons::need(i);
    server::Response r = server::get("/api/icons/mdi/" + server::urlEncode(n.name) + "?size=" + String(n.size), nullptr, true);
    if (r.code != 200) {
      LOGF("icon %s@%d: HTTP %d\n", n.name, n.size, r.code);
      continue;
    }
    Blob b;
    b.bytes = std::move(r.bytes);
    if (!parseSingle(b)) continue;
    store::write(namedPath(n.name, n.size).c_str(), b.bytes.data(), b.bytes.size());
    auto it = g_named.insert_or_assign(namedKey(n.name, n.size), std::move(b)).first;
    parseSingle(it->second);
  }
  for (int i = 0; i < icons::pictureNeedCount(); ++i) {
    const icons::PictureNeed& p = icons::pictureNeed(i);
    server::Response r = server::get("/api/art?src=" + server::urlEncode(p.src) + "&w=" + String(p.w) + "&h=" + String(p.h) + "&fmt=spectra",
                                     nullptr, true, 20000);
    if (r.code != 200 || r.bytes.size() < static_cast<size_t>((p.w + 1) / 2) * p.h) continue;
    const String key = pictureKey(p.src, p.w, p.h);
    store::write(key.c_str(), r.bytes.data(), r.bytes.size());
    g_pictures[key.c_str()] = std::move(r.bytes);
  }
}

}  // namespace theme
