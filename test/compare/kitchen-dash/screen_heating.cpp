// screen_heating.cpp — heating detail page (Key 1).
#include "shared_state.h"
#include "layout.h"
#include "heating_icons.h"
#include "small_icons.h"   // si_dev_battery, si_refresh, draw helpers for footer

static inline float roundDownTo(float v, float step) {
  return floorf(v / step) * step;
}
// Round v up to the nearest multiple of `step`.
static inline float roundUpTo(float v, float step) {
  return ceilf(v / step) * step;
}

void drawHeatingPage() {
  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();

  // ---- Compute the shared temperature scale across all zones ----
  // Min/max consider both current and setpoint of every valid zone, then
  // round to the nearest 5°C below the group minimum / above the group maximum.
  float gMin = 1e9, gMax = -1e9;
  for (int i = 0; i < heatZoneCount; i++) {
    ZoneData& z = heatZones[i];
    if (!z.valid) continue;
    if (!isnan(z.current)) { if (z.current < gMin) gMin = z.current; if (z.current > gMax) gMax = z.current; }
    if (!isnan(z.target))  { if (z.target  < gMin) gMin = z.target;  if (z.target  > gMax) gMax = z.target;  }
  }
  if (gMin > gMax) { gMin = 5; gMax = 25; }   // fallback if no data
  const float scaleMin = roundDownTo(gMin, 5.0f);
  const float scaleMax = roundUpTo(gMax, 5.0f);
  const float scaleSpan = (scaleMax - scaleMin) > 0 ? (scaleMax - scaleMin) : 1.0f;

  // ---- Layout constants (hoisted above do{} — flat-scope compiler) ----
  const int HP_LEFT_W   = 300;            // left flood panel width
  const int HP_SCREEN_W = PANEL_W;
  const int HP_SCREEN_H = PANEL_H;
  const int HP_LEFT_SPLIT_Y = 360;        // boundary between heating/hot-water blocks
  const bool heatOn    = heatAnyActive;
  const bool waterOn   = (currentWaterOp == "heating");

  // Right panel geometry
  const int RPANEL_X     = HP_LEFT_W + 24;                 // right content left edge
  const int HP_RIGHT_W   = HP_SCREEN_W - RPANEL_X - 20;    // right content width
  const int HP_ROW_TOP   = 18;
  const int HP_ROW_H     = 52;                    // per-zone row height
  const int TBAR_H       = 12;                    // temperature bar thickness
  const int HP_FLAME_W   = 22;                    // space for the calling flame
  const int TBAR_X       = RPANEL_X;              // bar spans full right width
  const int TBAR_W       = HP_RIGHT_W - HP_FLAME_W - 6;

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // ============================================================
    // LEFT PANEL — split flood block
    // ============================================================
    // Heating block (top) — flood color, with summary lines below ON/OFF
    {
      uint16_t bg = heatOn ? GxEPD_RED : GxEPD_BLACK;
      display.fillRect(0, 0, HP_LEFT_W, HP_LEFT_SPLIT_Y, bg);
      int16_t bx,by; uint16_t bw,bh;

      // Flame icon centered near top
      drawHeatIcon(display, hi_flame_lg, (HP_LEFT_W - 64) / 2, 24, 64, GxEPD_WHITE);

      // "HEATING" heading
      display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
      display.setTextSize(1);
      display.setTextColor(GxEPD_WHITE);
      display.getTextBounds("HEATING", 0, 0, &bx,&by,&bw,&bh);
      display.setCursor((HP_LEFT_W - bw)/2, 116);
      display.print("HEATING");

      // Status word (ON / OFF) big
      display.setFont(&AtkinsonHyperlegible_Bold42pt7b);
      const char* word = heatOn ? "ON" : "OFF";
      display.getTextBounds(word, 0, 0, &bx,&by,&bw,&bh);
      display.setCursor((HP_LEFT_W - bw)/2, 200);
      display.print(word);

      // Three summary lines below ON/OFF
      display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
      display.setTextColor(GxEPD_WHITE);

      // Average current temperature
      if (!isnan(currentHeatingTemp)) {
        String s = String(currentHeatingTemp, 1) + "C now";
        display.getTextBounds(s, 0, 0, &bx,&by,&bw,&bh);
        display.setCursor((HP_LEFT_W - bw)/2, 250);
        display.print(s);
      }

      // Average setpoint
      if (!isnan(currentHeatingTarget)) {
        String s = "Set " + String(currentHeatingTarget, 1) + "C";
        display.getTextBounds(s, 0, 0, &bx,&by,&bw,&bh);
        display.setCursor((HP_LEFT_W - bw)/2, 282);
        display.print(s);
      }

      // Zones currently calling for heat
      {
        String s = String(heatCallingCount) + " of " + String(heatZoneCount) + " calling";
        display.getTextBounds(s, 0, 0, &bx,&by,&bw,&bh);
        display.setCursor((HP_LEFT_W - bw)/2, 322);
        display.print(s);
      }
    }
    // Hot water block (bottom) — compressed to fit smaller area
    {
      uint16_t bg = waterOn ? GxEPD_RED : GxEPD_BLACK;
      display.fillRect(0, HP_LEFT_SPLIT_Y, HP_LEFT_W, HP_SCREEN_H - HP_LEFT_SPLIT_Y, bg);
      // Drop icon on the left
      drawHeatIcon(display, hi_drop_lg, 14, HP_LEFT_SPLIT_Y + 28, 64, GxEPD_WHITE);
      int16_t bx,by; uint16_t bw,bh;
      // Heading to the right of the icon
      display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
      display.setTextSize(1);
      display.setTextColor(GxEPD_WHITE);
      display.setCursor(90, HP_LEFT_SPLIT_Y + 50);
      display.print("HOT WATER");
      // current / target line
      display.setFont(&AtkinsonHyperlegible_Bold9pt7b);
      String wt = String(currentWaterTemp) + "C  ·  set " + currentWaterTarget + "C";
      display.setCursor(90, HP_LEFT_SPLIT_Y + 78);
      display.print(wt);
    }

    // ============================================================
    // RIGHT PANEL — per-zone temperature bars
    // ============================================================
    // ============================================================
    // Scale anchors at the TOP of the stack — min (left) / max (right).
    // These apply to every bar below, so they sit once at the top.
    {
      display.setFont(&AtkinsonHyperlegible_Bold9pt7b);
      display.setTextSize(1);
      display.setTextColor(GxEPD_BLACK);
      String mn = String((int)scaleMin) + "C";
      String mx = String((int)scaleMax) + "C";
      int16_t bx,by; uint16_t bw,bh;
      display.setCursor(TBAR_X, HP_ROW_TOP - 2);
      display.print(mn);
      display.getTextBounds(mx, 0, 0, &bx,&by,&bw,&bh);
      display.setCursor(TBAR_X + TBAR_W - bw, HP_ROW_TOP - 2);
      display.print(mx);
    }

    for (int i = 0; i < heatZoneCount; i++) {
      ZoneData& z = heatZones[i];
      if (!z.valid) continue;
      int y = HP_ROW_TOP + 10 + i * HP_ROW_H;   // +10 to clear the anchor header

      // Top line: room name (left) + "current → set" temps (right), above the bar
      display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
      display.setTextSize(1);
      display.setTextColor(z.active ? GxEPD_RED : GxEPD_BLACK);
      display.setCursor(RPANEL_X, y + 14);
      display.print(z.label);

      // Temps right-aligned on the same line: "20.4C -> 21C"
      display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
      display.setTextColor(GxEPD_BLACK);
      String temps = "";
      if (!isnan(z.current)) temps += String(z.current, 1) + "C";
      if (!isnan(z.target))  temps += "  set " + String(z.target, 0) + "C";
      {
        int16_t bx,by; uint16_t bw,bh;
        display.getTextBounds(temps, 0, 0, &bx,&by,&bw,&bh);
        display.setCursor(TBAR_X + TBAR_W - bw, y + 14);
        display.print(temps);
      }

      // ---- Temperature bar (full width, below the text) ----
      int barY = y + 24;
      display.drawRect(TBAR_X, barY, TBAR_W, TBAR_H, GxEPD_BLACK);

      if (!isnan(z.current)) {
        // Map a temperature to an x within the bar
        auto tempToX = [&](float t) -> int {
          float f = (t - scaleMin) / scaleSpan;
          if (f < 0) f = 0; if (f > 1) f = 1;
          return TBAR_X + (int)(f * (TBAR_W - 1));
        };
        int xCur = tempToX(z.current);

        if (!isnan(z.target)) {
          int xSet = tempToX(z.target);
          // Fill from setpoint to current.
          // Red if heating needed (current < set), blue if current > set.
          int xL = xCur < xSet ? xCur : xSet;
          int xR = xCur < xSet ? xSet : xCur;
          uint16_t fill = (z.current < z.target) ? GxEPD_RED : GxEPD_BLUE;
          if (xR > xL)
            display.fillRect(xL + 1, barY + 1, xR - xL - 1, TBAR_H - 2, fill);
          // Setpoint marker — black tick
          display.fillRect(xSet, barY - 3, 2, TBAR_H + 6, GxEPD_BLACK);
        } else {
          // No setpoint (zone off / frost protect) — just show a small
          // current-position marker so the bar isn't empty.
          display.fillRect(xCur - 1, barY - 3, 3, TBAR_H + 6, GxEPD_BLACK);
        }
      }

      // ---- Calling flame after the bar ----
      drawHeatIcon(display, hi_flame_sm, TBAR_X + TBAR_W + 4, barY - 4, 20,
                   z.active ? GxEPD_RED : GxEPD_BLACK);
    }

    // Standard footer (matches the main status screen): refresh icon · time ·
    // battery icon + percentage, right-aligned along FOOTER_Y.
    {
      int bpct = batteryPercent(readBatteryVoltage());
      uint16_t batCol = batteryColor(bpct);
      String pctStr = String(bpct) + "%";
      String footer = currentTimeStr();

      display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
      display.setTextSize(1);
      int16_t bx,by; uint16_t bw,bh;

      display.getTextBounds(pctStr, 0, 0, &bx,&by,&bw,&bh);
      int pctX  = PANEL_W - FOOTER_MARGIN_R - bw;
      int iconX = pctX - 24 - 2;
      int iconY = FOOTER_ICON_Y;
      drawSmallIconTinted(display, si_dev_battery, iconX, iconY, batCol);
      display.setTextColor(batCol);
      display.setCursor(pctX, FOOTER_Y);
      display.print(pctStr);

      display.getTextBounds(footer, 0, 0, &bx,&by,&bw,&bh);
      int timeX = iconX - FOOTER_GAP - bw;
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(timeX, FOOTER_Y);
      display.print(footer);

      int refX = timeX - 16 - 4;
      int refY = FOOTER_TINY_Y;
      drawTinyIcon(display, si_refresh, refX, refY, GxEPD_BLACK);
    }

  } while (display.nextPage());
  display.hibernate();
}