// screen_security.cpp — security overview page (Key 2, on-demand only).
//
// Layout (800x480):
//   LEFT   (0..239):   Alarm badge (big shield + state + since HH:MM)
//                      Summary counts: doors, windows, motion, cameras
//   MIDDLE (248..527): DOORS section then WINDOWS section
//                      Each row: colored left bar + icon + name + state + time
//   RIGHT  (536..799): MOTION section then CAMERAS section
//                      Same row format, showing last-triggered time
//
// Fetching is done inline (no shared globals). Timer wakes never call this.
#include <ArduinoJson.h>
#include "shared_state.h"
#include "layout.h"
#include "security_icons.h"
#include "small_icons.h"    // footer icons

// ---- Fetch helpers ----
// Returns state string and last_changed ISO string for a binary_sensor or
// alarm_control_panel entity. Returns false if the fetch fails.
static bool secFetch(const String& entityId, String& stateOut, String& changedOut) {
  stateOut = ""; changedOut = "";
  String body;
  if (haGet(entityId, body) != 200) return false;
  JsonDocument doc;
  JsonDocument filter;
  filter["state"] = true;
  filter["last_changed"] = true;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;
  stateOut   = doc["state"].as<String>();
  changedOut = doc["last_changed"].as<String>();
  return true;
}

// ---- Layout constants (hoisted above do{} — flat-scope compiler) ----
// Column X positions
static const int SC_LEFT_W   = 240;   // left panel width
static const int SC_MID_X    = 248;   // middle panel left edge
static const int SC_MID_W    = 278;   // middle panel width
static const int SC_RIGHT_X  = 534;   // right panel left edge
static const int SC_RIGHT_W  = PANEL_W - SC_RIGHT_X;

// Row geometry
static const int SC_ROW_H    = 28;    // height per sensor row
static const int SC_ICON_W   = 20;    // sensor icon size
static const int SC_BAR_W    = 4;     // left colored bar width

// Section heading Y positions — computed once, used in both do-loop pass and
// the fetch phase.
static const int SC_DOOR_Y   = 60;
static const int SC_WIN_Y    = SC_DOOR_Y + 2*SC_ROW_H + 30 + 18;  // 2 doors + gap
static const int SC_MOT_Y    = 60;
static const int SC_CAM_Y    = SC_MOT_Y + 5*SC_ROW_H + 30 + 18;   // 5 motions + gap

// ---- Row draw helper: colored left bar + small icon + label + state + time ----
// called inside the do{} loop
template<typename TDisplay>
static void drawSecRow(TDisplay& display,
                       int panelX, int y,
                       const uint8_t* icon, const char* label,
                       bool isOpen, const String& timeStr) {
  uint16_t col = isOpen ? GxEPD_RED : GxEPD_GREEN;
  // Left bar
  display.fillRect(panelX, y + 2, SC_BAR_W, SC_ROW_H - 4, col);
  // Icon
  drawSecIcon(display, icon, panelX + SC_BAR_W + 4, y + 4, 20, col);
  // Label
  display.setFont(&AtkinsonHyperlegible_Bold9pt7b);
  display.setTextSize(1);
  display.setTextColor(col);
  display.setCursor(panelX + SC_BAR_W + SC_ICON_W + 8, y + 18);
  display.print(label);
  // Time right-aligned
  if (timeStr.length()) {
    display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
    display.setTextColor(GxEPD_BLACK);
    int16_t bx,by; uint16_t bw,bh;
    display.getTextBounds(timeStr, 0, 0, &bx,&by,&bw,&bh);
    display.setCursor(panelX + (panelX == SC_MID_X ? SC_MID_W : SC_RIGHT_W) - bw - 4, y + 18);
    display.print(timeStr);
  }
}

// ---- Section heading ----
template<typename TDisplay>
static void drawSecHeading(TDisplay& display, int x, int y, const char* text) {
  display.setFont(&AtkinsonHyperlegible_Bold9pt7b);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(x, y);
  display.print(text);
  // Underline
  int16_t bx,by; uint16_t bw,bh;
  display.getTextBounds(text, 0, 0, &bx,&by,&bw,&bh);
  display.drawFastHLine(x, y + 4, bw + 2, GxEPD_BLACK);
}

void drawSecurityPage() {
  // ================================================================
  // FETCH PHASE — gather all data before starting the display loop
  // ================================================================

  // Alarm panel
  String alarmState, alarmChanged;
  secFetch(SEC_ALARM_ENTITY, alarmState, alarmChanged);
  String alarmSince = isoUtcToLocalHHMM(alarmChanged);
  bool disarmed = (alarmState == "disarmed");

  // Doors
  String doorState[SEC_DOOR_COUNT], doorTime[SEC_DOOR_COUNT];
  int doorsOpen = 0;
  for (int i = 0; i < SEC_DOOR_COUNT; i++) {
    secFetch(SEC_DOORS[i].entity, doorState[i], doorTime[i]);
    doorTime[i] = isoUtcToLocalHHMM(doorTime[i]);
    if (doorState[i] == "on") doorsOpen++;
  }

  // Windows
  String winState[SEC_WINDOW_COUNT], winTime[SEC_WINDOW_COUNT];
  int winsOpen = 0;
  for (int i = 0; i < SEC_WINDOW_COUNT; i++) {
    secFetch(SEC_WINDOWS[i].entity, winState[i], winTime[i]);
    winTime[i] = isoUtcToLocalHHMM(winTime[i]);
    if (winState[i] == "on") winsOpen++;
  }

  // Motion sensors
  String motState[SEC_MOTION_COUNT], motTime[SEC_MOTION_COUNT];
  for (int i = 0; i < SEC_MOTION_COUNT; i++) {
    secFetch(SEC_MOTIONS[i].entity, motState[i], motTime[i]);
    motTime[i] = isoUtcToLocalHHMM(motTime[i]);
  }

  // Cameras
  String camState[SEC_CAMERA_COUNT], camTime[SEC_CAMERA_COUNT];
  for (int i = 0; i < SEC_CAMERA_COUNT; i++) {
    secFetch(SEC_CAMERAS[i].motion_entity, camState[i], camTime[i]);
    camTime[i] = isoUtcToLocalHHMM(camTime[i]);
  }

  // ================================================================
  // DRAW PHASE
  // ================================================================
  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // ---- LEFT PANEL: Alarm badge ----
    {
      // Flood the badge area green/red
      uint16_t badgeCol = disarmed ? GxEPD_GREEN : GxEPD_RED;
      display.fillRect(0, 0, SC_LEFT_W, 100, badgeCol);

      // Shield icon (white)
      const uint8_t* shield = disarmed ? sec_shield_check : sec_shield_alert;
      drawSecIcon(display, shield, 12, 18, 64, GxEPD_WHITE);

      // State text
      display.setFont(&AtkinsonHyperlegible_Bold12pt7b);
      display.setTextSize(1);
      display.setTextColor(GxEPD_WHITE);
      display.setCursor(86, 46);
      display.print(disarmed ? "DISARMED" : "ARMED");

      // Since time
      display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
      display.setCursor(86, 68);
      display.print("since " + alarmSince);
    }

    // Summary counts
    {
      const int SX = 12;
      int sy = 116;
      const int SH = 30;

      auto drawSummaryRow = [&](const uint8_t* icon, const char* text, uint16_t col) {
        drawSecIcon(display, icon, SX, sy, 20, col);
        display.setFont(&AtkinsonHyperlegible_Regular9pt7b);
        display.setTextSize(1);
        display.setTextColor(col);
        display.setCursor(SX + 26, sy + 15);
        display.print(text);
        sy += SH;
      };

      // Doors
      String dTxt = String(SEC_DOOR_COUNT) + " doors — " +
                    (doorsOpen == 0 ? "all closed" : String(doorsOpen) + " open");
      drawSummaryRow(sec_door_sm, dTxt.c_str(), doorsOpen > 0 ? GxEPD_RED : GxEPD_BLACK);

      // Windows
      String wTxt = String(winsOpen) + " window" + (winsOpen == 1 ? "" : "s") +
                    (winsOpen == 0 ? " — all closed" : " open");
      drawSummaryRow(sec_window_sm, wTxt.c_str(), winsOpen > 0 ? GxEPD_RED : GxEPD_BLACK);

      // Motion
      String mTxt = String(SEC_MOTION_COUNT) + " motion sensors";
      drawSummaryRow(sec_motion_sm, mTxt.c_str(), GxEPD_BLACK);

      // Cameras
      String cTxt = String(SEC_CAMERA_COUNT) + " cameras";
      drawSummaryRow(sec_camera_sm, cTxt.c_str(), GxEPD_BLACK);
    }

    // ---- MIDDLE PANEL: Doors then Windows ----
    {
      // Divider line
      display.drawFastVLine(SC_MID_X - 4, 0, PANEL_H, GxEPD_BLACK);

      // DOORS
      drawSecHeading(display, SC_MID_X, SC_DOOR_Y - 16, "DOORS");
      for (int i = 0; i < SEC_DOOR_COUNT; i++) {
        bool open = (doorState[i] == "on");
        drawSecRow(display, SC_MID_X,
                   SC_DOOR_Y + i * SC_ROW_H,
                   sec_door_sm, SEC_DOORS[i].label,
                   open, doorTime[i]);
      }

      // WINDOWS
      int winStartY = SC_DOOR_Y + SEC_DOOR_COUNT * SC_ROW_H + 28;
      drawSecHeading(display, SC_MID_X, winStartY - 16, "WINDOWS");
      for (int i = 0; i < SEC_WINDOW_COUNT; i++) {
        bool open = (winState[i] == "on");
        drawSecRow(display, SC_MID_X,
                   winStartY + i * SC_ROW_H,
                   sec_window_sm, SEC_WINDOWS[i].label,
                   open, winTime[i]);
      }
    }

    // ---- RIGHT PANEL: Motion then Cameras ----
    {
      // Divider line
      display.drawFastVLine(SC_RIGHT_X - 4, 0, PANEL_H, GxEPD_BLACK);

      // MOTION
      drawSecHeading(display, SC_RIGHT_X, SC_MOT_Y - 16, "MOTION");
      for (int i = 0; i < SEC_MOTION_COUNT; i++) {
        bool active = (motState[i] == "on");
        drawSecRow(display, SC_RIGHT_X,
                   SC_MOT_Y + i * SC_ROW_H,
                   sec_motion_sm, SEC_MOTIONS[i].label,
                   active, motTime[i]);
      }

      // CAMERAS
      int camStartY = SC_MOT_Y + SEC_MOTION_COUNT * SC_ROW_H + 28;
      drawSecHeading(display, SC_RIGHT_X, camStartY - 16, "CAMERAS");
      for (int i = 0; i < SEC_CAMERA_COUNT; i++) {
        bool active = (camState[i] == "on");
        drawSecRow(display, SC_RIGHT_X,
                   camStartY + i * SC_ROW_H,
                   sec_camera_sm, SEC_CAMERAS[i].label,
                   active, camTime[i]);
      }
    }

    // ---- Standard footer ----
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
      drawSmallIconTinted(display, si_dev_battery, iconX, FOOTER_ICON_Y, batCol);
      display.setTextColor(batCol);
      display.setCursor(pctX, FOOTER_Y);
      display.print(pctStr);

      display.getTextBounds(footer, 0, 0, &bx,&by,&bw,&bh);
      int timeX = iconX - FOOTER_GAP - bw;
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(timeX, FOOTER_Y);
      display.print(footer);
      drawTinyIcon(display, si_refresh, timeX - 20, FOOTER_TINY_Y, GxEPD_BLACK);
    }

  } while (display.nextPage());
  display.hibernate();
}