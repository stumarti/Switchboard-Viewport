// The panel, on the host: 800x480 (or 480x800, portrait) palette indices, written out as an image
// in the colours the server's preview uses.
#pragma once
#include <stdio.h>
#include <vector>
#include "Adafruit_GFX.h"
#include "render/colors.h"

class Canvas : public Adafruit_GFX {
 public:
  explicit Canvas(int w = 800, int h = 480) : Adafruit_GFX(w, h), W(w), H(h), px(w * h, 0) {}
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    px[y * W + x] = index(c);
  }
  static uint8_t index(uint16_t c) {
    switch (c) {
      case GxEPD_WHITE: return 0;
      case GxEPD_BLACK: return 1;
      case GxEPD_RED: return 2;
      case GxEPD_YELLOW: return 3;
      case GxEPD_GREEN: return 4;
      case GxEPD_BLUE: return 5;
      default: return 1;
    }
  }
  bool writePpm(const char* path) const {
    static const uint8_t RGB[6][3] = {{255, 255, 255}, {17, 17, 17}, {198, 40, 40}, {224, 180, 0}, {46, 125, 50}, {21, 101, 192}};
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (uint8_t i : px) fwrite(RGB[i < 6 ? i : 1], 1, 3, f);
    fclose(f);
    return true;
  }
  int W, H;
  std::vector<uint8_t> px;
};
