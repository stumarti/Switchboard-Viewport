// shared_state.h — shared declarations across main.cpp and the screen_*.cpp files.
//
// main.cpp DEFINES these globals (and #defines SHARED_STATE_DEFINE before including
// this header). Every other translation unit (screen_status.cpp, etc.) includes
// this header to get `extern` declarations, so they can reference the same objects.
//
// This is what lets each screen be a real, independently-compiled .cpp file and
// keeps IntelliSense happy.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <GxEPD2_7C.h>
#include "config.h"
#include "AtkinsonHyperlegible_Regular9pt7b.h"
#include "AtkinsonHyperlegible_Bold9pt7b.h"
#include "AtkinsonHyperlegible_Bold12pt7b.h"
#include "AtkinsonHyperlegible_Bold42pt7b.h"

// ---- Display type (must match main.cpp's definition) ----
#define GxEPD2_DRIVER_CLASS GxEPD2_730c_GDEP073E01
#define MAX_DISPLAY_BUFFER_SIZE 16000
#define MAX_HEIGHT(EPD) \
    (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) \
         ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))
typedef GxEPD2_7C<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> DisplayType;

extern DisplayType display;

// ---- Shared data structures ----
struct Weather {
  String condition;
  String tempNum;
  String tempUnit;
  String humidity;
  String windSpeed;
  String windUnit;
  float  windBearing;
  String uvIndex;
  bool   valid = false;
};

struct ForecastDay {
  String condition;
  String dayLabel;
  String high;
  String low;
  String precip;
  bool   valid = false;
};

struct CalEvent {
  String  time;
  String  summary;
  String  description;
  uint8_t colorIdx;
  bool    valid = false;
};

struct ZoneData {
  String  label;
  float   current = NAN;
  float   target  = NAN;
  bool    active  = false;
  bool    valid   = false;
};

// ---- Current-wake state (defined in main.cpp) ----
extern Weather     currentWx;
extern ForecastDay currentForecast[FORECAST_DAYS];
extern CalEvent    currentEvents[MAX_CAL_EVENTS];
extern int         currentEventCount;

extern String  currentValues[8];
extern float   currentNums[8];
extern bool    currentIsNum[8];
extern String  currentBattStatus;
extern String  currentAlarmState;
extern String  currentAlarmEvent;
extern String  currentHeatingState;
extern int     currentHeatingActive;
extern float   currentHeatingTemp;
extern float   currentHeatingTarget;
extern float   currentSolcastToday;
extern int     currentRainState;
extern String  currentRainTime;
extern String  currentWaterOp;
extern int     currentWaterTemp;
extern int     currentWaterTarget;
extern bool    currentFrontDoor;
extern bool    currentBackDoor;
extern uint8_t currentWindows;
extern String  currentSoilStatus;  // comma-separated dry plant labels, "" if all fine
extern String  currentVac1;
extern int     currentVac1Batt;
extern bool    currentVac1Charging;
extern String  currentVac2;
extern int     currentVac2Batt;
extern bool    currentVac2Charging;
extern String  currentMower;
extern int     currentMowerBatt;
extern bool    currentMowerCharging;

extern ZoneData heatZones[16];
extern int      heatZoneCount;
extern int      heatCallingCount;
extern bool     heatAnyActive;

// HTTP status of the most recent haGet call (for the error screen).
extern int g_lastHttpCode;

// ---- Shared helper functions (defined in main.cpp) ----
String   currentTimeStr();
float    readBatteryVoltage();
int      batteryPercent(float v);
uint16_t batteryColor(int pct);
void     printRight(const String& s, int rightEdge, int y);
String   isoUtcToLocalHHMM(const String& iso);
int      haGet(const String& entityId, String& bodyOut);
int      haPost(const String& entityId, const String& body);

// ---- Screen entry points (each defined in its own screen_*.cpp) ----
void drawDashboard();
void drawHeatingPage();
void drawChargeScreen(int pct);
void drawErrorScreen(const char* title, const char* detail);
void drawSecurityPage();
void drawHelpScreen();