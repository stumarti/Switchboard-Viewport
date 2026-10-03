// screen_status.cpp — the main status dashboard.
// Real translation unit: includes shared_state.h for all globals/helpers,
// plus the icon headers it draws with.
//
// DEBUG: uncomment the line below to draw a coordinate grid over the dashboard.
// Every 20px = dashed line, every 100px = solid line with coordinate label.
// Comment it out again before flashing for production use.
// #define DEBUG_GRID
#include "shared_state.h"
#include "layout.h"
#include "weather_icons.h"
#include "mini_weather_icons.h"
#include "status_icons.h"
#include "status_icons_sm.h"
#include "dashboard_icons.h"
#include "small_icons.h"

// Energy cell sizing (status-screen only)
#define ENERGY_CELL_W  (LC_W / 2)
#define ENERGY_ICON_W  44
#define ENERGY_ICON_H  44

// Energy cell: 44x44 icon centred in cellW, value text centred below it.
// x = left edge of the cell, y = top of the cell.
// iconColor: BLACK normally, accent color when value >= 1kWh.

void drawEnergyCell(const uint8_t* icon, const String& value,
                    uint16_t iconColor, int x, int y) {
  static const uint16_t BASE_COLORS[] = {
    GxEPD_WHITE, GxEPD_BLACK, GxEPD_RED,
    GxEPD_YELLOW, GxEPD_GREEN, GxEPD_BLUE
  };

  // Centre icon horizontally within the cell
  int iconX = x + (ENERGY_CELL_W - ENERGY_ICON_W) / 2;
  int idx = 0;
  for (int16_t row = 0; row < ENERGY_ICON_H; row++) {
    for (int16_t col = 0; col < ENERGY_ICON_W; col += 2) {
      uint8_t b  = pgm_read_byte(&icon[idx++]);
      uint8_t hi = (b >> 4) & 0xF;
      uint8_t lo = b & 0xF;
      uint16_t cHi = (hi == 1) ? iconColor : (hi < 6 ? BASE_COLORS[hi] : GxEPD_WHITE);
      uint16_t cLo = (lo == 1) ? iconColor : (lo < 6 ? BASE_COLORS[lo] : GxEPD_WHITE);
      display.drawPixel(iconX + col,     y + row, cHi);
      display.drawPixel(iconX + col + 1, y + row, cLo);
    }
  }

  // Centre text horizontally within the cell
  display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK);
  int16_t bx, by; uint16_t bw, bh;
  display.getTextBounds(value, 0, 0, &bx, &by, &bw, &bh);
  int textX = x + (ENERGY_CELL_W - bw) / 2 - bx;
  display.setCursor(textX, y + ENERGY_ICON_H + 13);
  display.print(value);
}

// Return accent color for an energy value string when it exceeds 1 kWh,
// otherwise return GxEPD_BLACK.
uint16_t energyColor(const String& valueStr, uint16_t accentColor) {
  float v = valueStr.toFloat();
  return (v >= 1.0f) ? accentColor : GxEPD_BLACK;
}

void drawDashboard() {
  float vbat  = readBatteryVoltage();
  int   bpct  = batteryPercent(vbat);
  String footer = currentTimeStr();   // battery now drawn as icon, not text
  int ey = 0;  // reused for energy grid y and battery y

  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();

  // Shared right-column layout constants — hoisted outside do loop
  // to avoid redefinition errors across nested scopes.
  const int RC_BAR_W  = 4;     // colored left bar width for NOW/TODAY items
  const int RC_BAR_X  = RC_X;  // bar x — flush to divider

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // ----------------------------------------------------------------
    // VERTICAL DIVIDER — dotted, full panel height (2px dot, 5px gap)
    // ----------------------------------------------------------------
    for (int y = 0; y < PANEL_H; y += 7)
      display.drawFastVLine(DIV_X, y, 2, GxEPD_BLACK);

    // ----------------------------------------------------------------
    // RIGHT COLUMN STATUS BAR — horizontal dotted line at y=50
    // Creates a 50px tall status bar at the top of the right section
    // ----------------------------------------------------------------
    for (int x = RC_X; x < PANEL_W; x += 7)
      display.drawFastHLine(x, RC_STATUS_H, 2, GxEPD_BLACK);

    // ================================================================
    // LEFT COLUMN
    // ================================================================

    // ---- Big temperature — colour coded by value ----
    // Blue: below 0°C  |  Black: 0–20°C  |  Red: above 20°C
    {
      float tempVal = currentWx.tempNum.toFloat();
      uint16_t tempCol = GxEPD_BLACK;
      if (tempVal < 0.0f)  tempCol = GxEPD_BLUE;
      if (tempVal > 20.0f) tempCol = GxEPD_RED;

      display.setFont(&AtkinsonHyperlegible_Bold42pt7b);
      display.setTextSize(1);
      display.setTextColor(tempCol);
      display.setCursor(TEMP_X, TEMP_Y);
      display.print(currentWx.tempNum);

      // Degree circle drawn manually (font has no degree glyph), same colour
      int16_t bx,by; uint16_t bw,bh;
      display.getTextBounds(currentWx.tempNum, TEMP_X, TEMP_Y, &bx,&by,&bw,&bh);
      int cx = TEMP_X + bw + 15;
      int cy = TEMP_Y - bh + 3;
      for (int r = 8; r >= 5; r--) {
        display.drawCircle(cx, cy, r, tempCol);
      }
    }

    // ---- Weather icon (top-right of left column) ----
    {
      const uint8_t* bmp = weatherIconBitmap(currentWx.condition);
      if (bmp) drawWeatherIcon(display, bmp, WX_ICON_X, WX_ICON_Y);
    }

    // ---- Detail row: humidity · wind · UV ----
    // Three groups evenly spaced across the left column (240px / 3 = 80px each)
    {
      const int CELL = (LC_W + LC_X) / 3;  // ~80px per group
      const int DX_OFF = 5;                 // 5px right shift for detail icons
      int dy = DETAIL_Y;
      display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
      display.setTextSize(1);
      display.setTextColor(GxEPD_BLACK);

      // -- Humidity --
      int dx = LC_X + DX_OFF;
      drawSmallIcon(display, si_humidity, dx, dy - 18);
      display.setCursor(dx + 26, dy);
      display.print(currentWx.humidity); display.print("%");

      // -- Wind: weather-windy icon + speed number + direction arrow --
      dx = LC_X + CELL + DX_OFF;
      drawSmallIcon(display, si_wind, dx, dy - 18);
      display.setCursor(dx + 26, dy);
      display.print(currentWx.windSpeed);
      {
        int16_t bx,by; uint16_t bw,bh;
        display.getTextBounds(currentWx.windSpeed, 0, 0, &bx,&by,&bw,&bh);
        drawSmallIcon(display, windArrow(currentWx.windBearing), dx + 26 + bw + 4, dy - 18);
      }

      // -- UV: sun-wireless icon + value --
      dx = LC_X + CELL * 2 + DX_OFF;
      drawSmallIcon(display, si_uv, dx, dy - 18);
      display.setCursor(dx + 26, dy);
      display.print(currentWx.uvIndex);
    }

    // ================================================================
    // RAIN / SOLAR ROW  (y = RAIN_SOLAR_Y)
    // Priority: rain state > solar forecast.
    // - Currently raining: "Rain stops at HH:MM" (or "Rain continuing" if no stop)
    // - Rain coming:       "Rain at HH:MM"
    // - Dry + Solcast:     "Today X.X kWh  Tmr X.X kWh"
    // ================================================================
    {
      display.setFont(&AtkinsonHyperlegible_Bold9pt7b);
      display.setTextSize(1);

      if (currentRainState == 1) {
        // Currently raining — show stop time
        drawSmallIconTinted(display, si_water, LC_X, RAIN_SOLAR_Y - 16, GxEPD_BLUE);
        display.setTextColor(GxEPD_BLUE);
        display.setCursor(LC_X + 28, RAIN_SOLAR_Y);
        display.print("Rain stops " + currentRainTime);

      } else if (currentRainState == 2) {
        // Rain coming — show start time
        drawSmallIconTinted(display, si_water, LC_X, RAIN_SOLAR_Y - 16, GxEPD_BLUE);
        display.setTextColor(GxEPD_BLUE);
        display.setCursor(LC_X + 28, RAIN_SOLAR_Y);
        display.print("Rain at " + currentRainTime);

      } else if (currentRainState == 3) {
        // Raining, no stop in 12h window
        drawSmallIconTinted(display, si_water, LC_X, RAIN_SOLAR_Y - 16, GxEPD_BLUE);
        display.setTextColor(GxEPD_BLUE);
        display.setCursor(LC_X + 28, RAIN_SOLAR_Y);
        display.print("Rain continuing");

      } else if (!isnan(currentSolcastToday)) {
        // Dry — show solar forecast
        drawSmallIconTinted(display, si_solar_panel, LC_X, RAIN_SOLAR_Y - 16, GxEPD_BLACK);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(LC_X + 28, RAIN_SOLAR_Y);
        display.print("Expecting " + String(currentSolcastToday, 1) + "kWh today");
      }
      // If rainState==0 and no Solcast data, nothing is drawn — row stays blank.
    }

    // ================================================================
    // MINI 3-DAY FORECAST STRIP (left column, below detail row)
    // Three equal cells: day label | 32px icon | high temp
    // ================================================================
    {
      const int FCELL_W = (LC_W + LC_X) / FORECAST_DAYS;  // ~80px
      const int ICO = MINI_ICON_S;

      for (int i = 0; i < FORECAST_DAYS; i++) {
        ForecastDay& fc = currentForecast[i];
        if (!fc.valid) continue;

        int cx  = LC_X + i * FCELL_W;
        int fy  = MINI_FC_Y;

        // Day label — bold 9pt, centred
        display.setFont(&AtkinsonHyperlegible_Bold9pt7b);
        display.setTextSize(1);
        display.setTextColor(GxEPD_BLACK);
        {
          int16_t lbx,lby; uint16_t lbw,lbh;
          display.getTextBounds(fc.dayLabel,0,0,&lbx,&lby,&lbw,&lbh);
          display.setCursor(cx + (FCELL_W - lbw)/2, fy + 11);
          display.print(fc.dayLabel);
        }

        // Mini icon — centred
        {
          const uint8_t* bmp = miniWeatherIcon(fc.condition);
          if (bmp) drawMiniWeatherIcon(display, bmp,
                     cx + (FCELL_W - ICO)/2, fy + 14);
        }

        // High temp — black, centred, below icon
        {
          String hi = fc.high + "°";
          display.setFont(&AtkinsonHyperlegible_Bold9pt7b);
          display.setTextSize(1);
          display.setTextColor(GxEPD_BLACK);
          int16_t lbx,lby; uint16_t lbw,lbh;
          display.getTextBounds(hi,0,0,&lbx,&lby,&lbw,&lbh);
          display.setCursor(cx + (FCELL_W - lbw)/2, fy + 14 + ICO + 12);
          display.print(hi);
        }
      }
    }


    // ================================================================
    // ENERGY GRID  — 2 x 2
    // Order: Solar(TL) | Load(TR) / Grid Import(BL) | Grid Export(BR)
    // Entity index: [0]=GridExport [1]=GridImport [2]=Batt [3]=Solar [4]=Load
    // Icon color: BLACK when <1kWh, accent color when >=1kWh
    // Number color: always BLACK
    // ================================================================
    ey = ENERGY_Y;
    {
      String vSolar  = currentValues[3];
      String vLoad   = currentValues[4];
      String vImport = currentValues[1];
      String vExport = currentValues[0];

      int cellL = LC_X;                      // left cell start x
      int cellR = LC_X + ENERGY_CELL_W;      // right cell start x
      int rowGap = ENERGY_ICON_H + 13 + 14;  // icon + label + gap between rows

      // Row 1: Solar (left) | Load (right)
      // Active (>=1kWh): di_solar has baked yellow+2px black outline.
      // Inactive: di_solar_black is plain black — no yellow shown.
      {
        float sv = vSolar.toFloat();
        drawEnergyCell(sv >= 1.0f ? di_solar : di_solar_black,
                       vSolar, GxEPD_BLACK, cellL, ey);
      }
      drawEnergyCell(di_load,        vLoad,   energyColor(vLoad,   GxEPD_BLUE),   cellR, ey);
      ey += rowGap;
      // Row 2: Grid Import (left) | Grid Export (right)
      drawEnergyCell(di_grid_import, vImport, energyColor(vImport, GxEPD_RED),    cellL, ey);
      drawEnergyCell(di_grid_export, vExport, energyColor(vExport, GxEPD_GREEN),  cellR, ey);
    }

    // ================================================================
    // BATTERY STATUS
    // ================================================================
    ey = BATT_Y;

    // Derive state color and words
    uint16_t stateCol = GxEPD_BLACK;
    String stateWord  = "Idle";
    String etaWord    = "";
    if (currentBattStatus.startsWith("Charging"))         stateCol = GxEPD_GREEN;
    else if (currentBattStatus.startsWith("Discharging")) stateCol = GxEPD_RED;
    {
      int pipe = currentBattStatus.indexOf(" | ");
      if (pipe > 0) {
        stateWord = currentBattStatus.substring(0, pipe);
        etaWord   = currentBattStatus.substring(pipe + 3);
      } else {
        stateWord = currentBattStatus;
      }
    }

    // Battery bar x-coordinates (icon and text align to bar edges)
    int barLeft  = LC_X;
    int barRight = LC_X + BAR_W;

    // Battery icon — left-aligned to bar left edge, raised to align with text top
    drawSmallIcon(display, si_battery, barLeft, ey - 4);

    // SOC % — 12pt bold, next to icon, raised to match
    display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
    display.setTextSize(1);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(barLeft + 27, ey + 16);
    display.print(currentValues[2]);

    // State word — 9pt bold, right-aligned to bar right edge, moved up slightly
    display.setFont(&AtkinsonHyperlegible_Bold9pt7b);
    display.setTextSize(1);
    display.setTextColor(stateCol);
    printRight(stateWord, barRight, ey + 8);

    // ETA — 9pt regular, right-aligned to bar right edge
    if (etaWord.length()) {
      display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
      display.setTextSize(1);
      display.setTextColor(GxEPD_BLACK);
      printRight(etaWord, barRight, ey + 22);
    }

    // ---- Battery bar — shaped like a real battery ----
    {
      float soc = 0;
      String socStr = currentValues[2];
      int sp = socStr.indexOf(' ');
      if (sp > 0) soc = socStr.substring(0, sp).toFloat();
      else soc = socStr.toFloat();

      int bx = barLeft;
      int by = BAR_Y;
      int bw = BAR_W;
      int bh = BAR_H;
      int nw = BAR_NUB_W;
      int nh = BAR_NUB_H;

      // Main battery body outline
      display.drawRect(bx, by, bw, bh, GxEPD_BLACK);

      // White interior — creates the 2px white gap between outline and fill
      display.fillRect(bx + 1, by + 1, bw - 2, bh - 2, GxEPD_WHITE);

      // Positive terminal nub (right end, centred vertically)
      int nubX = bx + bw;
      int nubY = by + (bh - nh) / 2;
      display.drawRect(nubX, nubY, nw, nh, GxEPD_BLACK);
      display.fillRect(nubX + 1, nubY + 1, nw - 2, nh - 2, GxEPD_BLACK);

      // Colored fill — inset 3px (1px outline + 2px white border)
      int fillW = (int)((bw - 6) * soc / 100.0f);
      if (fillW > 0)
        display.fillRect(bx + 3, by + 3, fillW, bh - 6, stateCol);
    }

    // ================================================================
    // RIGHT COLUMN STATUS BAR (y=0 to RC_STATUS_H=50)
    // 9 icons evenly distributed across 540px right column.
    // Spacing: 14px lead + i*(32px icon + 27px gap) centres them nicely.
    // ================================================================
    {
      const int ICON_Y = (RC_STATUS_H - 32) / 2;  // 9px — centres 32px in 50px
      const int LEAD   = 14;
      const int STEP   = 59;   // 32px icon + 27px gap

      #define DRAW_SB(idx, bmp, col) \
        drawStatusIcon(display, bmp, RC_X + LEAD + (idx)*STEP, ICON_Y, col);

      // 1. Alarm / security — wired
      {
        const uint8_t* bmp = sb_shield_check;
        uint16_t col = GxEPD_BLACK;
        if (currentAlarmState == "disarmed") {
          bmp = sb_shield_check; col = GxEPD_GREEN;
        } else if (currentAlarmState.indexOf("partly") >= 0 ||
                   currentAlarmState.startsWith("partset")) {
          bmp = sb_shield_alert; col = GxEPD_RED;
        } else if (currentAlarmState.startsWith("armed")) {
          bmp = sb_shield_alert; col = GxEPD_RED;
        }
        DRAW_SB(0, bmp, col);
      }

      // 2. Door — red if any door open
      {
        bool anyOpen = currentFrontDoor || currentBackDoor;
        DRAW_SB(1, anyOpen ? sb_door_open : sb_door_closed,
                   anyOpen ? GxEPD_RED : GxEPD_BLACK);
      }
      // 3. Window — red if any window open
      {
        bool anyOpen = (currentWindows != 0);
        DRAW_SB(2, anyOpen ? sb_window_open : sb_window_closed,
                   anyOpen ? GxEPD_RED : GxEPD_BLACK);
      }
      // 4. Heating — red if state is heat/auto AND active_member_count > 0
      {
        bool heatingOn = (currentHeatingState == "heat" || currentHeatingState == "auto")
                         && currentHeatingActive > 0;
        DRAW_SB(3, heatingOn ? sb_radiator : sb_radiator_off,
                   heatingOn ? GxEPD_RED : GxEPD_BLACK);
      }
      // 5. Hot water — red if operation_mode == "heating"
      {
        bool waterOn = (currentWaterOp == "heating");
        DRAW_SB(4, waterOn ? sb_water_boiler : sb_water_boiler_off,
                   waterOn ? GxEPD_RED : GxEPD_BLACK);
      }
      // 6. Plants — red when any plant needs watering (list non-empty)
      {
        bool dry = (currentSoilStatus.length() > 0);
        DRAW_SB(5, sb_flower, dry ? GxEPD_RED : GxEPD_BLACK);
      }
      // 7. Vacuum 1 — green cleaning, red error, blue charging, else black
      {
        uint16_t col = GxEPD_BLACK;
        if (currentVac1 == "cleaning") col = GxEPD_GREEN;
        else if (currentVac1 == "error") col = GxEPD_RED;
        else if (currentVac1Charging && currentVac1Batt < 100) col = GxEPD_BLUE;
        DRAW_SB(6, sb_robot_vacuum, col);
      }
      // 8. Vacuum 2
      {
        uint16_t col = GxEPD_BLACK;
        if (currentVac2 == "cleaning") col = GxEPD_GREEN;
        else if (currentVac2 == "error") col = GxEPD_RED;
        else if (currentVac2Charging && currentVac2Batt < 100) col = GxEPD_BLUE;
        DRAW_SB(7, sb_robot_vacuum_variant, col);
      }
      // 9. Robot mower — green mowing, red error, blue charging, else black
      {
        uint16_t col = GxEPD_BLACK;
        if (currentMower == "mowing") col = GxEPD_GREEN;
        else if (currentMower == "error") col = GxEPD_RED;
        else if (currentMowerCharging && currentMowerBatt < 100) col = GxEPD_BLUE;
        DRAW_SB(8, sb_robot_mower, col);
      }

      #undef DRAW_SB
    }

    // ================================================================
    // RIGHT COLUMN — NOW section (grows dynamically below heading)
    // Each item: 40px tall, 5px gap, 24px icon centred vertically.
    // ================================================================
    int nowBottom;
    {
      const int ICON_W   = 24;
      const int NX       = RC_X + 4;
      const int TEXTX    = NX + ICON_W + 8;

      // Draw a two-line NOW item: 12pt colored top line, 9pt bottom line.
      // Returns nothing; caller advances itemTop.
      auto drawNowItem = [&](int top, const uint8_t* icon, uint16_t col,
                             const String& line1, const String& line2) {
        display.fillRect(RC_BAR_X, top, RC_BAR_W, ITEM_H, col);
        int iconY = top + (ITEM_H - ICON_W) / 2;
        drawStatusIconSm(display, icon, NX + 2, iconY, col);
        // Top line — 12pt bold, colored
        display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
        display.setTextSize(1);
        display.setTextColor(col);
        display.setCursor(TEXTX, top + 17);
        display.print(line1);
        // Bottom line — 9pt regular, black (may be blank)
        if (line2.length()) {
          display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
          display.setTextColor(GxEPD_BLACK);
          display.setCursor(TEXTX, top + 33);
          display.print(line2);
        }
      };

      // NOW heading
      display.setFont(&AtkinsonHyperlegible_Bold9pt7b);
      display.setTextSize(1);
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(RC_X + 8, RC_STATUS_H + 16);
      display.print("NOW");

      int itemTop = RC_STATUS_H + 24;

      // -- Alarm (always shown) — 2nd line is the last event message --
      {
        uint16_t col = GxEPD_BLACK;
        String line1 = "Alarm";
        const uint8_t* icon = sb_shield_check_sm;
        if (currentAlarmState == "disarmed") {
          col = GxEPD_GREEN; line1 = "Alarm disarmed"; icon = sb_shield_check_sm;
        } else if (currentAlarmState.indexOf("partly") >= 0 ||
                   currentAlarmState.startsWith("partset")) {
          col = GxEPD_RED; line1 = "Alarm part set"; icon = sb_shield_alert_sm;
        } else if (currentAlarmState.startsWith("armed")) {
          col = GxEPD_RED; line1 = "Alarm armed"; icon = sb_shield_alert_sm;
        }
        // Second line: last event message, capped so it fits the 9pt line
        String line2 = currentAlarmEvent;
        if (line2.length() > ALARM_EVENT_MAX_CHARS)
          line2 = line2.substring(0, ALARM_EVENT_MAX_CHARS - 1) + "...";
        drawNowItem(itemTop, icon, col, line1, line2);
        itemTop += ITEM_H + ITEM_GAP;
      }

      // -- Heating — only when at least one zone is genuinely calling for heat
      // (i.e. setpoint > current by more than 0.5°C). The aggregate's
      // active_member_count is just configured-member count, so we use the
      // per-zone calling count computed in fetchZones().
      if (heatCallingCount > 0) {
        drawNowItem(itemTop, sb_radiator_sm, GxEPD_RED,
                    "Heating",
                    String(heatCallingCount) + " zones heating");
        itemTop += ITEM_H + ITEM_GAP;
      }

      // -- Hot water — only when heating --
      {
        bool waterOn = (currentWaterOp == "heating");
        if (waterOn) {
          drawNowItem(itemTop, sb_water_boiler_sm, GxEPD_RED,
                      "Hot water",
                      String(currentWaterTemp) + "C / " + currentWaterTarget + "C target");
          itemTop += ITEM_H + ITEM_GAP;
        }
      }

      // -- Doors — only when open --
      {
        if (currentFrontDoor || currentBackDoor) {
          String line2;
          if (currentFrontDoor && currentBackDoor) line2 = "Front & back";
          else if (currentFrontDoor)               line2 = "Front";
          else                                     line2 = "Back";
          drawNowItem(itemTop, sb_door_open_sm, GxEPD_RED, "Door open", line2);
          itemTop += ITEM_H + ITEM_GAP;
        }
      }

      // -- Windows — only when open --
      if (currentWindows != 0) {
        String openList = "";
        int openCount = 0;
        for (int i = 0; i < WINDOW_COUNT; i++) {
          if (currentWindows & (1 << i)) {
            if (openList.length()) openList += ", ";
            openList += WINDOW_LABELS[i];
            openCount++;
          }
        }
        String line1 = (openCount > 1) ? (String(openCount) + " windows open") : "Window open";
        // Bottom line: the list, capped so it can't overflow
        if (openList.length() > 72) openList = openList.substring(0, 69) + "...";
        drawNowItem(itemTop, sb_window_open_sm, GxEPD_RED, line1, openList);
        itemTop += ITEM_H + ITEM_GAP;
      }

      // -- Plant needs watering — shown in blue when soil is dry --
      if (currentSoilStatus.length() > 0) {
        // currentSoilStatus is a comma-separated list of dry plant labels
        String plantLine1 = (currentSoilStatus.indexOf(",") >= 0)
                            ? "Plants need water" : "Plant needs water";
        drawNowItem(itemTop, si_watering_can, GxEPD_BLUE,
                    plantLine1,
                    currentSoilStatus);
        itemTop += ITEM_H + ITEM_GAP;
      }

      // -- Robots actively working — show battery so you know they'll finish --
      if (currentVac1 == "cleaning") {
        drawNowItem(itemTop, sb_robot_vacuum_sm, GxEPD_GREEN,
                    "Vacuum 1 cleaning", String(currentVac1Batt) + "% battery");
        itemTop += ITEM_H + ITEM_GAP;
      }
      if (currentVac2 == "cleaning") {
        drawNowItem(itemTop, sb_robot_vacuum_sm, GxEPD_GREEN,
                    "Vacuum 2 cleaning", String(currentVac2Batt) + "% battery");
        itemTop += ITEM_H + ITEM_GAP;
      }
      if (currentMower == "mowing") {
        drawNowItem(itemTop, sb_robot_mower_sm, GxEPD_GREEN,
                    "Mower active", String(currentMowerBatt) + "% battery");
        itemTop += ITEM_H + ITEM_GAP;
      }

      // Whitespace gap before TODAY
      nowBottom = itemTop + 10;
    }

    // ================================================================
    // RIGHT COLUMN — TODAY heading (positioned just below NOW)
    // ================================================================
    {
      display.setFont(&AtkinsonHyperlegible_Bold9pt7b);
      display.setTextSize(1);
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(RC_X + 8, nowBottom + 16);
      display.print("TODAY");
      // Dotted underline — matches the NOW/status bar divider style
      //for (int x = RC_X; x < PANEL_W; x += 7)
        //display.drawFastHLine(x, nowBottom + 22, 4, GxEPD_BLACK);
    }

    // ================================================================
    // RIGHT COLUMN — Calendar entries with colored left bar
    // ================================================================
    {
      static const uint16_t CAL_COLORS[] = {
        GxEPD_BLACK, GxEPD_BLUE, GxEPD_RED, GxEPD_GREEN
      };

      const int MAX_Y  = FOOTER_Y - FOOTER_GAP;
      int y = nowBottom + TODAY_GAP;  // heading (16px) + descent + 6px padding

      if (currentEventCount == 0) {
        display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
        display.setTextSize(1);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(RC_TEXT_X, y + 10);
        display.print("No events today");
      } else {
        for (int i = 0; i < currentEventCount && y <= MAX_Y; i++) {
          CalEvent& ce = currentEvents[i];
          if (!ce.valid) continue;

          uint16_t col = CAL_COLORS[ce.colorIdx < 4 ? ce.colorIdx : 0];

          // Colored left bar spans full row height
          display.fillRect(RC_BAR_X, y, RC_BAR_W, RC_LINE_H, col);

          // Top line — 12pt bold colored: time + summary
          display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
          display.setTextSize(1);
          display.setTextColor(col);
          display.setCursor(RC_TEXT_X, y + 17);
          int summaryBudget = CAL_SUMMARY_MAX_CHARS;  // total chars for the line
          if (ce.time.length() > 0) {
            display.print(ce.time);
            display.print(" ");
            summaryBudget -= (ce.time.length() + 1);  // time + space
          } else {
            display.print("· ");
            summaryBudget -= 2;
          }
          // Cursor is now at the start of the title text — remember this x so the
          // description on line 2 lines up under the title, not the time.
          int titleX = display.getCursorX();
          String summ = ce.summary;
          if ((int)summ.length() > summaryBudget && summaryBudget > 3)
            summ = summ.substring(0, summaryBudget - 1) + "...";
          display.print(summ);

          // Bottom line — 9pt regular black: description first words (may be blank)
          // Aligned under the title text (titleX), not the time prefix.
          if (ce.description.length()) {
            display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
            display.setTextColor(GxEPD_BLACK);
            display.setCursor(titleX, y + 33);
            String desc = ce.description;
            if (desc.length() > 84) desc = desc.substring(0, 81) + "...";
            display.print(desc);
          }

          y += RC_LINE_H + 5;
        }
      }
    }

    // ================================================================
    // FOOTER
    // ================================================================
    // Battery: tinted icon + percentage at far right; time to the left of it.
    {
      uint16_t batCol = batteryColor(bpct);
      String pctStr = String(bpct) + "%";

      display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
      display.setTextSize(1);

      // Measure percentage text
      int16_t bx,by; uint16_t bw,bh;
      display.getTextBounds(pctStr, 0, 0, &bx, &by, &bw, &bh);

      // Layout from the right edge: [icon 24px][2px][pct text] ending at x=788
      int pctX  = PANEL_W - 12 - bw;
      int iconX = pctX - 24 - 2;
      int iconY = FOOTER_ICON_Y;   // align 24px icon with text baseline

      drawSmallIconTinted(display, si_dev_battery, iconX, iconY, batCol);
      display.setTextColor(batCol);
      display.setCursor(pctX, FOOTER_Y);
      display.print(pctStr);

      // Time, right-aligned just left of the battery icon, with a small
      // refresh icon immediately to its left to mark it as the update time.
      display.getTextBounds(footer, 0, 0, &bx, &by, &bw, &bh);
      int timeX = iconX - FOOTER_GAP - bw;
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(timeX, FOOTER_Y);
      display.print(footer);

      // Refresh icon (16px) just left of the time text
      int refX = timeX - 16 - 4;
      int refY = FOOTER_TINY_Y;   // align 16px icon with text baseline
      drawTinyIcon(display, si_refresh, refX, refY, GxEPD_BLACK);

      // Quiet-mode indicator — bed-clock icon shown only during quiet hours
      // so you can see at a glance that the device is in low-refresh night mode.
      if (QUIET_ENABLED) {
        struct tm tmNow;
        if (getLocalTime(&tmNow, 0)) {
          int h = tmNow.tm_hour;
          bool quiet = (QUIET_START_HOUR < QUIET_END_HOUR)
            ? (h >= QUIET_START_HOUR && h < QUIET_END_HOUR)
            : (h >= QUIET_START_HOUR || h < QUIET_END_HOUR);
          if (quiet)
            drawTinyIcon(display, si_bed_clock, refX - 16 - 4, refY, GxEPD_BLUE);
        }
      }
    }

#ifdef DEBUG_GRID
    // ---- Coordinate grid overlay ----
    // Vertical lines every 20px; solid + labelled every 100px.
    for (int x = 0; x <= PANEL_W; x += 20) {
      if (x % 100 == 0) {
        // Solid line
        display.drawFastVLine(x, 0, PANEL_H, GxEPD_RED);
        // Label at top and bottom
        display.setFont(nullptr);   // tiny built-in font
        display.setTextColor(GxEPD_RED);
        display.setCursor(x + 2, 2);
        display.print(x);
        display.setCursor(x + 2, PANEL_H - 14);
        display.print(x);
      } else {
        // Dashed line
        for (int y = 0; y < PANEL_H; y += 6)
          display.drawFastVLine(x, y, 3, GxEPD_RED);
      }
    }
    // Horizontal lines every 20px; solid + labelled every 100px.
    for (int y = 0; y <= PANEL_H; y += 20) {
      if (y % 100 == 0) {
        display.drawFastHLine(0, y, PANEL_W, GxEPD_RED);
        display.setFont(nullptr);
        display.setTextColor(GxEPD_RED);
        display.setCursor(2, y + 2);
        display.print(y);
        display.setCursor(PANEL_W - 38, y + 2);
        display.print(y);
      } else {
        for (int x = 0; x < PANEL_W; x += 6)
          display.drawFastHLine(x, y, 3, GxEPD_RED);
      }
    }
#endif

  } while (display.nextPage());
  display.hibernate();
}