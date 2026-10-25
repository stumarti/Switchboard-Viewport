// =============================================================================
// carousel.h — which screen to show, and how long to sleep (pure; host-
// tested in test/host/test_main.cpp).
//
// The layout's screens are the carousel. On every screen the buttons are the
// same: left back, middle on (both wrapping), and the green one on the right
// home (the first screen). Between presses, the layout's carousel mode says
// what a timer wake does:
//   stay         refresh the screen showing
//   advance      move on every `everyMin` minutes
//   returnFirst  go back to the first screen `everyMin` minutes after the
//                last press (the kitchen panel: Heating or Security until
//                the next refresh, then Status again)
// =============================================================================
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace carousel {

enum class Wake : uint8_t {
  Boot,     // power-on, or a reset: the screen it was on, else the first
  Timer,    // the server's refresh time came round
  Home,     // right (green) button: the first screen
  Next,     // middle button
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

// A screen that takes over by itself (the layout's switchWhen; the server
// says which in X-Switch-To while its rule holds): the display goes there,
// stays while it's named, and goes back to the screen it was on after. A
// button press stays where it's put, and that screen doesn't take over
// again until its rule has ended. Kept through deep sleep (RTC memory).
struct Takeover {
  char active[40] = "";     // the screen that took over
  char returnTo[40] = "";   // the one it took over from
  char dismissed[40] = "";  // one a press moved away from, while its rule holds
};
// The screen to show (into `out`): `picked` (the carousel's choice) unless a
// takeover says otherwise. `switchTo`: the server's, "" for none (or not a
// screen this display has); `pressed`: a button woke it.
void takeover(Takeover& t, const char* picked, const char* switchTo, bool pressed, char* out, size_t cap);

// How long to sleep: what the server asked (refreshInSec), sooner when the
// carousel has something due (an advance, a return to the first screen),
// never under `minSec` nor over `maxSec`.
uint32_t sleepSec(uint32_t serverSec, int current, const Plan& plan, int64_t now, int64_t lastPress, int64_t lastChange,
                  uint32_t minSec, uint32_t maxSec);

}  // namespace carousel
