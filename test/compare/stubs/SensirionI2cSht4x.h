#pragma once
#include <stdint.h>
#include "Wire.h"
#define SHT40_I2C_ADDR_44 0x44
struct SensirionI2cSht4x {
  void begin(HostWire&, uint8_t) {}
  uint16_t measureHighPrecision(float& t, float& h) { t = 21.5f; h = 48.0f; return 0; }
};
