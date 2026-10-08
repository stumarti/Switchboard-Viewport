// =============================================================================
// display.h — the 7.3" Spectra 6 panel (GDEP073E01), driven exactly as the
// kitchen panel drove it: GxEPD2's 7-colour driver over its own SPI pins,
// paged (the draw runs once per band of the panel), a full refresh, then
// hibernate.
// =============================================================================
#pragma once
#include <functional>
#include "render/draw.h"

namespace display {

void begin();
// One full refresh of whatever `paint` draws (it may run several times, once
// per page: it must draw the same thing each time). `rotation`: how the
// display hangs, turned that many degrees clockwise (0, 90, 180, 270); at 90
// and 270 the GFX `paint` gets is 480x800.
void show(const std::function<void(draw::Gfx&)>& paint, int rotation = 0);
// The layout about to be drawn (before anything measures or fetches for
// it): how it hangs, and on a 13.3" board, large (twice the size) or not.
// The next show() draws it so, then the board goes back to its defaults
// (the setup and error screens). The E1002 has nothing to change.
void use(int rotation, bool large);
// Fill the panel white.
void clear();

}  // namespace display
