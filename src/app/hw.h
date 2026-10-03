// =============================================================================
// hw.h — the reTerminal E1002's own bits: battery, the SHT40, the LED, and
// which button woke it (and whether it was held).
// =============================================================================
#pragma once
#include <stdint.h>

namespace hw {

enum class Wake : uint8_t {
  PowerOn,      // first power-on, a reset or a flash
  Timer,        // the refresh the server scheduled
  Home,         // right, green (KEY0): the first screen, fetched afresh
  Next,         // middle (KEY1)
  Prev,         // left (KEY2)
  SetupHold,    // right, held: Wi-Fi setup
  ClearHold,    // middle, held: fill the panel white
  InfoHold      // left, held: the button card and device details
};

void begin();
// What woke it; a held button is told apart by waiting LONG_PRESS_MS and
// reading it again, as the kitchen panel did.
Wake wakeReason();
const char* wakeName(Wake w);

float batteryVoltage();
int batteryPercent(float volts);
int batteryPercent();

// The SHT40: false if it didn't answer.
bool climate(float& tempC, float& humidity);

void led(bool on);
// Any button pressed right now (to cut a wait short).
bool anyButton();

// Deep sleep until `seconds` pass or a button is pressed.
[[noreturn]] void sleepFor(uint32_t seconds);

}  // namespace hw
