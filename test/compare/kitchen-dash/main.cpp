#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <GxEPD2_7C.h>
#include <Wire.h>
#include <SensirionI2cSht4x.h>
#include "splash_screen.h"
#include "config.h"
#include "layout.h"

// ============================================================================
// reTerminal E1002 — HA dashboard  (800x480 Spectra-6 e-paper)
// Deep-sleep build: wake → connect → fetch → draw if changed → sleep.
// Layout:
//   LEFT  (0..389):  weather block / energy grid / battery bar
//   DIVIDER (390):   vertical dashed line
//   RIGHT (400..799): reserved for future HA notifications panel
//   FOOTER:          time · battery  (bottom-right)
// ============================================================================

#define uS_PER_SEC 1000000ULL

// Compute how many minutes to sleep based on the current local hour.
// During quiet hours (QUIET_START_HOUR..QUIET_END_HOUR, wrapping midnight)
// returns NIGHT_SLEEP_MINUTES, otherwise SLEEP_MINUTES.
uint64_t computeSleepMinutes() {
  if (!QUIET_ENABLED) return SLEEP_MINUTES;
  struct tm tmNow;
  if (!getLocalTime(&tmNow, 100)) return SLEEP_MINUTES;  // time unknown — use default
  int h = tmNow.tm_hour;
  bool quiet;
  if (QUIET_START_HOUR < QUIET_END_HOUR) {
    // Same-day window (e.g. 1..5)
    quiet = (h >= QUIET_START_HOUR && h < QUIET_END_HOUR);
  } else {
    // Wraps midnight (e.g. 23..6): quiet if at/after start OR before end
    quiet = (h >= QUIET_START_HOUR || h < QUIET_END_HOUR);
  }
  return quiet ? NIGHT_SLEEP_MINUTES : SLEEP_MINUTES;
}

// ----- Display -----
#include "shared_state.h"

// The display object is DEFINED here (declared extern in shared_state.h).
DisplayType display(GxEPD2_DRIVER_CLASS(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));
SPIClass hspi(HSPI);
WiFiClient wifiClient;

// ============================================================================
// RTC memory — survives deep sleep, wiped on power-loss or first flash.
// ============================================================================
RTC_DATA_ATTR bool  rtcInitialized = false;
RTC_DATA_ATTR bool  rtcChargeScreenShown = false;
RTC_DATA_ATTR int   bootCount      = 0;

// Weather snapshot
RTC_DATA_ATTR char  rtcWxCond[MAX_STR]   = "";
RTC_DATA_ATTR char  rtcWxTemp[MAX_STR]   = "";
RTC_DATA_ATTR char  rtcWxHumidity[MAX_STR] = "";
RTC_DATA_ATTR char  rtcWxWind[MAX_STR]   = "";
RTC_DATA_ATTR char  rtcWxUV[MAX_STR]     = "";
RTC_DATA_ATTR float rtcWxBearing         = -1.0f;
RTC_DATA_ATTR bool  rtcWxValid           = false;

// Sensor snapshot
RTC_DATA_ATTR char  rtcSensorStr[8][MAX_STR];
RTC_DATA_ATTR float rtcSensorNum[8];
RTC_DATA_ATTR bool  rtcSensorIsNum[8];

// Battery status snapshot
RTC_DATA_ATTR char  rtcBattStatus[MAX_STR] = "";

// Status bar snapshots
RTC_DATA_ATTR char  rtcAlarmState[MAX_STR]   = "";
RTC_DATA_ATTR char  rtcAlarmEvent[MAX_STR]   = "";
RTC_DATA_ATTR char  rtcHeatingState[MAX_STR] = "";
RTC_DATA_ATTR char  rtcWaterState[MAX_STR]   = "";
RTC_DATA_ATTR int   rtcCallingCount          = -1;
RTC_DATA_ATTR bool  rtcFrontDoor             = false;
RTC_DATA_ATTR bool  rtcBackDoor              = false;
RTC_DATA_ATTR uint8_t rtcWindows             = 0;   // bitmask, 1 bit per window
RTC_DATA_ATTR char    rtcSoilStatus[MAX_STR] = "";
RTC_DATA_ATTR char    rtcVac1[MAX_STR]       = "";
RTC_DATA_ATTR char    rtcVac2[MAX_STR]       = "";
RTC_DATA_ATTR char    rtcMower[MAX_STR]      = "";
RTC_DATA_ATTR int     rtcVac1Batt  = -1;
RTC_DATA_ATTR int     rtcVac2Batt  = -1;
RTC_DATA_ATTR int     rtcMowerBatt = -1;
RTC_DATA_ATTR bool    rtcVac1Chg   = false;
RTC_DATA_ATTR bool    rtcVac2Chg   = false;
RTC_DATA_ATTR bool    rtcMowerChg  = false;

// Forecast snapshot (3 days)
// ForecastDay struct is defined in shared_state.h
ForecastDay currentForecast[FORECAST_DAYS];
RTC_DATA_ATTR char rtcFcCond[FORECAST_DAYS][MAX_STR];
RTC_DATA_ATTR char rtcFcHigh[FORECAST_DAYS][MAX_STR];
RTC_DATA_ATTR char rtcFcLow[FORECAST_DAYS][MAX_STR];

// Calendar events for today (across all calendars)
// CalEvent struct is defined in shared_state.h
CalEvent currentEvents[MAX_CAL_EVENTS];
int      currentEventCount = 0;
// RTC: simple hash of all summaries — detects any change
RTC_DATA_ATTR uint32_t rtcCalHash = 0;

// ============================================================================
// Current-wake values (plain RAM — rebuilt each wake)
// Weather struct is defined in shared_state.h
// ============================================================================
Weather currentWx;
String  currentValues[8];
float   currentNums[8];
bool    currentIsNum[8];
String  currentBattStatus;
String  currentAlarmState;
String  currentAlarmEvent;
String  currentHeatingState;
int     currentHeatingActive;
float   currentHeatingTemp   = NAN;   // aggregate current temp (whole_house)
float   currentHeatingTarget = NAN;   // aggregate setpoint
float   currentSolcastToday    = NAN; // Solcast forecast kWh today

// Rain state derived from hourly forecast
// rainEventTime: local HH:MM of next rain start or stop event
// rainState: 0=no rain in 12h, 1=currently raining (shows stop time),
//            2=rain coming (shows start time), 3=rain all day (no stop found)
int     currentRainState = 0;
String  currentRainTime  = "";
String  currentWaterOp;
int     currentWaterTemp;
int     currentWaterTarget;
bool    currentFrontDoor = false;
bool    currentBackDoor  = false;
uint8_t currentWindows   = 0;   // bitmask — bit i set means WINDOW_ENTITIES[i] is open
String  currentSoilStatus;      // comma-separated list of dry plant labels, or "" if all fine
String  currentVac1;            // first vacuum state
int     currentVac1Batt = 0;
bool    currentVac1Charging = false;
String  currentVac2;            // second vacuum state
int     currentVac2Batt = 0;
bool    currentVac2Charging = false;
String  currentMower;           // lawn mower state
int     currentMowerBatt = 0;
bool    currentMowerCharging = false;

void copyToBuf(char* dst, const String& s) {
  strncpy(dst, s.c_str(), MAX_STR - 1);
  dst[MAX_STR - 1] = '\0';
}

// ============================================================================
// LED
// ============================================================================
void setLed(bool on) { digitalWrite(LED_PIN, on ? LOW : HIGH); }

bool connectWiFi(unsigned long timeoutMs = 20000) {
  Serial1.printf("Connecting to: %s\n", WIFI_NAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NAME, WIFI_PASS);
  unsigned long start = millis();
  bool s = false;
  while (WiFi.status() != WL_CONNECTED) {
    s = !s; setLed(s); delay(100);
    if (millis() - start > timeoutMs) {
      Serial1.println("WiFi timeout.");
      setLed(false); return false;
    }
  }
  setLed(false);
  Serial1.print("Connected. IP: "); Serial1.println(WiFi.localIP());
  return true;
}

// ============================================================================
// Time (NTP)
// ============================================================================
void syncTime() {
  // Two NTP servers: local (Unraid/router) first, public pool as fallback.
  // configTzTime accepts up to 3 NTP servers and tries them in order.
  configTzTime(TZ_INFO, NTP_SERVER, NTP_FALLBACK_SERVER);
  struct tm tm;
  bool synced = false;
  for (int i = 0; i < 20 && !synced; i++) {
    if (getLocalTime(&tm, 250)) synced = true;
  }
  if (synced) {
    char buf[32]; strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    Serial1.printf("Time synced: %s\n", buf);
  } else {
    Serial1.println("Time sync failed — using stale RTC time.");
  }
}
String currentTimeStr() {
  struct tm tm;
  if (!getLocalTime(&tm, 100)) return "--:--";
  char buf[6]; strftime(buf, sizeof(buf), "%H:%M", &tm);
  return String(buf);
}

static time_t utcFieldsToEpoch(const struct tm& t) {
  int year = t.tm_year + 1900, mon = t.tm_mon + 1;
  if (mon <= 2) { year--; mon += 12; }
  long era = (year >= 0 ? year : year - 399) / 400;
  unsigned yoe = (unsigned)(year - era * 400);
  unsigned doy = (153*(mon-3)+2)/5 + t.tm_mday - 1;
  unsigned doe = yoe*365 + yoe/4 - yoe/100 + doy;
  long days = era*146097 + (long)doe - 719468;
  return (time_t)(days*86400L + t.tm_hour*3600L + t.tm_min*60L + t.tm_sec);
}
String isoUtcToLocalHHMM(const String& iso) {
  if (iso.length() < 19) return "";
  struct tm t = {0};
  t.tm_year = iso.substring(0,4).toInt()-1900;
  t.tm_mon  = iso.substring(5,7).toInt()-1;
  t.tm_mday = iso.substring(8,10).toInt();
  t.tm_hour = iso.substring(11,13).toInt();
  t.tm_min  = iso.substring(14,16).toInt();
  t.tm_sec  = iso.substring(17,19).toInt();
  if (t.tm_year < 100 || t.tm_mon < 0 || t.tm_mon > 11) return "";
  time_t utc = utcFieldsToEpoch(t);
  struct tm local; localtime_r(&utc, &local);
  char buf[6]; strftime(buf, sizeof(buf), "%H:%M", &local);
  return String(buf);
}

// ============================================================================
// Battery (device hardware)
// ============================================================================
float readBatteryVoltage() {
  pinMode(BATTERY_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_ENABLE_PIN, HIGH); delay(10);
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  int mv = analogReadMilliVolts(BATTERY_ADC_PIN);
  digitalWrite(BATTERY_ENABLE_PIN, LOW);
  return (mv / 1000.0f) * 2.0f;
}
int batteryPercent(float v) {
  static const float volts[] = {4.15f,3.96f,3.91f,3.85f,3.80f,3.75f,
                                 3.68f,3.58f,3.49f,3.41f,3.30f,3.27f};
  static const float pcts[]  = {100.f,90.f,80.f,70.f,60.f,50.f,
                                  40.f,30.f,20.f,10.f,5.f,0.f};
  const int n = sizeof(volts)/sizeof(volts[0]);
  if (v >= volts[0]) return 100;
  if (v <= volts[n-1]) return 0;
  for (int i = 0; i < n-1; i++) {
    if (v <= volts[i] && v > volts[i+1]) {
      float frac = (v-volts[i+1])/(volts[i]-volts[i+1]);
      float p = pcts[i+1] + frac*(pcts[i]-pcts[i+1]);
      return (int)(p+0.5f);
    }
  }
  return 0;
}

// Device battery color thresholds:
//   > 25%  GREEN
//   > 5%   BLACK
//   <= 5%  RED
uint16_t batteryColor(int pct) {
  if (pct > BATT_GREEN_PCT) return GxEPD_GREEN;
  if (pct > BATT_RED_PCT)   return GxEPD_BLACK;
  return GxEPD_RED;
}

// ============================================================================
// Helpers
// ============================================================================
String prettyCondition(const String& c) {
  if (c=="clear-night")     return "Clear Night";
  if (c=="cloudy")          return "Cloudy";
  if (c=="fog")             return "Fog";
  if (c=="hail")            return "Hail";
  if (c=="lightning")       return "Lightning";
  if (c=="lightning-rainy") return "Thunderstorms";
  if (c=="partlycloudy")    return "Partly Cloudy";
  if (c=="pouring")         return "Pouring";
  if (c=="rainy")           return "Rainy";
  if (c=="snowy")           return "Snowy";
  if (c=="snowy-rainy")     return "Sleet";
  if (c=="sunny")           return "Sunny";
  if (c=="windy")           return "Windy";
  if (c=="windy-variant")   return "Windy";
  if (c=="exceptional")     return "Exceptional";
  return c;
}
String asciiUnit(const char* u) {
  String out;
  if (!u) return out;
  for (const char* p = u; *p; p++) {
    unsigned char ch = (unsigned char)*p;
    if (ch >= 32 && ch < 127) out += (char)ch;
  }
  return out;
}

// ============================================================================
// HA fetch
// ============================================================================
int  g_lastHttpCode = 0;   // HTTP status of the most recent haGet (for error screen)
bool g_haSawSuccess = false;  // true if ANY haGet returned 200 this wake

int haGet(const String& entityId, String& bodyOut) {
  bodyOut = "";
  if (WiFi.status() != WL_CONNECTED) { g_lastHttpCode = -1; return -1; }
  HTTPClient http;
  String url = String(HA_BASE_URL) + "/api/states/" + entityId;
  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
  http.addHeader("Content-Type", "application/json");
  int code = http.GET();
  g_lastHttpCode = code;
  if (code == 200) { bodyOut = http.getString(); g_haSawSuccess = true; }
  else Serial1.printf("HTTP %d for %s\n", code, entityId.c_str());
  http.end();
  return code;
}

// POST a state value to HA. Creates the entity if it doesn't exist.
// body: JSON string e.g. {"state":"42","attributes":{"unit_of_measurement":"%"}}
// Returns HTTP code (200 or 201 on success).
int haPost(const String& entityId, const String& body) {
  if (WiFi.status() != WL_CONNECTED) return -1;
  HTTPClient http;
  String url = String(HA_BASE_URL) + "/api/states/" + entityId;
  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  if (code != 200 && code != 201)
    Serial1.printf("haPost HTTP %d for %s\n", code, entityId.c_str());
  http.end();
  return code;
}

// Read the onboard SHT40 sensor and write device telemetry to HA.
// Called once per timer wake after a successful HA fetch, so the data
// lands in HA alongside the main sensor refresh.
void writeTelemetry(int battPct, float battVolts) {
  // ---- SHT40 temperature and humidity ----
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  SensirionI2cSht4x sht4x;
  sht4x.begin(Wire, SHT40_I2C_ADDR_44);
  float tempC = NAN, humidity = NAN;
  uint16_t err = sht4x.measureHighPrecision(tempC, humidity);
  if (err) {
    Serial1.printf("SHT40 error: %d\n", err);
    tempC = NAN; humidity = NAN;
  } else {
    Serial1.printf("SHT40: %.1fC  %.0f%%RH\n", tempC, humidity);
  }

  // ---- Battery ----
  String battJson = String("{\"state\":\"") + battPct +
    "\",\"attributes\":{\"unit_of_measurement\":\"%\","
    "\"device_class\":\"battery\","
    "\"state_class\":\"measurement\","
    "\"friendly_name\":\"Kitchen Dash Battery\"}}";
  haPost(HA_ENTITY_BATTERY, battJson);

  String voltJson = String("{\"state\":\"") + String(battVolts, 2) +
    "\",\"attributes\":{\"unit_of_measurement\":\"V\","
    "\"device_class\":\"voltage\","
    "\"state_class\":\"measurement\","
    "\"friendly_name\":\"Kitchen Dash Voltage\"}}";
  haPost(HA_ENTITY_VOLTAGE, voltJson);

  // ---- Temperature ----
  if (!isnan(tempC)) {
    String tJson = String("{\"state\":\"") + String(tempC, 1) +
      "\",\"attributes\":{\"unit_of_measurement\":\"°C\","
      "\"device_class\":\"temperature\","
      "\"state_class\":\"measurement\","
      "\"friendly_name\":\"Kitchen Dash Temperature\"}}";
    haPost(HA_ENTITY_TEMPERATURE, tJson);
  }

  // ---- Humidity ----
  if (!isnan(humidity)) {
    String hJson = String("{\"state\":\"") + String(humidity, 0) +
      "\",\"attributes\":{\"unit_of_measurement\":\"%\","
      "\"device_class\":\"humidity\","
      "\"state_class\":\"measurement\","
      "\"friendly_name\":\"Kitchen Dash Humidity\"}}";
    haPost(HA_ENTITY_HUMIDITY, hJson);
  }

}

bool fetchWeather(Weather& out) {
  out = Weather();
  String body;
  if (haGet(WEATHER_ENTITY, body) != 200) return false;

  JsonDocument filter;
  filter["state"] = true;
  filter["attributes"]["temperature"] = true;
  filter["attributes"]["temperature_unit"] = true;
  filter["attributes"]["humidity"] = true;
  filter["attributes"]["wind_speed"] = true;
  filter["attributes"]["wind_speed_unit"] = true;
  filter["attributes"]["wind_bearing"] = true;
  filter["attributes"]["uv_index"] = true;

  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;
  JsonObject attr = doc["attributes"];

  out.condition = prettyCondition(doc["state"].as<String>());

  // Distinguish partly-cloudy day vs night by local hour (night = 20:00-06:00)
  if (out.condition == "Partly Cloudy") {
    struct tm tmNow;
    if (getLocalTime(&tmNow, 100)) {
      int h = tmNow.tm_hour;
      if (h >= 20 || h < 6) out.condition = "Partly Cloudy Night";
    }
  }

  if (!attr["temperature"].isNull()) {
    out.tempNum  = String((int)roundf(attr["temperature"].as<float>()));
    out.tempUnit = asciiUnit(attr["temperature_unit"]);
  } else { out.tempNum = "--"; out.tempUnit = ""; }

  out.humidity = attr["humidity"].isNull() ? "--"
               : String(attr["humidity"].as<float>(), 0);

  if (!attr["wind_speed"].isNull()) {
    out.windSpeed   = String(attr["wind_speed"].as<float>(), 0);
    out.windUnit    = asciiUnit(attr["wind_speed_unit"]);
    out.windBearing = attr["wind_bearing"].isNull() ? 0.0f
                    : attr["wind_bearing"].as<float>();
  } else { out.windSpeed = "--"; out.windUnit = ""; out.windBearing = 0.0f; }

  out.uvIndex = attr["uv_index"].isNull() ? "--"
              : String(attr["uv_index"].as<float>(), 1);

  out.valid = true;
  return true;
}

bool fetchSensor(const char* entityId, String& valueOut,
                 float& numOut, bool& isNumOut) {
  valueOut = ""; numOut = 0.0f; isNumOut = false;
  String body;
  if (haGet(entityId, body) != 200) return false;
  JsonDocument filter;
  filter["state"] = true;
  filter["attributes"]["unit_of_measurement"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;
  String stateStr = doc["state"].as<String>();
  char* endp = nullptr;
  float parsed = strtof(stateStr.c_str(), &endp);
  if (endp != stateStr.c_str() && *endp == '\0') { isNumOut=true; numOut=parsed; }
  valueOut = stateStr;
  String u = asciiUnit(doc["attributes"]["unit_of_measurement"]);
  if (u.length()) { valueOut += " "; valueOut += u; }
  return true;
}

String buildBatteryStatus() {
  String v; float power; bool isNum;
  if (!fetchSensor(BATT_POWER_ENTITY, v, power, isNum) || !isNum) return "";
  if (power < -IDLE_WATTS) {
    String s = "Charging";
    String eta; float fn; bool b;
    if (fetchSensor(BATT_CHARGE_ETA_ENTITY, eta, fn, b)) {
      String hhmm = isoUtcToLocalHHMM(eta);
      if (hhmm.length()) s += " | full at " + hhmm;
    }
    return s;
  } else if (power > IDLE_WATTS) {
    String s = "Discharging";
    String eta; float fn; bool b;
    if (fetchSensor(BATT_DISCHARGE_ETA_ENTITY, eta, fn, b)) {
      String hhmm = isoUtcToLocalHHMM(eta);
      if (hhmm.length()) s += " | empty at " + hhmm;
    }
    return s;
  }
  return "Idle";
}

bool weatherChanged() {
  return !rtcWxValid ||
         currentWx.condition != String(rtcWxCond) ||
         currentWx.tempNum   != String(rtcWxTemp) ||
         currentWx.humidity  != String(rtcWxHumidity) ||
         currentWx.windSpeed != String(rtcWxWind) ||
         currentWx.uvIndex   != String(rtcWxUV) ||
         fabsf(currentWx.windBearing - rtcWxBearing) > 22.0f;
}

// Fetch heating state from climate.whole_house, including the aggregate
// current and target temperatures (used by the heating page).
void fetchHeating() {
  currentHeatingState  = "off";
  currentHeatingActive = 0;
  currentHeatingTemp   = NAN;
  currentHeatingTarget = NAN;
  String body;
  if (haGet(HEATING_ENTITY, body) != 200) return;

  JsonDocument filter;
  filter["state"] = true;
  filter["attributes"]["active_member_count"]   = true;
  filter["attributes"]["current_temperature"]   = true;
  filter["attributes"]["temperature"]           = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return;

  currentHeatingState  = doc["state"].as<String>();
  JsonObject a = doc["attributes"];
  if (!a["active_member_count"].isNull())
    currentHeatingActive = a["active_member_count"].as<int>();
  if (!a["current_temperature"].isNull())
    currentHeatingTemp = a["current_temperature"].as<float>();
  if (!a["temperature"].isNull())
    currentHeatingTarget = a["temperature"].as<float>();
}

// Fetch water heater state and temperatures
void fetchWaterHeater() {
  currentWaterOp     = "off";
  currentWaterTemp   = 0;
  currentWaterTarget = 0;
  String body;
  if (haGet(WATER_HEATER_ENTITY, body) != 200) return;

  JsonDocument filter;
  filter["attributes"]["operation_mode"]      = true;
  filter["attributes"]["current_temperature"] = true;
  filter["attributes"]["temperature"]         = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return;

  JsonObject attr = doc["attributes"];
  if (!attr["operation_mode"].isNull())
    currentWaterOp = attr["operation_mode"].as<String>();
  if (!attr["current_temperature"].isNull())
    currentWaterTemp = (int)roundf(attr["current_temperature"].as<float>());
  if (!attr["temperature"].isNull())
    currentWaterTarget = (int)roundf(attr["temperature"].as<float>());
}

// Fetch Solcast solar forecast — today and tomorrow kWh totals.
void fetchSolcast() {
  currentSolcastToday    = NAN;
  String body;
  if (haGet(SOLCAST_TODAY, body) == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, body))
      currentSolcastToday = doc["state"].as<float>();
  }
  if (!isnan(currentSolcastToday))
    Serial1.printf("Solcast: today=%.1f kWh\n", currentSolcastToday);
}

// Fetch hourly forecast and derive rain state for the next 12 hours.
// rainState: 0=dry, 1=currently raining (shows stop), 2=rain coming (shows start),
//            3=raining with no stop found in 12h window.
// rainTime: local HH:MM of the transition event.
void fetchHourlyRain() {
  currentRainState = 0;
  currentRainTime  = "";

  if (WiFi.status() != WL_CONNECTED) return;

  // POST to get_forecasts service with return_response
  HTTPClient http;
  String url = String(HA_BASE_URL) +
               "/api/services/weather/get_forecasts?return_response";
  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
  http.addHeader("Content-Type", "application/json");

  String reqBody = String("{\"entity_id\":\"") + WEATHER_HOURLY_ENTITY +
                   "\",\"type\":\"hourly\"}";
  int code = http.POST(reqBody);
  if (code != 200) {
    Serial1.printf("fetchHourlyRain HTTP %d\n", code);
    http.end();
    return;
  }

  String resp = http.getString();
  http.end();

  // Parse: {"service_response":{"weather.X":{"forecast":[...]}}}
  JsonDocument doc;
  JsonDocument filter;
  filter["service_response"][WEATHER_HOURLY_ENTITY]["forecast"] = true;
  if (deserializeJson(doc, resp, DeserializationOption::Filter(filter))) return;

  JsonArray forecast = doc["service_response"][WEATHER_HOURLY_ENTITY]["forecast"];
  if (forecast.isNull() || forecast.size() == 0) return;

  // Get current local time so we can skip hours in the past and cap at +12h
  struct tm tmNow;
  if (!getLocalTime(&tmNow, 100)) return;
  time_t tNow = mktime(&tmNow);

  // Walk the next 12 hours. First entry is the current hour.
  bool currentlyRaining = false;
  bool foundEvent = false;
  int hoursChecked = 0;

  for (JsonObject entry : forecast) {
    if (hoursChecked >= 12) break;

    // Parse the ISO datetime string to a time_t
    String dt = entry["datetime"].as<String>();
    // dt format: "2026-06-20T22:00:00+00:00"
    struct tm tmEntry = {};
    int yr,mo,dy,hr,mn,sc,offH,offM;
    char sign;
    // Parse manually since strptime isn't reliable on ESP32
    if (sscanf(dt.c_str(), "%d-%d-%dT%d:%d:%d%c%d:%d",
               &yr,&mo,&dy,&hr,&mn,&sc,&sign,&offH,&offM) < 6) continue;
    tmEntry.tm_year = yr - 1900;
    tmEntry.tm_mon  = mo - 1;
    tmEntry.tm_mday = dy;
    tmEntry.tm_hour = hr;
    tmEntry.tm_min  = mn;
    tmEntry.tm_sec  = sc;
    tmEntry.tm_isdst = 0;
    time_t tEntry = mktime(&tmEntry);  // UTC epoch

    // Skip entries more than 1 hour in the past
    if (tEntry < tNow - 3600) continue;
    // Stop after 12 hours from now
    if (tEntry > tNow + 12 * 3600) break;

    float precip = entry["precipitation"].as<float>();
    bool raining  = (precip > RAIN_THRESHOLD_MM);

    if (hoursChecked == 0) {
      currentlyRaining = raining;
    }

    if (currentlyRaining && !raining && !foundEvent) {
      // Rain stopping — convert UTC entry time to local for display
      time_t tLocal = tEntry;
      struct tm tmLocal;
      localtime_r(&tLocal, &tmLocal);
      char buf[6];
      strftime(buf, sizeof(buf), "%H:%M", &tmLocal);
      currentRainState = 1;  // currently raining
      currentRainTime  = String(buf);
      foundEvent = true;
    } else if (!currentlyRaining && raining && !foundEvent) {
      // Rain starting
      time_t tLocal = tEntry;
      struct tm tmLocal;
      localtime_r(&tLocal, &tmLocal);
      char buf[6];
      strftime(buf, sizeof(buf), "%H:%M", &tmLocal);
      currentRainState = 2;  // rain coming
      currentRainTime  = String(buf);
      foundEvent = true;
    }

    hoursChecked++;
  }

  // Currently raining but no stop found in 12h window
  if (currentlyRaining && !foundEvent) {
    currentRainState = 3;
  }

  Serial1.printf("Rain state: %d  time: %s\n",
                 currentRainState, currentRainTime.c_str());
}


// ZoneData struct is defined in shared_state.h
ZoneData heatZones[16];
int      heatZoneCount = 0;
int      heatCallingCount = 0;       // zones with target-current > 0.5°C
bool     heatAnyActive = false;      // any room actively heating
// hot water for the page (reuses currentWaterOp/Temp/Target)

// Fetch all heating zones into heatZones[]. Runs every wake (not just on Key 1)
// so the status screen can show "N zones heating" in its NOW section.
void fetchZones() {
  heatZoneCount = 0;
  heatCallingCount = 0;

  for (int i = 0; i < HEAT_ZONE_COUNT && heatZoneCount < 16; i++) {
    String body;
    if (haGet(HEAT_ZONES[i].entity, body) != 200) continue;

    JsonDocument filter;
    filter["state"] = true;
    filter["attributes"]["current_temperature"] = true;
    filter["attributes"]["temperature"]         = true;
    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) continue;

    ZoneData& z = heatZones[heatZoneCount];
    z.label = HEAT_ZONES[i].label;
    z.valid = true;
    String st = doc["state"].as<String>();
    JsonObject a = doc["attributes"];
    if (!a["current_temperature"].isNull()) z.current = a["current_temperature"].as<float>();
    if (!a["temperature"].isNull())         z.target  = a["temperature"].as<float>();

    // Per-zone "calling for heat" / valve open: the heating system can be
    // running while individual valves stay closed. A zone is treated as
    // calling only when its setpoint exceeds the measured temperature by more
    // than 0.5°C — that's roughly the threshold at which a TRV opens.
    bool enabled = (st == "heat" || st == "auto");
    z.active = (enabled && !isnan(z.current) && !isnan(z.target)
                && (z.target - z.current) > 0.5f);
    if (z.active) heatCallingCount++;
    heatZoneCount++;
  }
}

// Fetch everything needed by the heating page. Called on Key-1 wake.
// Zones are already fetched by fetchZones() during fetchAllAndCompare();
// this only adds the hot-water tank state.
void fetchHeatingPage() {
  // Fetch zones now (on Key-1 wake, fetchAllAndCompare is not called so
  // fetchZones hasn't run yet — we must call it here explicitly).
  fetchZones();
  heatAnyActive = (heatCallingCount > 0);
  fetchWaterHeater();
}

// Fetch 3-day daily forecast via weather.get_forecasts service call.
// POST /api/services/weather/get_forecasts?return_response
// Returns true if at least one day was parsed.
bool fetchForecast() {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  String url = String(HA_BASE_URL) + "/api/services/weather/get_forecasts?return_response";
  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
  http.addHeader("Content-Type", "application/json");

  // Request daily forecast for our weather entity
  String body = String("{\"type\":\"daily\",\"entity_id\":\"") + WEATHER_ENTITY + "\"}";
  int code = http.POST(body);
  if (code != 200) {
    Serial1.printf("Forecast HTTP %d\n", code);
    http.end();
    return false;
  }

  String resp = http.getString();
  http.end();

  // Parse: response -> service_response -> weather.forecast_home -> forecast[]
  JsonDocument doc;
  if (deserializeJson(doc, resp)) return false;

  JsonArray arr = doc["service_response"][WEATHER_ENTITY]["forecast"].as<JsonArray>();
  if (arr.isNull() || arr.size() == 0) return false;

  // Day name lookup
  const char* DAYS[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

  // Skip index 0 (today) — take the next FORECAST_DAYS entries
  int filled = 0;
  for (int i = 1; i < (int)arr.size() && filled < FORECAST_DAYS; i++) {
    JsonObject day = arr[i];
    ForecastDay& fc = currentForecast[filled];

    fc.condition = prettyCondition(day["condition"].as<String>());

    // Parse day-of-week from datetime e.g. "2026-06-08T11:00:00+00:00"
    String dt = day["datetime"].as<String>();
    if (dt.length() >= 10) {
      struct tm t = {0};
      t.tm_year = dt.substring(0,4).toInt() - 1900;
      t.tm_mon  = dt.substring(5,7).toInt() - 1;
      t.tm_mday = dt.substring(8,10).toInt();
      mktime(&t);  // fills tm_wday
      fc.dayLabel = DAYS[t.tm_wday];
    } else {
      fc.dayLabel = "?";
    }

    fc.high   = day["temperature"].isNull() ? "--"
              : String((int)roundf(day["temperature"].as<float>()));
    fc.low    = day["templow"].isNull()     ? "--"
              : String((int)roundf(day["templow"].as<float>()));
    fc.precip = day["precipitation"].isNull() ? ""
              : String(day["precipitation"].as<float>(), 1);

    fc.valid = true;
    filled++;
  }

  Serial1.printf("Forecast: fetched %d days\n", filled);
  return filled > 0;
}

bool forecastChanged() {
  for (int i = 0; i < FORECAST_DAYS; i++) {
    if (!currentForecast[i].valid) continue;
    if (currentForecast[i].condition != String(rtcFcCond[i])) return true;
    if (currentForecast[i].high      != String(rtcFcHigh[i])) return true;
    if (currentForecast[i].low       != String(rtcFcLow[i]))  return true;
  }
  return false;
}

// Simple djb2-style hash for change detection across all event summaries
static uint32_t hashEvents() {
  uint32_t h = 5381;
  for (int i = 0; i < currentEventCount; i++) {
    for (char c : currentEvents[i].summary)     h = ((h << 5) + h) + c;
    for (char c : currentEvents[i].time)        h = ((h << 5) + h) + c;
    for (char c : currentEvents[i].description) h = ((h << 5) + h) + c;
  }
  return h;
}

// Extract HH:MM in local time from a dateTime string like "2026-06-07T10:00:00+01:00"
static String dtToLocalHHMM(const String& dt) {
  if (dt.length() < 19) return "";
  // Parse UTC offset from end e.g. "+01:00" or "-05:00"
  int offsetMins = 0;
  if (dt.length() >= 25) {
    char sign = dt[19];
    if (sign == '+' || sign == '-') {
      int oh = dt.substring(20,22).toInt();
      int om = dt.substring(23,25).toInt();
      offsetMins = (sign=='+' ? 1 : -1) * (oh*60+om);
    }
  }
  int h = dt.substring(11,13).toInt();
  int m = dt.substring(14,16).toInt();
  // Apply offset to get UTC, then local via NTP TZ
  // Simpler: just show the wall-clock time as given (already in local tz from HA)
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
  return String(buf);
}

// Fetch today's events from all configured calendars.
// Events are sorted: timed events first (by time), then all-day.
void fetchCalendar() {
  currentEventCount = 0;

  // Build today's start/end in UTC ISO format
  struct tm tmNow;
  if (!getLocalTime(&tmNow, 100)) return;

  // Midnight local time today and tomorrow
  struct tm tmStart = tmNow; tmStart.tm_hour=0; tmStart.tm_min=0; tmStart.tm_sec=0;
  struct tm tmEnd   = tmNow; tmEnd.tm_hour=23;  tmEnd.tm_min=59;  tmEnd.tm_sec=59;

  char startBuf[32], endBuf[32];
  // Format as UTC ISO — use mktime then gmtime
  time_t tStart = mktime(&tmStart);
  time_t tEnd   = mktime(&tmEnd);
  struct tm *gs = gmtime(&tStart); strftime(startBuf, sizeof(startBuf), "%Y-%m-%dT%H:%M:%S.000Z", gs);
  struct tm *ge = gmtime(&tEnd);   strftime(endBuf,   sizeof(endBuf),   "%Y-%m-%dT%H:%M:%S.000Z", ge);

  for (int ci = 0; ci < CALENDAR_COUNT && currentEventCount < MAX_CAL_EVENTS; ci++) {
    if (WiFi.status() != WL_CONNECTED) break;
    HTTPClient http;
    String url = String(HA_BASE_URL) + "/api/calendars/" +
                 CALENDARS[ci].entity + "?start=" + startBuf + "&end=" + endBuf;
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
    http.addHeader("Content-Type", "application/json");
    int code = http.GET();
    if (code != 200) {
      Serial1.printf("Calendar HTTP %d for %s\n", code, CALENDARS[ci].entity);
      http.end(); continue;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) continue;
    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) continue;

    for (JsonObject ev : arr) {
      if (currentEventCount >= MAX_CAL_EVENTS) break;
      CalEvent& ce = currentEvents[currentEventCount];
      ce.summary  = ev["summary"].as<String>();
      ce.colorIdx = CALENDARS[ci].color;
      ce.valid    = true;

      // Description: take the first ~6 words, strip newlines, cap length
      ce.description = "";
      if (!ev["description"].isNull()) {
        String d = ev["description"].as<String>();
        d.replace("\n", " ");
        d.replace("\r", " ");
        d.trim();
        // Keep first 6 words
        int words = 0, idx = 0;
        while (idx < (int)d.length() && words < 6) {
          int sp = d.indexOf(' ', idx);
          if (sp < 0) { idx = d.length(); break; }
          idx = sp + 1;
          words++;
        }
        ce.description = d.substring(0, idx);
        ce.description.trim();
        if (ce.description.length() < d.length()) ce.description += "...";
        // Hard cap so it can't overflow the column
        if (ce.description.length() > 32)
          ce.description = ce.description.substring(0, 32) + "...";
      }

      // Timed event has dateTime, all-day has date
      if (!ev["start"]["dateTime"].isNull()) {
        ce.time = dtToLocalHHMM(ev["start"]["dateTime"].as<String>());
      } else {
        ce.time = "";  // all-day
      }
      currentEventCount++;
    }
  }

  // Sort: timed events first (by time string), then all-day
  // Simple insertion sort — small N, fine here
  for (int i = 1; i < currentEventCount; i++) {
    CalEvent key = currentEvents[i];
    int j = i - 1;
    // timed (non-empty time) before all-day (empty time)
    bool keyTimed = key.time.length() > 0;
    while (j >= 0) {
      bool jTimed = currentEvents[j].time.length() > 0;
      bool swap = false;
      if (!jTimed && keyTimed) swap = true;          // timed before all-day
      else if (jTimed == keyTimed && keyTimed &&
               key.time < currentEvents[j].time) swap = true; // earlier time first
      if (!swap) break;
      currentEvents[j+1] = currentEvents[j]; j--;
    }
    currentEvents[j+1] = key;
  }

  Serial1.printf("Calendar: %d events today\n", currentEventCount);
}

bool fetchAllAndCompare() {
  bool changed = false;
  Weather wx;
  if (fetchWeather(wx)) {
    currentWx = wx;
    if (weatherChanged()) changed = true;
  }
  for (int i = 0; i < ENTITY_COUNT; i++) {
    String v; float n; bool isNum;
    if (fetchSensor(ENTITIES[i].id, v, n, isNum)) {
      currentValues[i]=v; currentNums[i]=n; currentIsNum[i]=isNum;
    } else {
      currentValues[i]="n/a"; currentIsNum[i]=false;
    }
    if (!rtcInitialized) { changed=true; continue; }
    if (currentIsNum[i] != rtcSensorIsNum[i]) { changed=true; }
    else if (currentIsNum[i]) {
      if (fabsf(currentNums[i]-rtcSensorNum[i]) >= ENTITIES[i].threshold) changed=true;
    } else {
      if (currentValues[i] != String(rtcSensorStr[i])) changed=true;
    }
  }
  currentBattStatus = buildBatteryStatus();
  if (currentBattStatus != String(rtcBattStatus)) changed=true;

  // Status bar entities
  {
    String v; float n; bool isNum;
    fetchSensor(ALARM_ENTITY, v, n, isNum);
    // Strip any unit — alarm state is a plain string like "disarmed"
    int sp = v.indexOf(' ');
    currentAlarmState = (sp > 0) ? v.substring(0, sp) : v;
    if (currentAlarmState != String(rtcAlarmState)) changed=true;

    // Last alarm event message (full string, shown as alarm's 2nd line)
    fetchSensor(ALARM_EVENT_ENTITY, v, n, isNum);
    currentAlarmEvent = v;
    if (currentAlarmEvent != String(rtcAlarmEvent)) changed=true;
  }

  fetchHeating();
  {
    String key = currentHeatingState + String(currentHeatingActive);
    if (key != String(rtcHeatingState)) changed=true;
  }

  // Per-zone fetch — runs every wake so the status screen can show
  // "N zones heating" and hide the NOW item when nothing is calling.
  fetchZones();
  if (heatCallingCount != rtcCallingCount) changed=true;

  fetchWaterHeater();
  {
    String key = currentWaterOp + String(currentWaterTemp) + String(currentWaterTarget);
    if (key != String(rtcWaterState)) changed=true;
  }

  // Door sensors
  {
    String v; float n; bool isNum;
    fetchSensor(FRONT_DOOR_ENTITY, v, n, isNum);
    currentFrontDoor = (v == "on");
    fetchSensor(BACK_DOOR_ENTITY, v, n, isNum);
    currentBackDoor = (v == "on");
    if (currentFrontDoor != rtcFrontDoor || currentBackDoor != rtcBackDoor) changed=true;
  }

  // Window sensors — fetch each, build bitmask
  {
    String v; float n; bool isNum;
    currentWindows = 0;
    for (int i = 0; i < WINDOW_COUNT; i++) {
      fetchSensor(WINDOW_ENTITIES[i], v, n, isNum);
      if (v == "on") currentWindows |= (1 << i);
    }
    if (currentWindows != rtcWindows) changed=true;
  }

  // Plant soil moisture, vacuums, mower
  {
    String v; float n; bool isNum;
    // Plant moisture sensors — build a list of dry plant labels
  {
    String v; float n; bool isNum;
    String dryList = "";
    for (int i = 0; i < PLANT_COUNT; i++) {
      fetchSensor(PLANTS[i].entity, v, n, isNum);
      if (v.indexOf("Dry") >= 0) {
        if (dryList.length()) dryList += ", ";
        dryList += PLANTS[i].label;
      }
    }
    if (dryList != currentSoilStatus) changed = true;
    currentSoilStatus = dryList;
  }

    // -- Vacuum 1 --
    fetchSensor(VACUUM1_ENTITY, v, n, isNum);
    currentVac1 = v;
    fetchSensor(VACUUM1_BATTERY, v, n, isNum);
    currentVac1Batt = isNum ? (int)roundf(n) : 0;
    fetchSensor(VACUUM1_CHARGING, v, n, isNum);
    currentVac1Charging = (v == "on");
    if (currentVac1 != String(rtcVac1) ||
        currentVac1Batt != rtcVac1Batt ||
        currentVac1Charging != rtcVac1Chg) changed=true;

    // -- Vacuum 2 — no charging sensor, infer from docked + <100% --
    fetchSensor(VACUUM2_ENTITY, v, n, isNum);
    currentVac2 = v;
    fetchSensor(VACUUM2_BATTERY, v, n, isNum);
    currentVac2Batt = isNum ? (int)roundf(n) : 0;
    currentVac2Charging = (currentVac2 == "docked" && currentVac2Batt < 100);
    if (currentVac2 != String(rtcVac2) ||
        currentVac2Batt != rtcVac2Batt ||
        currentVac2Charging != rtcVac2Chg) changed=true;

    // -- Mower --
    fetchSensor(MOWER_ENTITY, v, n, isNum);
    currentMower = v;
    fetchSensor(MOWER_BATTERY, v, n, isNum);
    currentMowerBatt = isNum ? (int)roundf(n) : 0;
    fetchSensor(MOWER_CHARGING, v, n, isNum);
    currentMowerCharging = (v == "on");
    if (currentMower != String(rtcMower) ||
        currentMowerBatt != rtcMowerBatt ||
        currentMowerCharging != rtcMowerChg) changed=true;
  }

  fetchForecast();
  if (forecastChanged()) changed=true;

  fetchCalendar();
  if (hashEvents() != rtcCalHash) changed=true;

  // Solcast solar forecast — triggers redraw only if forecast shifts by >0.5 kWh
  float prevToday = currentSolcastToday;
  fetchSolcast();
  if (!isnan(currentSolcastToday) && !isnan(prevToday) &&
      fabsf(currentSolcastToday - prevToday) > 0.5f) changed=true;

  // Hourly rain forecast — triggers redraw if rain state changes
  int prevRainState = currentRainState;
  fetchHourlyRain();
  if (currentRainState != prevRainState) changed=true;

  if (!rtcInitialized) changed=true;
  return changed;
}

void commitSnapshot() {
  copyToBuf(rtcWxCond,     currentWx.condition);
  copyToBuf(rtcWxTemp,     currentWx.tempNum);
  copyToBuf(rtcWxHumidity, currentWx.humidity);
  copyToBuf(rtcWxWind,     currentWx.windSpeed);
  copyToBuf(rtcWxUV,       currentWx.uvIndex);
  rtcWxBearing  = currentWx.windBearing;
  rtcWxValid    = currentWx.valid;
  for (int i = 0; i < ENTITY_COUNT; i++) {
    copyToBuf(rtcSensorStr[i], currentValues[i]);
    rtcSensorNum[i]   = currentNums[i];
    rtcSensorIsNum[i] = currentIsNum[i];
  }
  copyToBuf(rtcBattStatus, currentBattStatus);
  copyToBuf(rtcAlarmState, currentAlarmState);
  copyToBuf(rtcAlarmEvent, currentAlarmEvent);
  { String k = currentHeatingState + String(currentHeatingActive);
    copyToBuf(rtcHeatingState, k); }
  rtcCallingCount = heatCallingCount;
  { String k = currentWaterOp + String(currentWaterTemp) + String(currentWaterTarget);
    copyToBuf(rtcWaterState, k); }
  rtcFrontDoor = currentFrontDoor;
  rtcBackDoor  = currentBackDoor;
  rtcWindows   = currentWindows;
  copyToBuf(rtcSoilStatus, currentSoilStatus);
  copyToBuf(rtcVac1,  currentVac1);
  copyToBuf(rtcVac2,  currentVac2);
  copyToBuf(rtcMower, currentMower);
  rtcVac1Batt  = currentVac1Batt;  rtcVac1Chg  = currentVac1Charging;
  rtcVac2Batt  = currentVac2Batt;  rtcVac2Chg  = currentVac2Charging;
  rtcMowerBatt = currentMowerBatt; rtcMowerChg = currentMowerCharging;
  for (int i = 0; i < FORECAST_DAYS; i++) {
    copyToBuf(rtcFcCond[i], currentForecast[i].condition);
    copyToBuf(rtcFcHigh[i], currentForecast[i].high);
    copyToBuf(rtcFcLow[i],  currentForecast[i].low);
  }
  rtcCalHash = hashEvents();
  rtcInitialized = true;
}

// ============================================================================
// Layout constants — see layout.h to adjust positioning
// ============================================================================
// ============================================================================
// Draw helpers
// ============================================================================
// Print right-aligned text ending at x=rightEdge, baseline y
void printRight(const String& s, int rightEdge, int y) {
  int16_t bx,by; uint16_t bw,bh;
  display.getTextBounds(s,0,0,&bx,&by,&bw,&bh);
  display.setCursor(rightEdge - bw, y);
  display.print(s);
}



// ============================================================================
// drawDashboard

// ============================================================================


void goToSleep() {
  // Give the e-paper time to fully settle after refresh before cutting power.
  // Some Spectra-6 panels return from hibernate() before the physical update
  // completes; deep-sleeping too early can leave a partial/blank image.
  if (PRE_SLEEP_DELAY_MS > 0) {
    Serial1.printf("Settling %d ms before sleep...\n", PRE_SLEEP_DELAY_MS);
    delay(PRE_SLEEP_DELAY_MS);
  }

  uint64_t mins = computeSleepMinutes();
  uint64_t sleepUs = mins * 60ULL * uS_PER_SEC;
  Serial1.printf("Sleeping %llu min...\n", mins);
  Serial1.flush();

  // Wake on timer (normal refresh cycle)
  esp_sleep_enable_timer_wakeup(sleepUs);

  // Wake on any of the three user buttons (GPIO3/4/5, active-low)
  const uint64_t BTN_MASK = (1ULL << BTN_KEY0) | (1ULL << BTN_KEY1) | (1ULL << BTN_KEY2);
  esp_sleep_enable_ext1_wakeup(BTN_MASK, ESP_EXT1_WAKEUP_ANY_LOW);

  esp_deep_sleep_start();
}

void setup() {
  Serial1.begin(115200, SERIAL_8N1, SERIAL_RX, SERIAL_TX);
  bootCount++;
  Serial1.printf("\n=== Wake #%d ===\n", bootCount);

  // Check what woke us — button press forces a redraw regardless of data change.
  // Identify WHICH button via the ext1 wakeup pin mask.
  // KEY1 long-press (held >1s after wake) clears the screen to white.
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  bool forceDraw = (cause == ESP_SLEEP_WAKEUP_EXT1);
  bool heatingPageRequested  = false;
  bool securityPageRequested = false;
  bool clearScreenRequested  = false;
  bool helpScreenRequested   = false;
  if (forceDraw) {
    uint64_t mask = esp_sleep_get_ext1_wakeup_status();
    if (mask & (1ULL << BTN_KEY1)) {
      // Check for long press: wait 1 second, see if button is still held.
      // Buttons are active-low, so LOW = still pressed.
      pinMode(BTN_KEY1, INPUT_PULLUP);
      delay(1000);
      if (digitalRead(BTN_KEY1) == LOW) {
        clearScreenRequested = true;
        Serial1.println("KEY1 long press — clear screen.");
      } else {
        heatingPageRequested = true;
      }
    }
    if (mask & (1ULL << BTN_KEY2)) {
      // Long press on KEY2 shows the help/instructions screen.
      pinMode(BTN_KEY2, INPUT_PULLUP);
      delay(1000);
      if (digitalRead(BTN_KEY2) == LOW) {
        helpScreenRequested = true;
        Serial1.println("KEY2 long press — help screen.");
      } else {
        securityPageRequested = true;
      }
    }
    Serial1.printf("Woke by button (mask=0x%llx)%s%s%s%s.\n", mask,
                   heatingPageRequested  ? " — heating page"   : "",
                   securityPageRequested ? " — security page"  : "",
                   clearScreenRequested  ? " — clear screen"   : "",
                   helpScreenRequested   ? " — help screen"    : "");
  } else {
    Serial1.println("Woke by timer.");
  }

  pinMode(LED_PIN, OUTPUT); setLed(false);
  pinMode(SD_EN_PIN, OUTPUT); digitalWrite(SD_EN_PIN, HIGH);
  pinMode(EPD_RES_PIN, OUTPUT);
  pinMode(EPD_DC_PIN, OUTPUT);
  pinMode(EPD_CS_PIN, OUTPUT);

  hspi.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, -1);
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));

  // First-boot splash — drawn immediately on power-on so the panel shows
  // something during the 10-20 second startup sequence instead of retaining
  // the last image (which can look like a freeze). Skipped on subsequent
  // deep-sleep wakes since rtcInitialized will be true.
  if (!rtcInitialized) {
    display.init(115200);
    display.setRotation(0);
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      display.setTextColor(GxEPD_BLACK);
      display.setTextSize(4);
      display.setCursor(180, 220);
      display.print("Kitchen-Dash");
    } while (display.nextPage());
    display.hibernate();
    Serial1.println("First boot — connecting screen shown.");
  }

  // Critical battery check — before doing anything power-hungry.
  // At <=2% show the charge screen and go straight back to sleep,
  // skipping WiFi and data fetch entirely to conserve what's left.
  {
    int critPct = batteryPercent(readBatteryVoltage());
    if (critPct <= BATT_CRITICAL_PCT) {
      Serial1.printf("Battery critical (%d%%) — charge screen.\n", critPct);
      // Only redraw the charge screen if we weren't already showing it
      if (!rtcChargeScreenShown) {
        drawChargeScreen(critPct);
        rtcChargeScreenShown = true;
      }
      goToSleep();
      return;
    }
    rtcChargeScreenShown = false;  // recovered above critical threshold
  }

  // Long-press clear: fill the panel white and sleep immediately.
  // No WiFi needed. The next timer wake will redraw the status dashboard.
  if (clearScreenRequested) {
    display.init(115200);
    display.setRotation(0);
    display.setFullWindow();
    display.firstPage();
    do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
    display.hibernate();
    Serial1.println("Screen cleared — sleeping.");
    goToSleep();
    return;
  }

  bool wifiOk = connectWiFi();

  if (wifiOk) {
    syncTime();

    if (heatingPageRequested) {
      // Heating page: fetch only the climate zones + hot water, draw, sleep.
      // Timer wakes never reach here, so the panel always returns to the
      // status dashboard on the next scheduled refresh.
      Serial1.println("Fetching heating zones for heating page.");
      fetchHeatingPage();
      // If HA didn't answer (bad token / HA down), show the error screen.
      if (!g_haSawSuccess) {
        Serial1.printf("HA unreachable (HTTP %d) — error screen.\n", g_lastHttpCode);
        drawErrorScreen("Can't reach Home Assistant",
                        g_lastHttpCode == 401 ? "Authentication failed - check token"
                                              : "No response from Home Assistant");
      } else {
        drawHeatingPage();
      }
      // Note: no commitSnapshot — heating page doesn't use change detection.
    } else if (securityPageRequested) {
      // Security page: fetches and draws on demand, never on timer wakes.
      Serial1.println("Fetching security state.");
      drawSecurityPage();   // fetch + draw combined inside the screen
    } else if (helpScreenRequested) {
      // Help screen: no WiFi or fetch needed — purely local content.
      Serial1.println("Drawing help screen.");
      drawHelpScreen();
    } else {
      bool changed = fetchAllAndCompare();
      // If HA never answered this wake (bad token / HA down / network), the
      // last HTTP code reflects the failure. Show the error screen instead of
      // a blank or stale dashboard. (200 means at least one fetch succeeded.)
      if (!g_haSawSuccess) {
        Serial1.printf("HA unreachable (HTTP %d) — error screen.\n", g_lastHttpCode);
        drawErrorScreen("Can't reach Home Assistant",
                        g_lastHttpCode == 401 ? "Authentication failed - check token"
                                              : "No response from Home Assistant");
      } else if (changed || forceDraw) {
        Serial1.println("Drawing panel.");
        drawDashboard();
        commitSnapshot();
        // Write device telemetry back to HA so it's available for automations.
        // Done after the draw so the display refresh isn't delayed by the extra POST calls.
        float vbat = readBatteryVoltage();
        writeTelemetry(batteryPercent(vbat), vbat);
      } else {
        Serial1.println("No change -> sleeping.");
      }
    }
  } else {
    Serial1.println("WiFi failed — error screen.");
    drawErrorScreen("No WiFi connection", "Check network or router");
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  goToSleep();
}

void loop() {
  // Never reached — setup() always ends in deep sleep.
}