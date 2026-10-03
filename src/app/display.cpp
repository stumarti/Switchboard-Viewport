#include "app/display.h"

#include <GxEPD2_7C.h>
#include <SPI.h>

#include "config.h"

namespace display {

namespace {
#define GxEPD2_DRIVER_CLASS GxEPD2_730c_GDEP073E01
#define MAX_DISPLAY_BUFFER_SIZE 16000
#define MAX_HEIGHT(EPD) \
  (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))
using Panel = GxEPD2_7C<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>;

Panel g_panel(GxEPD2_DRIVER_CLASS(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));
SPIClass g_spi(HSPI);
bool g_begun = false;
}  // namespace

void begin() {
  if (g_begun) return;
  pinMode(EPD_RES_PIN, OUTPUT);
  pinMode(EPD_DC_PIN, OUTPUT);
  pinMode(EPD_CS_PIN, OUTPUT);
  g_spi.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);
  g_panel.epd2.selectSPI(g_spi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  g_begun = true;
}

void show(const std::function<void(draw::Gfx&)>& paint) {
  begin();
  g_panel.init(115200);
  g_panel.setRotation(0);
  g_panel.setFullWindow();
  g_panel.firstPage();
  do {
    paint(g_panel);
  } while (g_panel.nextPage());
  g_panel.hibernate();
}

void clear() {
  show([](draw::Gfx& g) { g.fillScreen(GxEPD_WHITE); });
}

}  // namespace display
