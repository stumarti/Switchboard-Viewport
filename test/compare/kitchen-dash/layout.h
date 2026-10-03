// =============================================================================
// layout.h — Display layout constants for the reTerminal E1002 dashboard
// Panel: 800 x 480 px, Spectra-6 6-color e-paper
//
// Adjust these to reposition elements without touching the screen files.
// =============================================================================
#pragma once

// -----------------------------------------------------------------------------
// Panel dimensions
// -----------------------------------------------------------------------------
#define PANEL_W     800
#define PANEL_H     480

// -----------------------------------------------------------------------------
// Columns
// -----------------------------------------------------------------------------
#define LC_X    5
#define LC_W    235
#define DIV_X   250
#define RC_X    260
#define RC_STATUS_H  50

// Right column text geometry
#define RC_LINE_H   40
#define RC_TEXT_X   (RC_X + 10)
#define ITEM_H      40
#define ITEM_GAP    5
#define TODAY_GAP   21

// -----------------------------------------------------------------------------
// Weather block
// -----------------------------------------------------------------------------
#define WX_ICON_X   152
#define WX_ICON_Y   2
#define TEMP_X      LC_X
#define TEMP_Y      70
#define DETAIL_Y    106
#define RAIN_SOLAR_Y 132  // rain/solar forecast line — just below detail row
#define MINI_FC_Y   152   // mini 3-day forecast strip — below rain/solar line
#define MINI_ICON_S 32    // mini weather icon size in px

// -----------------------------------------------------------------------------
// Energy grid
// -----------------------------------------------------------------------------
#define ENERGY_Y    268
#define COL_L       LC_X
#define COL_R       130
#define ROW_H       88

// -----------------------------------------------------------------------------
// Battery section
// -----------------------------------------------------------------------------
#define BATT_Y      432
#define BAR_Y       455
#define BAR_H       22
#define BAR_W       (LC_W - 10)
#define BAR_NUB_W   6
#define BAR_NUB_H   12

// -----------------------------------------------------------------------------
// Footer
// -----------------------------------------------------------------------------
#define FOOTER_Y        468
#define FOOTER_ICON_Y   (FOOTER_Y - 18)
#define FOOTER_TINY_Y   (FOOTER_Y - 13)
#define FOOTER_MARGIN_R 12
#define FOOTER_GAP      10