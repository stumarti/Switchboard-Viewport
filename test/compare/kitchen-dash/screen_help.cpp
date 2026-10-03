// screen_help.cpp — button reference card, shown on KEY2 long press.
// No WiFi or data fetch needed — purely local content.
#include "shared_state.h"
#include "layout.h"
#include "small_icons.h"
#include "heating_icons.h"
#include "security_icons.h"

void drawHelpScreen() {
  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // ---- Title ----
    display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
    display.setTextSize(1);
    display.setTextColor(GxEPD_BLACK);
    int16_t bx,by; uint16_t bw,bh;
    const char* title = "Kitchen Dashboard";
    display.getTextBounds(title, 0, 0, &bx,&by,&bw,&bh);
    display.setCursor((PANEL_W - bw) / 2, 36);
    display.print(title);

    // Divider under title
    display.drawFastHLine(40, 48, PANEL_W - 80, GxEPD_BLACK);

    // ---- Button reference table ----
    // Three columns: KEY0 / KEY1 / KEY2, each with short and long press rows.
    // Layout: 3 equal columns across 800px, starting at y=70.

    const int COL_W  = PANEL_W / 3;         // ~266px each
    const int COL_Y  = 70;
    const int HELP_ROW_H = 90;                   // height per press type
    const int HELP_BAR_W = 4;

    struct Entry {
      const char* key;
      uint16_t    keyCol;
      const char* shortTitle;
      const char* shortDesc;
      uint16_t    shortCol;
      const char* longTitle;
      const char* longDesc;
      uint16_t    longCol;
    };

    Entry entries[] = {
      {
        "KEY 0", GxEPD_BLACK,
        "Refresh",      "Fetch latest data\nand redraw panel",  GxEPD_BLACK,
        "Not assigned", "",                                      GxEPD_BLACK,
      },
      {
        "KEY 1", GxEPD_BLACK,
        "Heating page",  "Per-zone temperatures\nand calling status",  GxEPD_RED,
        "Clear screen",  "Fills panel white\nNext wake redraws", GxEPD_BLACK,
      },
      {
        "KEY 2", GxEPD_BLACK,
        "Security page", "Alarm, doors, windows\nmotion and cameras",  GxEPD_GREEN,
        "This screen",   "Button reference\nand device info",  GxEPD_BLUE,
      },
    };

    for (int col = 0; col < 3; col++) {
      int cx = col * COL_W;
      Entry& e = entries[col];

      // Column heading — key name
      display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
      display.setTextSize(1);
      display.setTextColor(GxEPD_BLACK);
      display.getTextBounds(e.key, 0, 0, &bx,&by,&bw,&bh);
      display.setCursor(cx + (COL_W - bw) / 2, COL_Y);
      display.print(e.key);

      // Vertical divider between columns (not after last)
      if (col < 2)
        display.drawFastVLine(cx + COL_W, COL_Y - 10, PANEL_H - COL_Y + 10, GxEPD_BLACK);

      // Short press row
      {
        int ry = COL_Y + 16;
        display.fillRect(cx + 8, ry, HELP_BAR_W, HELP_ROW_H - 4, e.shortCol);

        display.setFont(&AtkinsonHyperlegible_Bold9pt7b);
        display.setTextSize(1);
        display.setTextColor(e.shortCol);
        display.setCursor(cx + 18, ry + 14);
        display.print("Short press");

        display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(cx + 18, ry + 34);
        display.print(e.shortTitle);

        if (e.shortDesc[0]) {
          display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
          display.setTextColor(GxEPD_BLACK);
          // Handle newline in description
          String desc = e.shortDesc;
          int nl = desc.indexOf('\n');
          if (nl >= 0) {
            display.setCursor(cx + 18, ry + 52);
            display.print(desc.substring(0, nl));
            display.setCursor(cx + 18, ry + 66);
            display.print(desc.substring(nl + 1));
          } else {
            display.setCursor(cx + 18, ry + 52);
            display.print(desc);
          }
        }
      }

      // Divider between short and long press
      for (int x = cx + 8; x < cx + COL_W - 8; x += 7)
        display.drawFastHLine(x, COL_Y + 16 + HELP_ROW_H, 4, GxEPD_BLACK);

      // Long press row
      {
        int ry = COL_Y + 16 + HELP_ROW_H + 10;
        display.fillRect(cx + 8, ry, HELP_BAR_W, HELP_ROW_H - 4, e.longCol);

        display.setFont(&AtkinsonHyperlegible_Bold9pt7b);
        display.setTextSize(1);
        display.setTextColor(e.longCol);
        display.setCursor(cx + 18, ry + 14);
        display.print("Long press (1s)");

        display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(cx + 18, ry + 34);
        display.print(e.longTitle);

        if (e.longDesc[0]) {
          display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
          display.setTextColor(GxEPD_BLACK);
          String desc = e.longDesc;
          int nl = desc.indexOf('\n');
          if (nl >= 0) {
            display.setCursor(cx + 18, ry + 52);
            display.print(desc.substring(0, nl));
            display.setCursor(cx + 18, ry + 66);
            display.print(desc.substring(nl + 1));
          } else {
            display.setCursor(cx + 18, ry + 52);
            display.print(desc);
          }
        }
      }
    }

    // ---- Horizontal divider before device info ----
    int divY = COL_Y + 16 + HELP_ROW_H * 2 + 20;
    display.drawFastHLine(40, divY, PANEL_W - 80, GxEPD_BLACK);

    // ---- Device info row ----
    {
      int iy = divY + 20;
      display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
      display.setTextSize(1);
      display.setTextColor(GxEPD_BLACK);

      // Left: sleep interval
      String sleepStr = "Refreshes every " + String((int)SLEEP_MINUTES) + " min";
      display.setCursor(40, iy);
      display.print(sleepStr);

      // Centre: current time
      String ts = currentTimeStr();
      display.getTextBounds(ts, 0, 0, &bx,&by,&bw,&bh);
      display.setCursor((PANEL_W - bw) / 2, iy);
      display.print(ts);

      // Right: battery %
      float vbat = readBatteryVoltage();
      int bpct = batteryPercent(vbat);
      String batStr = "Battery " + String(bpct) + "%";
      display.getTextBounds(batStr, 0, 0, &bx,&by,&bw,&bh);
      display.setCursor(PANEL_W - 40 - bw, iy);
      display.print(batStr);
    }

  } while (display.nextPage());
  display.hibernate();
}