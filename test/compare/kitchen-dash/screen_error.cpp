// screen_error.cpp — full-screen "can't connect" message.
// Shown when WiFi fails or Home Assistant is unreachable (bad token, HA down,
// network issue), so the panel says what's wrong instead of going blank/stale.
#include "shared_state.h"
#include "layout.h"
#include "error_icons.h"

void drawErrorScreen(const char* title, const char* detail) {
  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();

  // Pick icon by title: WiFi problems get the wifi-off glyph, everything else
  // (HA unreachable, auth, etc.) gets the cloud-off glyph.
  const uint8_t* icon = err_cloud_off;
  String t = title ? String(title) : "";
  if (t.indexOf("WiFi") >= 0 || t.indexOf("Wi-Fi") >= 0) icon = err_wifi_off;

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Icon centered, upper third
    const int ICON_SZ = 96;
    int iconX = (PANEL_W - ICON_SZ) / 2;
    int iconY = 110;
    drawErrorIcon(display, icon, iconX, iconY, GxEPD_RED);

    int16_t bx, by; uint16_t bw, bh;

    // Title — big bold, centered
    display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
    display.setTextSize(2);
    display.setTextColor(GxEPD_BLACK);
    display.getTextBounds(title, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor((PANEL_W - bw) / 2, 290);
    display.print(title);

    // Detail line — smaller, centered
    if (detail && detail[0]) {
      display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
      display.setTextSize(1);
      display.setTextColor(GxEPD_BLACK);
      display.getTextBounds(detail, 0, 0, &bx, &by, &bw, &bh);
      display.setCursor((PANEL_W - bw) / 2, 340);
      display.print(detail);
    }

    // Date and time at the bottom — date matters if the error persists for days
    display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
    display.setTextSize(1);
    display.setTextColor(GxEPD_BLACK);
    // Build "Day DD Mon YYYY  HH:MM" string from local time
    struct tm tm;
    String dateTime;
    if (getLocalTime(&tm, 100)) {
      char buf[32];
      strftime(buf, sizeof(buf), "%a %d %b %Y  %H:%M", &tm);
      dateTime = String(buf);
    } else {
      dateTime = "--:--";
    }
    display.getTextBounds(dateTime, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor((PANEL_W - bw) / 2, 420);
    display.print(dateTime);

  } while (display.nextPage());
  display.hibernate();
}