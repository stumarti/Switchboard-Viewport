#include "Adafruit_GFX.h"

#include <stdlib.h>

void Adafruit_GFX::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c) {
  int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
  for (;;) {
    drawPixel(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    const int16_t e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void Adafruit_GFX::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
  int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
  drawPixel(x0, y0 + r, color);
  drawPixel(x0, y0 - r, color);
  drawPixel(x0 + r, y0, color);
  drawPixel(x0 - r, y0, color);
  while (x < y) {
    if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
    x++;
    ddF_x += 2;
    f += ddF_x;
    drawPixel(x0 + x, y0 + y, color);
    drawPixel(x0 - x, y0 + y, color);
    drawPixel(x0 + x, y0 - y, color);
    drawPixel(x0 - x, y0 - y, color);
    drawPixel(x0 + y, y0 + x, color);
    drawPixel(x0 - y, y0 + x, color);
    drawPixel(x0 + y, y0 - x, color);
    drawPixel(x0 - y, y0 - x, color);
  }
}

void Adafruit_GFX::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t c) {
  for (int16_t y = -r; y <= r; ++y)
    for (int16_t x = -r; x <= r; ++x)
      if (x * x + y * y <= r * r) drawPixel(x0 + x, y0 + y, c);
}

// As Adafruit GFX draws them.
void Adafruit_GFX::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c) {
  const int16_t maxR = ((w < h) ? w : h) / 2;
  if (r > maxR) r = maxR;
  fillRect(x + r, y, w - 2 * r, h, c);
  fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, c);
  fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, c);
}

void Adafruit_GFX::fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta, uint16_t c) {
  int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r, px = x, py = y;
  delta++;
  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
    if (x < (y + 1)) {
      if (corners & 1) drawFastVLine(x0 + x, y0 - y, 2 * y + delta, c);
      if (corners & 2) drawFastVLine(x0 - x, y0 - y, 2 * y + delta, c);
    }
    if (y != py) {
      if (corners & 1) drawFastVLine(x0 + py, y0 - px, 2 * px + delta, c);
      if (corners & 2) drawFastVLine(x0 - py, y0 - px, 2 * px + delta, c);
      py = y;
    }
    px = x;
  }
}

void Adafruit_GFX::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint8_t sx, uint8_t sy) {
  c -= gfxFont->first;
  const GFXglyph* glyph = &gfxFont->glyph[c];
  const uint8_t* bitmap = gfxFont->bitmap;
  uint16_t bo = glyph->bitmapOffset;
  const uint8_t w = glyph->width, h = glyph->height;
  const int8_t xo = glyph->xOffset, yo = glyph->yOffset;
  uint8_t bits = 0, bit = 0;
  for (uint8_t yy = 0; yy < h; yy++) {
    for (uint8_t xx = 0; xx < w; xx++) {
      if (!(bit++ & 7)) bits = bitmap[bo++];
      if (bits & 0x80) {
        if (sx == 1 && sy == 1) drawPixel(x + xo + xx, y + yo + yy, color);
        else fillRect(x + (xo + xx) * sx, y + (yo + yy) * sy, sx, sy, color);
      }
      bits <<= 1;
    }
  }
}

size_t Adafruit_GFX::write(uint8_t c) {
  if (!gfxFont) return 1;  // the firmware always sets a font
  if (c == '\n') {
    cursor_x = 0;
    cursor_y += textsize_y * gfxFont->yAdvance;
  } else if (c != '\r') {
    if (c >= gfxFont->first && c <= gfxFont->last) {
      const GFXglyph* glyph = &gfxFont->glyph[c - gfxFont->first];
      if (glyph->width > 0 && glyph->height > 0) drawChar(cursor_x, cursor_y, c, textcolor, textsize_x, textsize_y);
      cursor_x += glyph->xAdvance * static_cast<int16_t>(textsize_x);
    }
  }
  return 1;
}

void Adafruit_GFX::charBounds(unsigned char c, int16_t* x, int16_t* y, int16_t* minx, int16_t* miny, int16_t* maxx, int16_t* maxy) {
  if (!gfxFont) return;
  if (c == '\n') {
    *x = 0;
    *y += textsize_y * gfxFont->yAdvance;
  } else if (c != '\r') {
    if (c >= gfxFont->first && c <= gfxFont->last) {
      const GFXglyph* glyph = &gfxFont->glyph[c - gfxFont->first];
      const int16_t tsx = textsize_x, tsy = textsize_y;
      const int16_t x1 = *x + glyph->xOffset * tsx, y1 = *y + glyph->yOffset * tsy;
      const int16_t x2 = x1 + glyph->width * tsx - 1, y2 = y1 + glyph->height * tsy - 1;
      if (x1 < *minx) *minx = x1;
      if (y1 < *miny) *miny = y1;
      if (x2 > *maxx) *maxx = x2;
      if (y2 > *maxy) *maxy = y2;
      *x += glyph->xAdvance * tsx;
    }
  }
}

void Adafruit_GFX::getTextBounds(const char* str, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
  uint8_t c;
  int16_t minx = _width, miny = _height, maxx = -1, maxy = -1;
  *x1 = x;
  *y1 = y;
  *w = *h = 0;
  while ((c = static_cast<uint8_t>(*str++))) charBounds(c, &x, &y, &minx, &miny, &maxx, &maxy);
  if (maxx >= minx) {
    *x1 = minx;
    *w = maxx - minx + 1;
  }
  if (maxy >= miny) {
    *y1 = miny;
    *h = maxy - miny + 1;
  }
}
