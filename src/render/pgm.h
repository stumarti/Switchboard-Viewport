// PROGMEM and pgm_read_byte: Arduino's on the device, plain memory in the
// host tests.
#pragma once
#ifdef SB_HOST
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#endif
#else
#include <pgmspace.h>
#endif
