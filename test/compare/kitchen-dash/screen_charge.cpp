// screen_charge.cpp — full-screen low-battery "charge me" warning.
#include "shared_state.h"
#include "layout.h"
#include "charge_screen.h"

// ============================================================================
// setup / loop
// ============================================================================
// Full-screen low-battery warning shown at <= 2%.
// Big red plug icon, "CHARGE ME" text. Skips WiFi/data entirely.
void drawChargeScreen(int pct) {
  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Big plug icon centred horizontally, upper-middle
    int iconX = (PANEL_W - 96) / 2;
    int iconY = 150;
    drawChargeIcon(display, cs_plug, iconX, iconY, GxEPD_RED);

    // "CHARGE ME" heading
    display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
    display.setTextSize(2);
    display.setTextColor(GxEPD_RED);
    int16_t bx,by; uint16_t bw,bh;
    display.getTextBounds("CHARGE ME", 0, 0, &bx, &by, &bw, &bh);
    display.setCursor((PANEL_W - bw) / 2, 320);
    display.print("CHARGE ME");

    // Battery percentage below
    display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
    display.setTextSize(1);
    display.setTextColor(GxEPD_BLACK);
    String pctStr = "Battery " + String(pct) + "%";
    display.getTextBounds(pctStr, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor((PANEL_W - bw) / 2, 370);
    display.print(pctStr);
  } while (display.nextPage());
  display.hibernate();
}