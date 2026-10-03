#include "render/draw.h"

#include <string.h>

#include "kd/AtkinsonHyperlegible_Regular9pt7b.h"
#include "kd/AtkinsonHyperlegible_Bold9pt7b.h"
#include "kd/AtkinsonHyperlegible_Bold12pt7b.h"
#include "kd/AtkinsonHyperlegible_Bold42pt7b.h"

namespace draw {

uint16_t color(int idx) {
  static const uint16_t P[] = {GxEPD_WHITE, GxEPD_BLACK, GxEPD_RED, GxEPD_YELLOW, GxEPD_GREEN, GxEPD_BLUE};
  return idx >= 0 && idx < 6 ? P[idx] : GxEPD_BLACK;
}

static const GFXfont* const BUILT_IN[FACE_COUNT] = {
    &AtkinsonHyperlegible_Regular9pt7b,
    &AtkinsonHyperlegible_Bold9pt7b,
    &AtkinsonHyperlegible_Bold12pt7b,
    &AtkinsonHyperlegible_Bold42pt7b,
};
static const GFXfont* g_override[FACE_COUNT] = {nullptr, nullptr, nullptr, nullptr};

const GFXfont* font(Face f) { return f < FACE_COUNT ? (g_override[f] ? g_override[f] : BUILT_IN[f]) : BUILT_IN[0]; }
void setFont(Face f, const GFXfont* o) {
  if (f < FACE_COUNT) g_override[f] = o;
}

void face(Gfx& g, Face f, uint16_t col, uint8_t size) {
  g.setFont(font(f));
  g.setTextSize(size);
  g.setTextColor(col);
  g.setTextWrap(false);
}

Bounds bounds(Gfx& g, const char* s, int x, int y) {
  Bounds b{0, 0, 0, 0};
  g.getTextBounds(s, x, y, &b.x, &b.y, &b.w, &b.h);
  return b;
}

void text(Gfx& g, int x, int y, const char* s) {
  g.setCursor(x, y);
  g.print(s);
}

void textRight(Gfx& g, const char* s, int right, int y) {
  const Bounds b = bounds(g, s);
  g.setCursor(right - b.w, y);
  g.print(s);
}

void textCentered(Gfx& g, const char* s, int x, int w, int y) {
  const Bounds b = bounds(g, s);
  g.setCursor(x + (w - static_cast<int>(b.w)) / 2, y);
  g.print(s);
}

int textWrapped(Gfx& g, const char* s, int x, int y, int w, int lineH, int maxLines) {
  char line[164];
  int lines = 0;
  const char* p = s;
  while (*p && lines < maxLines) {
    while (*p == ' ') ++p;
    if (!*p) break;
    // The most whole words that fit (a single long word is cut instead).
    size_t fit = 0;
    const char* q = p;
    for (;;) {
      const char* e = q;
      while (*e && *e != ' ') ++e;
      const size_t len = static_cast<size_t>(e - p);
      if (len >= sizeof(line) - 4) break;
      memcpy(line, p, len);
      line[len] = 0;
      if (fit && bounds(g, line).w > w) break;
      fit = len;
      if (!*e) break;
      q = e;
      while (*q == ' ') ++q;
    }
    if (!fit) fit = strnlen(p, sizeof(line) - 4);
    memcpy(line, p, fit);
    line[fit] = 0;
    const char* rest = p + fit;
    while (*rest == ' ') ++rest;
    // The last line, with more to come, ends in "...".
    if (lines == maxLines - 1 && *rest) {
      char tmp[sizeof(line)];
      for (size_t n = strlen(line); ; --n) {
        snprintf(tmp, sizeof(tmp), "%.*s...", static_cast<int>(n), line);
        if (n == 0 || bounds(g, tmp).w <= w) break;
      }
      snprintf(line, sizeof(line), "%s", tmp);
    }
    text(g, x, y + lines * lineH, line);
    ++lines;
    p = rest;
  }
  return lines;
}

const char* fixed(char* out, size_t cap, float value, int prec) {
  if (!cap) return out;
  double number = value;
  if (number != number) return snprintf(out, cap, "nan"), out;
  if (number - number != 0) return snprintf(out, cap, "inf"), out;
  char buf[48];
  size_t n = 0;
  int fillme = prec + 2;
  if (prec > 0) fillme -= prec + 1;
  const bool negative = number < 0.0;
  if (negative) {
    fillme--;
    number = -number;
  }
  double rounding = 2.0;
  for (int i = 0; i < prec; ++i) rounding *= 10.0;
  number += 1.0 / rounding;
  double tenpow = 1.0;
  int digitcount = 1;
  while (number >= 10.0 * tenpow && digitcount < 20) {
    tenpow *= 10.0;
    digitcount++;
  }
  number /= tenpow;
  fillme -= digitcount;
  while (fillme-- > 0 && n < sizeof(buf) - 1) buf[n++] = ' ';
  if (negative && n < sizeof(buf) - 1) buf[n++] = '-';
  digitcount += prec;
  while (digitcount-- > 0 && n < sizeof(buf) - 2) {
    int digit = static_cast<int>(number);
    if (digit > 9) digit = 9;
    buf[n++] = static_cast<char>('0' | digit);
    if (digitcount == prec && prec > 0) buf[n++] = '.';
    number -= digit;
    number *= 10.0;
  }
  buf[n] = 0;
  snprintf(out, cap, "%s", buf);
  return out;
}

void cut(char* out, size_t cap, const char* src, int limit, int keep) {
  const size_t len = strlen(src);
  if (static_cast<int>(len) <= limit) {
    snprintf(out, cap, "%s", src);
    return;
  }
  snprintf(out, cap, "%.*s...", keep, src);
}

// --- Icons -----------------------------------------------------------------

static void fourBit(Gfx& g, const uint8_t* bmp, int w, int h, int x, int y, uint16_t tint, Mode mode) {
  const int rowBytes = (w + 1) / 2;
  for (int row = 0; row < h; ++row) {
    for (int col = 0; col < w; ++col) {
      const uint8_t b = pgm_read_byte(&bmp[row * rowBytes + col / 2]);
      const uint8_t v = (col & 1) ? (b & 0x0F) : (b >> 4);
      if (v >= 6) continue;
      if (mode == Mode::Mask && v != 1) continue;
      if (mode == Mode::Ink && v == 0) continue;
      g.drawPixel(x + col, y + row, v == 1 ? tint : color(v));
    }
  }
}

static void oneBit(Gfx& g, const uint8_t* bits, int w, int h, int x, int y, uint16_t tint, Mode mode) {
  const int rowBytes = (w + 7) / 8;
  for (int row = 0; row < h; ++row) {
    for (int col = 0; col < w; ++col) {
      // Switchboard's icon format: bit 0 is ink, 1 is background.
      const bool ink = !(pgm_read_byte(&bits[row * rowBytes + col / 8]) & (0x80 >> (col & 7)));
      if (ink) g.drawPixel(x + col, y + row, tint);
      else if (mode == Mode::Opaque) g.drawPixel(x + col, y + row, GxEPD_WHITE);
    }
  }
}

void icon(Gfx& g, const Icon& ic, int x, int y, uint16_t tint, Mode mode) {
  if (!ic.bits) return;
  if (ic.fourBit) fourBit(g, ic.bits, ic.w, ic.h, x, y, tint, mode);
  else oneBit(g, ic.bits, ic.w, ic.h, x, y, tint, mode);
}

void picture(Gfx& g, const uint8_t* bits, int w, int h, int x, int y) {
  fourBit(g, bits, w, h, x, y, GxEPD_BLACK, Mode::Opaque);
}

void dottedV(Gfx& g, int x, int y0, int y1) {
  for (int y = y0; y < y1; y += 7) g.drawFastVLine(x, y, 2, GxEPD_BLACK);
}

void dottedH(Gfx& g, int x0, int x1, int y) {
  for (int x = x0; x < x1; x += 7) g.drawFastHLine(x, y, 2, GxEPD_BLACK);
}

}  // namespace draw
