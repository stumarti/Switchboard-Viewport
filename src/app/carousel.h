// =============================================================================
// carousel.h — which screen to show, and how long to sleep (pure; host-
// tested in test/host/test_main.cpp).
//
// The layout's screens are the carousel. The right button steps forward, the
// left back (wrapping), the middle refreshes the one showing. Between
// presses, the layout's carousel mode says what a timer wake does:
//   stay         refresh the screen showing
//   advance      move on every `everyMin` minutes
//   returnFirst  go back to the first screen `everyMin` minutes after the
//                last press (the kitchen panel: Heating or Security until
//                the next refresh, then Status again)
// =============================================================================
#pragma once
#include <stdint.h>

namespace carousel {

enum class Wake : uint8_t {
  Boot,     // power-on, or a reset: the screen it was on, else the first
  Timer,    // the server's refresh time came round
  Refresh,  // middle button
  Next,     // right button
  Prev      // left button
};

enum class Mode : uint8_t { Stay, Advance, ReturnFirst };
Mode modeOf(const char* s);

struct Plan {
  Mode mode = Mode::Stay;
  uint32_t everyMin = 30;
};

// The screen to show (an index into the enabled screens) for this wake.
// `now`, `lastPress` and `lastChange` are seconds (epoch); 0 = unknown.
int pick(int count, int current, Wake wake, const Plan& plan, int64_t now, int64_t lastPress, int64_t lastChange);

// How long to sleep: what the server asked (refreshInSec), sooner when the
// carousel has something due (an advance, a return to the first screen),
// never under `minSec` nor over `maxSec`.
uint32_t sleepSec(uint32_t serverSec, int current, const Plan& plan, int64_t now, int64_t lastPress, int64_t lastChange,
                  uint32_t minSec, uint32_t maxSec);

}  // namespace carousel
