// Host stand-in for Adafruit_GFX: the drawing and text calls the firmware
// makes, with the library's own algorithms for custom-font text placement,
// text bounds and circles, so text lands where it does on the panel.
#pragma once
#include "Arduino.h"
#include "gfxfont.h"

class Adafruit_GFX : public Print {
 public:
  Adafruit_GFX(int16_t w, int16_t h) : _width(w), _height(h) {}
  virtual void drawPixel(int16_t x, int16_t y, uint16_t color) = 0;
  virtual void fillScreen(uint16_t color) { fillRect(0, 0, _width, _height, color); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) { for (int16_t i = 0; i < h; ++i) drawPixel(x, y + i, c); }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) { for (int16_t i = 0; i < w; ++i) drawPixel(x + i, y, c); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
    for (int16_t j = 0; j < h; ++j) drawFastHLine(x, y + j, w, c);
  }
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
    drawFastHLine(x, y, w, c);
    drawFastHLine(x, y + h - 1, w, c);
    drawFastVLine(x, y, h, c);
    drawFastVLine(x + w - 1, y, h, c);
  }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c);
  void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t c);
  void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t c);
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c);
  void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t corners, int16_t delta, uint16_t c);

  void setFont(const GFXfont* f) { gfxFont = const_cast<GFXfont*>(f); }
  void setTextSize(uint8_t s) { textsize_x = textsize_y = s > 0 ? s : 1; }
  void setTextColor(uint16_t c) { textcolor = textbgcolor = c; }
  void setTextColor(uint16_t c, uint16_t bg) { textcolor = c; textbgcolor = bg; }
  void setTextWrap(bool w) { wrap = w; }
  void setCursor(int16_t x, int16_t y) { cursor_x = x; cursor_y = y; }
  int16_t getCursorX() const { return cursor_x; }
  int16_t getCursorY() const { return cursor_y; }
  int16_t width() const { return _width; }
  int16_t height() const { return _height; }
  void getTextBounds(const char* str, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h);
  void getTextBounds(const String& s, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
    getTextBounds(s.c_str(), x, y, x1, y1, w, h);
  }
  size_t write(uint8_t c) override;
  using Print::write;

 protected:
  void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint8_t sx, uint8_t sy);
  void charBounds(unsigned char c, int16_t* x, int16_t* y, int16_t* minx, int16_t* miny, int16_t* maxx, int16_t* maxy);
  int16_t _width, _height;
  int16_t cursor_x = 0, cursor_y = 0;
  uint16_t textcolor = 0xFFFF, textbgcolor = 0xFFFF;
  uint8_t textsize_x = 1, textsize_y = 1;
  bool wrap = true;
  GFXfont* gfxFont = nullptr;
};
