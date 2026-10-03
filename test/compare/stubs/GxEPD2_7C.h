// GxEPD2's 7-colour panel, on the host: the drawing lands on a Canvas.
#pragma once
#include "kd_env.h"
#include "SPI.h"
#include "canvas.h"
struct GxEPD2_730c_GDEP073E01 {
  static const int16_t WIDTH = 800, HEIGHT = 480;
  GxEPD2_730c_GDEP073E01(int, int, int, int) {}
  void selectSPI(SPIClass&, SPISettings) {}
};
template <typename Driver, int PageHeight>
class GxEPD2_7C : public Canvas {
 public:
  explicit GxEPD2_7C(Driver d) : epd2(d) {}
  void init(uint32_t) {}
  void setRotation(int) {}
  void setFullWindow() {}
  void firstPage() {}
  bool nextPage() { return false; }
  void hibernate() {}
  Driver epd2;
};
