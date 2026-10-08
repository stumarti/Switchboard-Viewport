// Renders Switchboard Server states (test/fixtures/*.json, captured from the
// demo) as the panel would draw them: test/host/out/<name>.ppm.
//
//   test/host/run.sh renders them all (and converts them to PNG).
#include <stdio.h>
#include <string>
#include <ArduinoJson.h>
#include "canvas.h"
#include "render/icons.h"
#include "render/screens.h"

static std::string slurp(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) return "";
  std::string s;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
  fclose(f);
  return s;
}

// The device's cache of fetched icons, on the host: test/fixtures/icons
// (tools/gen_icons.js --fixtures), in /api/icons/mdi's format.
#include <map>
static std::map<std::string, std::string> g_icons;
static bool fixtureIcon(const char* name, int size, draw::Icon& out) {
  const std::string key = std::string(name) + "_" + std::to_string(size);
  auto it = g_icons.find(key);
  if (it == g_icons.end()) {
    std::string b = slurp(("test/fixtures/icons/" + key + ".icon").c_str());
    if (b.size() < 6) return false;
    it = g_icons.emplace(key, b).first;
  }
  const uint8_t* p = reinterpret_cast<const uint8_t*>(it->second.data());
  out.w = p[0] | (p[1] << 8);
  out.h = p[2] | (p[3] << 8);
  out.bits = p + 6;
  out.fourBit = false;
  return true;
}

// Pictures (album art, photos) as the device keeps them: the server's
// spectra bitmaps, in test/fixtures/pictures/<FNV-1a of the src>_<w>x<h>.bin
// (the device's own cache name, app/theme.cpp).
static std::map<std::string, std::string> g_pictures;
static const uint8_t* fixturePicture(const char* src, int w, int h) {
  uint32_t hash = 2166136261u;
  for (const char* p = src; *p; ++p) hash = (hash ^ static_cast<uint8_t>(*p)) * 16777619u;
  char key[64];
  snprintf(key, sizeof(key), "%08x_%dx%d", static_cast<unsigned>(hash), w, h);
  auto it = g_pictures.find(key);
  if (it == g_pictures.end()) {
    std::string b = slurp((std::string("test/fixtures/pictures/") + key + ".bin").c_str());
    if (b.size() < static_cast<size_t>((w + 1) / 2) * h) return nullptr;
    it = g_pictures.emplace(key, b).first;
  }
  return reinterpret_cast<const uint8_t*>(it->second.data());
}

int main(int argc, char** argv) {
  icons::setLookups(nullptr, fixtureIcon);
  icons::setPictureLookup(fixturePicture);
  if (argc < 3) {
    fprintf(stderr, "usage: render <out-dir> fixture.json...\n");
    return 2;
  }
  int failed = 0;
  for (int i = 2; i < argc; ++i) {
    JsonDocument doc;
    if (deserializeJson(doc, slurp(argv[i]))) {
      fprintf(stderr, "%s: bad JSON\n", argv[i]);
      ++failed;
      continue;
    }
    screens::Ctx ctx;
    snprintf(ctx.time, sizeof(ctx.time), "11:04");
    ctx.battPct = 76;
    ctx.quiet = doc["quiet"] | false;
    // The kitchen panel's carousel, on its first screen.
    static const char* MARKS[] = {"view-dashboard-outline", "radiator", "shield-home-outline"};
    for (int k = 0; k < 3; ++k) ctx.marks[k] = MARKS[k];
    ctx.markCount = 3;
    ctx.current = doc["current"] | 0;
    // A layout that hangs portrait (rotation 90 or 270): 480x800.
    ctx.portrait = doc["portrait"] | false;
    JsonObjectConst screen = doc["data"];

    // The collecting pass: which icons would have to be fetched.
    screens::NullCanvas none;
    icons::beginCollect();
    screens::drawScreen(none, screen, ctx);
    icons::endCollect();

    Canvas c(ctx.portrait ? 480 : 800, ctx.portrait ? 800 : 480);
    screens::drawScreen(c, screen, ctx);
    std::string name = argv[i];
    name = name.substr(name.find_last_of('/') + 1);
    name = name.substr(0, name.size() - 5);
    const std::string out = std::string(argv[1]) + "/" + name + ".ppm";
    c.writePpm(out.c_str());
    printf("%-36s fetch:", name.c_str());
    for (int k = 0; k < icons::needCount(); ++k) printf(" %s@%d", icons::need(k).name, icons::need(k).size);
    for (int k = 0; k < icons::pictureNeedCount(); ++k) printf(" [picture %s %dx%d]", icons::pictureNeed(k).src, icons::pictureNeed(k).w, icons::pictureNeed(k).h);
    printf("\n");
  }
  return failed ? 1 : 0;
}
