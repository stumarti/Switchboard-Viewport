#include "app/hw.h"

#include <Arduino.h>
#include <SensirionI2cSht4x.h>
#include <Wire.h>
#include <esp_sleep.h>

#include "config.h"
#include "log.h"

namespace hw {

void begin() {
  pinMode(LED_PIN, OUTPUT);
  led(false);
  pinMode(SD_EN_PIN, OUTPUT);
  digitalWrite(SD_EN_PIN, HIGH);
  pinMode(BTN_KEY0, INPUT_PULLUP);
  pinMode(BTN_KEY1, INPUT_PULLUP);
  pinMode(BTN_KEY2, INPUT_PULLUP);
}

void led(bool on) { digitalWrite(LED_PIN, on ? LOW : HIGH); }

bool anyButton() { return digitalRead(BTN_KEY0) == LOW || digitalRead(BTN_KEY1) == LOW || digitalRead(BTN_KEY2) == LOW; }

static bool held(int pin) {
  pinMode(pin, INPUT_PULLUP);
  delay(LONG_PRESS_MS);
  return digitalRead(pin) == LOW;  // buttons are active-low
}

Wake wakeReason() {
  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_TIMER) return Wake::Timer;
  if (cause != ESP_SLEEP_WAKEUP_EXT1) return Wake::PowerOn;
  const uint64_t mask = esp_sleep_get_ext1_wakeup_status();
  if (mask & (1ULL << BTN_KEY0)) return held(BTN_KEY0) ? Wake::SetupHold : Wake::Refresh;
  if (mask & (1ULL << BTN_KEY1)) return held(BTN_KEY1) ? Wake::ClearHold : Wake::Next;
  if (mask & (1ULL << BTN_KEY2)) return held(BTN_KEY2) ? Wake::InfoHold : Wake::Prev;
  return Wake::Refresh;
}

const char* wakeName(Wake w) {
  switch (w) {
    case Wake::PowerOn: return "power-on";
    case Wake::Timer: return "timer";
    case Wake::Refresh: return "middle (refresh)";
    case Wake::Next: return "right (next)";
    case Wake::Prev: return "left (previous)";
    case Wake::SetupHold: return "middle held (Wi-Fi setup)";
    case Wake::ClearHold: return "right held (clear)";
    case Wake::InfoHold: return "left held (info)";
  }
  return "?";
}

// The kitchen panel's battery reading: the monitor switched on, the ADC
// through its /2 divider, and its discharge curve.
float batteryVoltage() {
  pinMode(BATTERY_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_ENABLE_PIN, HIGH);
  delay(10);
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  const int mv = analogReadMilliVolts(BATTERY_ADC_PIN);
  digitalWrite(BATTERY_ENABLE_PIN, LOW);
  return (mv / 1000.0f) * 2.0f;
}

int batteryPercent(float v) {
  static const float volts[] = {4.15f, 3.96f, 3.91f, 3.85f, 3.80f, 3.75f, 3.68f, 3.58f, 3.49f, 3.41f, 3.30f, 3.27f};
  static const float pcts[] = {100.f, 90.f, 80.f, 70.f, 60.f, 50.f, 40.f, 30.f, 20.f, 10.f, 5.f, 0.f};
  const int n = sizeof(volts) / sizeof(volts[0]);
  if (v >= volts[0]) return 100;
  if (v <= volts[n - 1]) return 0;
  for (int i = 0; i < n - 1; i++) {
    if (v <= volts[i] && v > volts[i + 1]) {
      const float frac = (v - volts[i + 1]) / (volts[i] - volts[i + 1]);
      return static_cast<int>(pcts[i + 1] + frac * (pcts[i] - pcts[i + 1]) + 0.5f);
    }
  }
  return 0;
}

int batteryPercent() { return batteryPercent(batteryVoltage()); }

bool climate(float& tempC, float& humidity) {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  SensirionI2cSht4x sht4x;
  sht4x.begin(Wire, SHT40_I2C_ADDR_44);
  tempC = NAN;
  humidity = NAN;
  const int16_t err = sht4x.measureHighPrecision(tempC, humidity);
  if (err) {
    LOGF("SHT40 error %d\n", err);
    return false;
  }
  return true;
}

void sleepFor(uint32_t seconds) {
  // The Spectra panel can still be settling after hibernate() returns.
  if (PRE_SLEEP_DELAY_MS > 0) delay(PRE_SLEEP_DELAY_MS);
  LOGF("Sleeping %u s\n", static_cast<unsigned>(seconds));
  Serial1.flush();
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);
  const uint64_t buttons = (1ULL << BTN_KEY0) | (1ULL << BTN_KEY1) | (1ULL << BTN_KEY2);
  esp_sleep_enable_ext1_wakeup(buttons, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
  for (;;) {
  }
}

}  // namespace hw
