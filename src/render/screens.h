// =============================================================================
// screens.h — drawing a screen from Switchboard Server's state.
//
// A "sections" screen is laid out as the kitchen panel was: a template's
// columns, each a stack of sections drawn top to bottom, then the footer. The
// sections the panel had (weather, energy, home battery, status icons, now,
// calendar, heating, alarm, doors and windows, motion, cameras) are its own
// drawing code, moved from its globals onto the server's values, at its own
// coordinates; the rest follow the server's preview in the panel's fonts.
//
// Every icon or picture a screen needs that isn't built in must be fetched
// before it's drawn (a draw never touches the network): render once onto a
// NullCanvas with icons::collecting on, fetch what it asked for, then draw.
// =============================================================================
#pragma once
#include <ArduinoJson.h>
#include "render/draw.h"

namespace screens {

struct Ctx {
  char time[8] = "--:--";  // the footer's clock: when this was drawn
  int battPct = -1;        // the device's own battery, -1 = unknown
  bool quiet = false;      // quiet hours: the footer's bed-and-clock
  // The carousel: each enabled screen's icon, in order, and the one showing
  // (underlined in the footer). Fewer than two: none drawn.
  const char* marks[12] = {};
  int markCount = 0;
  int current = 0;
};

// A screen as /api/viewports/me/state?screen=<id> returns it (its `data`).
void drawScreen(draw::Gfx& g, JsonObjectConst screen, const Ctx& ctx);

// The footer every sections screen ends with: refresh icon, time, battery.
void footer(draw::Gfx& g, const Ctx& ctx);

// An Adafruit_GFX that draws nothing: for the collecting pass.
class NullCanvas : public Adafruit_GFX {
 public:
  NullCanvas() : Adafruit_GFX(draw::PANEL_W, draw::PANEL_H) {}
  void drawPixel(int16_t, int16_t, uint16_t) override {}
};

}  // namespace screens
