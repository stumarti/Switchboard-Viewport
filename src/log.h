// Logging to the reTerminal's UART (pins 44/43), as the kitchen panel did.
#pragma once
#include <Arduino.h>
#define LOGF(...) Serial1.printf(__VA_ARGS__)
