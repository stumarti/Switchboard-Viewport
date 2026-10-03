// Force-included into the kitchen panel's own sources on the host: the
// ESP32/Arduino calls they make, answered so it runs as it did on the panel —
// at the captured moment, against the captured Home Assistant (house.json).
#pragma once
#include <time.h>
#include "Arduino.h"

#define RTC_DATA_ATTR
#define SERIAL_8N1 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LOW 0
#define HIGH 1
#define ADC_11db 3

struct HostSerial : public Print {
  size_t write(uint8_t) override { return 1; }  // quiet
  void begin(unsigned long, int = 0, int = 0, int = 0) {}
  void flush() {}
};
extern HostSerial Serial1;

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline void delay(unsigned long) {}
inline void analogReadResolution(int) {}
inline void analogSetPinAttenuation(int, int) {}
extern int g_batteryMilliVolts;  // what the ADC reads (half the battery)
inline int analogReadMilliVolts(int) { return g_batteryMilliVolts; }

// The clock, stopped at the captured moment, in the panel's time zone.
extern time_t g_now;
inline bool getLocalTime(struct tm* t, uint32_t = 0) {
  localtime_r(&g_now, t);
  return true;
}
inline void configTzTime(const char* tz, const char*, const char* = nullptr, const char* = nullptr) {
  setenv("TZ", tz, 1);
  tzset();
}

typedef int esp_sleep_wakeup_cause_t;
#define ESP_SLEEP_WAKEUP_EXT1 3
#define ESP_EXT1_WAKEUP_ANY_LOW 1
inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return 0; }
inline uint64_t esp_sleep_get_ext1_wakeup_status() { return 0; }
inline void esp_sleep_enable_timer_wakeup(uint64_t) {}
inline void esp_sleep_enable_ext1_wakeup(uint64_t, int) {}
inline void esp_deep_sleep_start() {}
