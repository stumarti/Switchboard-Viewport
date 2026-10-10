#include "app/display.h"

#include <GxEPD2_7C.h>
#include <SPI.h>

#include "config.h"
#include "log.h"

namespace display {

namespace {
// GxEPD2's driver gives up waiting for the panel after 20 s, then powers it
// off and puts it to sleep. A Spectra 6 refresh lays the colours down in
// turn, blue last, and when the panel is cool it runs a little past that:
// cut off there, the blue never fills in. Allow it 10 s more.
class Driver : public GxEPD2_730c_GDEP073E01 {
 public:
  Driver(int16_t cs, int16_t dc, int16_t rst, int16_t busy) : GxEPD2_730c_GDEP073E01(cs, dc, rst, busy) {
    _busy_timeout = BUSY_TIMEOUT_US;
  }
  static constexpr uint32_t BUSY_TIMEOUT_US = 30UL * 1000 * 1000;
};
#define GxEPD2_DRIVER_CLASS Driver
#define MAX_DISPLAY_BUFFER_SIZE 16000
#define MAX_HEIGHT(EPD) \
  (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))
using Panel = GxEPD2_7C<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>;

Panel g_panel(GxEPD2_DRIVER_CLASS(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));
SPIClass g_spi(HSPI);
bool g_begun = false;

// How long the panel was busy, from the driver's busy callback (called
// every millisecond or so while it waits): the longest single wait of a
// show() is its refresh.
uint32_t g_waitStart = 0, g_lastCall = 0, g_longest = 0;
void whileBusy(const void*) {
  const uint32_t now = millis();
  if (now - g_lastCall > 50) g_waitStart = now;  // a new wait
  g_lastCall = now;
  if (now - g_waitStart > g_longest) g_longest = now - g_waitStart;
  delay(1);
}
// The last refresh, kept through deep sleep for the device info screen.
RTC_DATA_ATTR uint32_t rtcRefreshMs = 0;
}  // namespace

uint32_t lastRefreshMs() { return rtcRefreshMs; }

void begin() {
  if (g_begun) return;
  pinMode(EPD_RES_PIN, OUTPUT);
  pinMode(EPD_DC_PIN, OUTPUT);
  pinMode(EPD_CS_PIN, OUTPUT);
  g_spi.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);
  g_panel.epd2.selectSPI(g_spi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  g_panel.epd2.setBusyCallback(whileBusy);
  g_begun = true;
}

void show(const std::function<void(draw::Gfx&)>& paint, int rotation) {
  begin();
  g_panel.init(115200);
  // As ESPHome drives the E1002: the busy line pulled up, so it reads
  // "busy" only while the panel holds it low (init() leaves it floating).
  pinMode(EPD_BUSY_PIN, INPUT_PULLUP);
  // The display turned clockwise: the picture turned back the other way.
  // (GFX rotation 1 puts the panel's top edge, the buttons, on the left.)
  g_panel.setRotation((4 - (rotation / 90) % 4) % 4);
  g_panel.setFullWindow();
  g_longest = 0;
  g_lastCall = 0;
  g_panel.firstPage();
  do {
    paint(g_panel);
  } while (g_panel.nextPage());
  g_panel.hibernate();
  rtcRefreshMs = g_longest;
  LOGF("Panel: refreshed in %.1f s%s\n", g_longest / 1000.0,
       g_longest * 1000ULL >= Driver::BUSY_TIMEOUT_US ? " (gave up waiting: the picture may be unfinished)" : "");
}

void use(int, bool) {}

void clear() {
  show([](draw::Gfx& g) { g.fillScreen(GxEPD_WHITE); });
}

}  // namespace display
