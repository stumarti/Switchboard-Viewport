// =============================================================================
// config.h — All user-configurable settings for the reTerminal E1002 dashboard
// Edit this file to match your setup. Do NOT paste secrets into chat or git.
// =============================================================================
#pragma once
#include <stdint.h>

#if defined(__has_include)
  #if __has_include("secrets.h")
    #include "secrets.h"
  #endif
#endif

#ifndef SECRETS_WIFI_SSID
#define SECRETS_WIFI_SSID "YOUR_WIFI_SSID"
#endif
#ifndef SECRETS_WIFI_PASS
#define SECRETS_WIFI_PASS "YOUR_WIFI_PASSWORD"
#endif
#ifndef SECRETS_HA_URL
#define SECRETS_HA_URL "http://YOUR_HA_HOST:8123"
#endif
#ifndef SECRETS_HA_TOKEN
#define SECRETS_HA_TOKEN "YOUR_LONG_LIVED_ACCESS_TOKEN"
#endif
#ifndef SECRETS_MQTT_BROKER_HOST
#define SECRETS_MQTT_BROKER_HOST "YOUR_MQTT_BROKER"
#endif
#ifndef SECRETS_MQTT_PORT
#define SECRETS_MQTT_PORT 1883
#endif
#ifndef SECRETS_MQTT_USERNAME
#define SECRETS_MQTT_USERNAME "YOUR_MQTT_USERNAME"
#endif
#ifndef SECRETS_MQTT_PASSWORD
#define SECRETS_MQTT_PASSWORD "YOUR_MQTT_PASSWORD"
#endif
#ifndef SECRETS_MQTT_CLIENT_ID
#define SECRETS_MQTT_CLIENT_ID "dashboard"
#endif
#ifndef SECRETS_MQTT_TOPIC_PREFIX
#define SECRETS_MQTT_TOPIC_PREFIX "dashboard"
#endif

// -----------------------------------------------------------------------------
// WiFi
// 2.4 GHz only — the reTerminal E Series does not support 5 GHz networks.
// -----------------------------------------------------------------------------
constexpr const char* WIFI_NAME = SECRETS_WIFI_SSID; // 2.4 GHz only
constexpr const char* WIFI_PASS = SECRETS_WIFI_PASS; // 2.4 GHz only

// -----------------------------------------------------------------------------
// Home Assistant
// Use a LAN IP rather than homeassistant.local — mDNS is unreliable on ESP32.
// Generate a Long-Lived Access Token from your HA profile → Security.
// -----------------------------------------------------------------------------
constexpr const char* HA_BASE_URL = SECRETS_HA_URL;   // no trailing slash
constexpr const char* HA_TOKEN    = SECRETS_HA_TOKEN;

// -----------------------------------------------------------------------------
// MQTT
// Set these to your broker host/credentials if you want the device to publish
// telemetry to MQTT as well as Home Assistant REST.
// -----------------------------------------------------------------------------
constexpr const char* MQTT_BROKER_HOST = SECRETS_MQTT_BROKER_HOST;
constexpr int         MQTT_PORT        = SECRETS_MQTT_PORT;
constexpr const char* MQTT_USERNAME    = SECRETS_MQTT_USERNAME;
constexpr const char* MQTT_PASSWORD    = SECRETS_MQTT_PASSWORD;
constexpr const char* MQTT_CLIENT_ID   = SECRETS_MQTT_CLIENT_ID;
constexpr const char* MQTT_TOPIC_PREFIX = SECRETS_MQTT_TOPIC_PREFIX;

// -----------------------------------------------------------------------------
// Weather entity
// Check the exact ID in Developer Tools → States and search for "weather."
// -----------------------------------------------------------------------------
constexpr const char* WEATHER_ENTITY = "weather.home";

// -----------------------------------------------------------------------------
// Sensor entities
// Add/remove rows freely. Each entry:
//   { "sensor.entity_id", "Label on screen", change_threshold }
//
// threshold: how much a NUMERIC value must move before the panel redraws.
//   Use 0.0f to redraw on ANY change (good for battery %, on/off states).
//   Use 0.1f or higher for climbing kWh counters to avoid redrawing every tick.
// -----------------------------------------------------------------------------
struct Entity { const char* id; const char* label; float threshold; };
const Entity ENTITIES[] = {
  { "sensor.grid_export",       "Grid Export", 0.1f },
  { "sensor.grid_import",       "Grid Import", 0.1f },
  { "sensor.battery_soc",       "Battery",     0.0f },
  { "sensor.solar_generation",  "Solar",       0.1f },
  { "sensor.load_today",        "Load Today",  0.1f },
};
const int ENTITY_COUNT = sizeof(ENTITIES) / sizeof(ENTITIES[0]);

// -----------------------------------------------------------------------------
// Solcast solar forecast entities
// -----------------------------------------------------------------------------
constexpr const char* SOLCAST_TODAY    = "sensor.solar_forecast_today";

// -----------------------------------------------------------------------------
// Hourly weather forecast — rain detection
// Precipitation threshold in mm/hour. Values at or below this are treated as
// dry. 0.1 catches light drizzle; raise to 0.5 to ignore drizzle entirely.
// -----------------------------------------------------------------------------
constexpr const char* WEATHER_HOURLY_ENTITY = "weather.home";
const float RAIN_THRESHOLD_MM = 0.1f;   // mm/hour — below this = dry

// -----------------------------------------------------------------------------
// Status bar entities (right column, top strip)
// -----------------------------------------------------------------------------
constexpr const char* ALARM_ENTITY        = "sensor.home_alarm_state";
constexpr const char* ALARM_EVENT_ENTITY  = "sensor.home_alarm_event";  // last event, shown as alarm 2nd line
// Max chars for the alarm event message on the NOW item's 2nd line (9pt).
const int   ALARM_EVENT_MAX_CHARS = 50;
constexpr const char* HEATING_ENTITY      = "climate.whole_house";
constexpr const char* WATER_HEATER_ENTITY = "water_heater.home_tank";

// -----------------------------------------------------------------------------
// Heating page (Key 1) — per-room climate zones shown in the top 80% of the
// left bar; hot water in the bottom 20%. A room is "active" when its state is
// heat/auto AND hvac_action is "heating". The bar section turns red when active.
// -----------------------------------------------------------------------------
struct HeatZone { const char* entity; const char* label; };
const HeatZone HEAT_ZONES[] = {
  { "climate.kitchen",     "Kitchen"     },
  { "climate.living_room", "Living Room" },
  { "climate.hall",        "Hall"        },
  { "climate.bathroom",    "Bathroom"    },
  { "climate.landing",     "Landing"     },
  { "climate.bedroom",     "Bedroom"     },
  { "climate.bedroom_2",   "Bedroom 2"   },
  { "climate.office",      "Office"      },
};
const int HEAT_ZONE_COUNT = sizeof(HEAT_ZONES) / sizeof(HEAT_ZONES[0]);
constexpr const char* FRONT_DOOR_ENTITY   = "binary_sensor.front_door";
constexpr const char* BACK_DOOR_ENTITY    = "binary_sensor.back_door";

// Window sensors — "on" = open, "off" = closed
constexpr const char* WINDOW_ENTITIES[] = {
  "binary_sensor.window_side",
  "binary_sensor.window_living_room",
  "binary_sensor.window_kitchen",
  "binary_sensor.window_office_left",
  "binary_sensor.window_office_right",
  "binary_sensor.window_bedroom_left",
  "binary_sensor.window_bedroom_right",
};
constexpr const char* WINDOW_LABELS[] = {
  "Side",
  "Living room",
  "Kitchen",
  "Office left",
  "Office right",
  "Master bed left",
  "Master bed right",
};
const int WINDOW_COUNT = 7;

// Plant soil moisture — state is a descriptive string ("Dry", "Almost Dry",
// "Wet", etc). The flower icon turns red when the state contains "Dry".
constexpr const char* SOIL_MOISTURE_ENTITY = "sensor.plant_soil_moisture";

// Plant soil moisture — each entry: entity ID + display label.
// State is a descriptive string ("Dry", "Almost Dry", "Wet", etc).
// The watering can NOW item appears when any plant's state contains "Dry".
struct PlantSensor { const char* entity; const char* label; };
const PlantSensor PLANTS[] = {
  { "sensor.plant_soil_moisture", "House plant" },
  // Add more plants here, e.g.:
  // { "sensor.living_room_plant_moisture", "Living room" },
};
const int PLANT_COUNT = sizeof(PLANTS) / sizeof(PLANTS[0]);

// Robot vacuums — standard vacuum states: cleaning/docked/idle/paused/returning/error.
// Icon: GREEN when cleaning, RED on error, BLUE when docked+charging (<100%),
//       BLACK when docked/full or otherwise idle.
constexpr const char* VACUUM1_ENTITY      = "vacuum.vacuum1";
constexpr const char* VACUUM1_BATTERY     = "sensor.vacuum1_battery";
constexpr const char* VACUUM1_CHARGING    = "binary_sensor.vacuum1_charging";  // "on" when charging
constexpr const char* VACUUM2_ENTITY      = "vacuum.vacuum2";
constexpr const char* VACUUM2_BATTERY     = "sensor.vacuum2_battery";
constexpr const char* VACUUM2_CHARGING    = "";  // no charging sensor — infer from docked + <100%

// Robot mower — standard lawn_mower states: mowing/docked/paused/error.
// Icon: GREEN when mowing, RED on error, BLUE when docked+charging (<100%), else BLACK.
constexpr const char* MOWER_ENTITY        = "lawn_mower.mower";
constexpr const char* MOWER_BATTERY       = "sensor.mower_battery";
constexpr const char* MOWER_CHARGING      = "binary_sensor.mower_charging";  // "on" when charging

// -----------------------------------------------------------------------------
// Calendars (shown in right column below status bar)
// Order determines display order. Color is the GxEPD2 color for each calendar.
// -----------------------------------------------------------------------------
struct CalendarConfig { const char* entity; uint16_t color; };
// Colors assigned at runtime since GxEPD_* constants need display context —
// we store an index: 0=BLACK 1=BLUE 2=RED 3=GREEN
#define CAL_COLOR_BLACK  0
#define CAL_COLOR_BLUE   1
#define CAL_COLOR_RED    2
#define CAL_COLOR_GREEN  3
const CalendarConfig CALENDARS[] = {
  { "calendar.home_schedule",        CAL_COLOR_BLACK },
  { "calendar.work",                 CAL_COLOR_BLUE  },
  { "calendar.birthdays",            CAL_COLOR_RED   },
  { "calendar.holidays_in_ireland",  CAL_COLOR_GREEN },
};
const int CALENDAR_COUNT = sizeof(CALENDARS) / sizeof(CALENDARS[0]);
const int MAX_EVENTS_PER_CAL = 5;   // cap per calendar to avoid overflow

// Max characters on a calendar entry's top line (time + summary combined).
// The summary is truncated with "..." so it can't overflow the column.
const int CAL_SUMMARY_MAX_CHARS = 60;
// battery_power: negative = charging, positive = discharging.
// Readings within ±IDLE_WATTS are treated as idle.
// -----------------------------------------------------------------------------
constexpr const char* BATT_POWER_ENTITY         = "sensor.battery_power";
constexpr const char* BATT_CHARGE_ETA_ENTITY    = "sensor.battery_charge_eta";
constexpr const char* BATT_DISCHARGE_ETA_ENTITY = "sensor.battery_discharge_eta";
const float IDLE_WATTS                = 100.0f;

// -----------------------------------------------------------------------------
// Time & timezone
// TZ_INFO uses POSIX timezone strings — handles DST automatically.
// Common examples:
//   UK:           "GMT0BST,M3.5.0/1,M10.5.0"
//   US Eastern:   "EST5EDT,M3.2.0,M11.1.0"
//   US Central:   "CST6CDT,M3.2.0,M11.1.0"
//   US Pacific:   "PST8PDT,M3.2.0,M11.1.0"
//   CET (Europe): "CET-1CEST,M3.5.0,M10.5.0/3"
//   AEST:         "AEST-10AEDT,M10.1.0,M4.1.0/3"
// Full list: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
// -----------------------------------------------------------------------------
constexpr const char* TZ_INFO    = "GMT0BST,M3.5.0/1,M10.5.0";
constexpr const char* NTP_SERVER          = "pool.ntp.org";    // primary NTP
constexpr const char* NTP_FALLBACK_SERVER = "pool.ntp.org";   // fallback (set to your local NTP if available)

// -----------------------------------------------------------------------------
// Sleep & refresh
// SLEEP_MINUTES: how long the device deep-sleeps between data refreshes.
// Shorter = more current data, more battery use.
// Recommended: 5–30 min for a live energy dashboard on mains power,
//              30–60 min if running on battery.
// -----------------------------------------------------------------------------
const uint64_t SLEEP_MINUTES = 30;

// -----------------------------------------------------------------------------
// Quiet hours — overnight the dashboard refreshes less often to save battery.
// During quiet hours the device sleeps NIGHT_SLEEP_MINUTES instead of
// SLEEP_MINUTES. Hours are local time, 24h clock. The window runs from
// QUIET_START_HOUR (inclusive) to QUIET_END_HOUR (exclusive), wrapping midnight.
// Default: 1-hour refreshes from 23:00 to 06:00.
// Set QUIET_ENABLED = false to disable and always use SLEEP_MINUTES.
// -----------------------------------------------------------------------------
const bool     QUIET_ENABLED      = true;
const int      QUIET_START_HOUR   = 23;   // 11pm
const int      QUIET_END_HOUR     = 6;    // 6am
const uint64_t NIGHT_SLEEP_MINUTES = 60;  // 1 hour during quiet hours

// -----------------------------------------------------------------------------
// Device battery thresholds (the reTerminal's own battery, shown in footer).
//   > BATT_GREEN_PCT   -> green icon
//   > BATT_RED_PCT     -> black icon
//   <= BATT_RED_PCT    -> red icon
//   <= BATT_CRITICAL_PCT -> full-screen "charge me" warning, skips WiFi/refresh
// -----------------------------------------------------------------------------
const int BATT_GREEN_PCT    = 25;
const int BATT_RED_PCT      = 5;
const int BATT_CRITICAL_PCT = 2;

// -----------------------------------------------------------------------------
// PRE_SLEEP_DELAY_MS: pause after the display refresh before entering deep sleep.
// The e-paper panel can take a moment to finish its physical update; cutting
// power too soon may leave a partial or blank screen. Increase if you see
// incomplete refreshes; set to 0 to disable. 2000-3000ms is typical for
// Spectra-6 full refreshes.
// -----------------------------------------------------------------------------
const int PRE_SLEEP_DELAY_MS = 2500;

// -----------------------------------------------------------------------------
// Hardware pins — only change if you have a different board variant
// -----------------------------------------------------------------------------
#define EPD_SCK_PIN        7
#define EPD_MOSI_PIN       9
#define EPD_CS_PIN         10
#define EPD_DC_PIN         11
#define EPD_RES_PIN        12
#define EPD_BUSY_PIN       13
#define SD_EN_PIN          16
#define LED_PIN            6    // green LED, inverted logic (LOW = on)
#define BATTERY_ADC_PIN    1    // battery voltage via /2 divider
#define BATTERY_ENABLE_PIN 21   // enables battery monitor circuit
#define BTN_KEY0           3    // user button KEY0 (active-low)
#define BTN_KEY1           4    // user button KEY1 (active-low)
#define BTN_KEY2           5    // user button KEY2 (active-low)
#define SERIAL_RX          44
#define SERIAL_TX          43

// I2C — onboard SHT40 temperature/humidity sensor
#define I2C_SDA_PIN        19
#define I2C_SCL_PIN        20



// -----------------------------------------------------------------------------
// Device telemetry — written back to HA every wake via POST /api/states.
// These entity IDs are created automatically in HA on first write; no
// integration or config needed. Values appear under Developer Tools → States.
// -----------------------------------------------------------------------------
constexpr const char* HA_ENTITY_BATTERY     = "sensor.dashboard_battery";
constexpr const char* HA_ENTITY_VOLTAGE     = "sensor.dashboard_voltage";
constexpr const char* HA_ENTITY_TEMPERATURE = "sensor.dashboard_temperature";
constexpr const char* HA_ENTITY_HUMIDITY    = "sensor.dashboard_humidity";
 

// -----------------------------------------------------------------------------
// Shared sizing constants (used across main.cpp and the screen_*.cpp files)
// -----------------------------------------------------------------------------
#define MAX_STR        64    // max length of cached state strings
#define FORECAST_DAYS  3     // number of forecast days shown
#define MAX_CAL_EVENTS 20    // max calendar events held



// -----------------------------------------------------------------------------
// Security page (Key 2) — on-demand fetch only, never on timer wakes.
// -----------------------------------------------------------------------------
constexpr const char* SEC_ALARM_ENTITY   = "alarm_control_panel.home_alarm";
constexpr const char* SEC_ALARM_MODE     = "sensor.home_alarm_state";
constexpr const char* SEC_EVENT_MSG      = "sensor.home_alarm_event";
 
struct SecDoor   { const char* entity; const char* label; };
struct SecWindow { const char* entity; const char* label; };
struct SecMotion { const char* entity; const char* label; };
struct SecCamera { const char* motion_entity; const char* label; };
 
const SecDoor SEC_DOORS[] = {
  { "binary_sensor.front_door", "Front door" },
  { "binary_sensor.back_door",  "Back door"  },
};
const int SEC_DOOR_COUNT = sizeof(SEC_DOORS) / sizeof(SEC_DOORS[0]);
 
const SecWindow SEC_WINDOWS[] = {
  { "binary_sensor.window_kitchen",       "Kitchen"      },
  { "binary_sensor.window_living_room",   "Living room"  },
  { "binary_sensor.window_side",          "Side windows" },
  { "binary_sensor.window_bedroom_left",  "Bedroom L"    },
  { "binary_sensor.window_bedroom_right", "Bedroom R"    },
  { "binary_sensor.window_office_left",   "Office L"     },
  { "binary_sensor.window_office_right",  "Office R"     },
};
const int SEC_WINDOW_COUNT = sizeof(SEC_WINDOWS) / sizeof(SEC_WINDOWS[0]);
 
const SecMotion SEC_MOTIONS[] = {
  { "binary_sensor.front_door_motion",      "Front door"  },
  { "binary_sensor.hall_motion",            "Hall"        },
  { "binary_sensor.landing_motion",         "Landing"     },
  { "binary_sensor.recessed_landing_motion","Rec. Landing"},
  { "binary_sensor.back_garden_motion",     "Back garden" },
};
const int SEC_MOTION_COUNT = sizeof(SEC_MOTIONS) / sizeof(SEC_MOTIONS[0]);
 
const SecCamera SEC_CAMERAS[] = {
  { "binary_sensor.camera_front_motion",         "Front camera" },
  { "binary_sensor.camera_back_motion",          "Back camera"  },
  { "binary_sensor.camera_kitchen_motion",       "Kitchen"      },
  { "binary_sensor.doorbell_recent_motion",     "Doorbell"     },
};
const int SEC_CAMERA_COUNT = sizeof(SEC_CAMERAS) / sizeof(SEC_CAMERAS[0]);
 