// Host tests for the firmware's pure logic (test/host/run.sh).
#include <stdio.h>
#include <string.h>

#include "app/carousel.h"

static int g_failed = 0, g_run = 0;
#define CHECK_EQ(a, b)                                                                              \
  do {                                                                                             \
    ++g_run;                                                                                       \
    const long long _a = static_cast<long long>(a), _b = static_cast<long long>(b);                \
    if (_a != _b) {                                                                                \
      ++g_failed;                                                                                  \
      fprintf(stderr, "%s:%d: %s == %lld, expected %lld\n", __FILE__, __LINE__, #a, _a, _b);       \
    }                                                                                              \
  } while (0)

using carousel::Mode;
using carousel::Plan;
using carousel::Wake;

static void buttons() {
  Plan p;
  // Right steps on, left steps back, both wrap; the middle stays.
  CHECK_EQ(carousel::pick(3, 0, Wake::Next, p, 0, 0, 0), 1);
  CHECK_EQ(carousel::pick(3, 2, Wake::Next, p, 0, 0, 0), 0);
  CHECK_EQ(carousel::pick(3, 0, Wake::Prev, p, 0, 0, 0), 2);
  CHECK_EQ(carousel::pick(3, 1, Wake::Refresh, p, 0, 0, 0), 1);
  // A screen that's gone (the layout lost one): the first.
  CHECK_EQ(carousel::pick(2, 5, Wake::Boot, p, 0, 0, 0), 0);
  CHECK_EQ(carousel::pick(0, 0, Wake::Next, p, 0, 0, 0), 0);
}

static void kitchenPanel() {
  // returnFirst every 30: Heating after a press stays until the next wake
  // half an hour on, then it's Status again.
  Plan p{Mode::ReturnFirst, 30};
  const int64_t t = 1'700'000'000;
  CHECK_EQ(carousel::pick(3, 1, Wake::Timer, p, t + 10 * 60, t, t), 1);
  CHECK_EQ(carousel::pick(3, 1, Wake::Timer, p, t + 30 * 60, t, t), 0);
  CHECK_EQ(carousel::pick(3, 0, Wake::Timer, p, t + 90 * 60, t, t), 0);
  // It sleeps no longer than until it's due back on Status...
  CHECK_EQ(carousel::sleepSec(1800, 1, p, t + 10 * 60, t, t, 60, 43200), 20 * 60);
  // ...and once on Status, as long as the server says (quiet hours: an hour).
  CHECK_EQ(carousel::sleepSec(3600, 0, p, t + 10 * 60, t, t, 60, 43200), 3600);
  // Never pressed, clock unknown: straight back.
  CHECK_EQ(carousel::pick(3, 2, Wake::Timer, p, 0, 0, 0), 0);
}

static void advancing() {
  Plan p{Mode::Advance, 10};
  const int64_t t = 1'700'000'000;
  CHECK_EQ(carousel::pick(3, 0, Wake::Timer, p, t + 5 * 60, 0, t), 0);
  CHECK_EQ(carousel::pick(3, 0, Wake::Timer, p, t + 10 * 60, 0, t), 1);
  CHECK_EQ(carousel::pick(3, 2, Wake::Timer, p, t + 10 * 60, 0, t), 0);
  CHECK_EQ(carousel::sleepSec(1800, 0, p, t + 4 * 60, 0, t, 60, 43200), 6 * 60);
  // One screen: nothing to advance to.
  CHECK_EQ(carousel::pick(1, 0, Wake::Timer, p, t + 60 * 60, 0, t), 0);
}

static void staying() {
  Plan p{Mode::Stay, 30};
  CHECK_EQ(carousel::pick(3, 2, Wake::Timer, p, 1'700'000'000, 1, 1), 2);
  // The server's time, within the panel's limits.
  CHECK_EQ(carousel::sleepSec(1800, 2, p, 1'700'000'000, 1, 1, 60, 43200), 1800);
  CHECK_EQ(carousel::sleepSec(5, 2, p, 1'700'000'000, 1, 1, 60, 43200), 60);
  CHECK_EQ(carousel::sleepSec(0, 2, p, 1'700'000'000, 1, 1, 60, 43200), 43200);
  CHECK_EQ(carousel::sleepSec(99999, 2, p, 1'700'000'000, 1, 1, 60, 43200), 43200);
  CHECK_EQ(static_cast<int>(carousel::modeOf("returnFirst")), static_cast<int>(Mode::ReturnFirst));
  CHECK_EQ(static_cast<int>(carousel::modeOf("bogus")), static_cast<int>(Mode::Stay));
}

int main() {
  buttons();
  kitchenPanel();
  advancing();
  staying();
  printf("%d checks, %d failed\n", g_run, g_failed);
  return g_failed ? 1 : 0;
}
