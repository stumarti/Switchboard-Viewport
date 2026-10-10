// =============================================================================
// display.h — the 7.3" Spectra 6 panel (GDEP073E01), driven exactly as the
// kitchen panel drove it: GxEPD2's 7-colour driver over its own SPI pins,
// paged (the draw runs once per band of the panel), a full refresh, then
// hibernate.
// =============================================================================
#pragma once
#include <stdint.h>
#include <functional>
#include "render/draw.h"

namespace display {

void begin();
// One full refresh of whatever `paint` draws (it may run several times, once
// per page: it must draw the same thing each time).
void show(const std::function<void(draw::Gfx&)>& paint);
// Fill the panel white.
void clear();
// How long the last refresh took, in ms (0 before the first one).
uint32_t lastRefreshMs();

}  // namespace display
