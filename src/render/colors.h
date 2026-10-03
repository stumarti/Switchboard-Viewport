// The panel's colours as GxEPD2 names them (the 16-bit values its 7-colour
// driver maps to inks). The host tests define the same values themselves.
#pragma once
#ifdef SB_HOST
#define GxEPD_WHITE  0xFFFF
#define GxEPD_BLACK  0x0000
#define GxEPD_RED    0xF800
#define GxEPD_YELLOW 0xFFE0
#define GxEPD_GREEN  0x07E0
#define GxEPD_BLUE   0x001F
#define GxEPD_ORANGE 0xFC00
#else
#include <GxEPD2.h>
#endif
