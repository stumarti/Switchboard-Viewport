// Host tests for the firmware's pure logic (test/host/run.sh).
#include <stdio.h>
#include <string.h>

#include "app/battery.h"
#include "app/carousel.h"

#include <ArduinoJson.h>
#include <fstream>
#include <sstream>
#include <string>

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
  // The middle steps on, the left back, both wrapping; the green one goes home.
  CHECK_EQ(carousel::pick(3, 0, Wake::Next, p, 0, 0, 0), 1);
  CHECK_EQ(carousel::pick(3, 2, Wake::Next, p, 0, 0, 0), 0);
  CHECK_EQ(carousel::pick(3, 0, Wake::Prev, p, 0, 0, 0), 2);
  CHECK_EQ(carousel::pick(3, 1, Wake::Home, p, 0, 0, 0), 0);
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

static void homeNextPrevious() {
  // On every screen: home, next and previous, whatever the carousel mode. From
  // Status that's the kitchen panel's own buttons (green Status, middle
  // Heating, left Security).
  for (Mode m : {Mode::Stay, Mode::Advance, Mode::ReturnFirst}) {
    Plan p{m, 30};
    for (int from = 0; from < 3; ++from) {
      CHECK_EQ(carousel::pick(3, from, Wake::Home, p, 0, 0, 0), 0);
      CHECK_EQ(carousel::pick(3, from, Wake::Next, p, 0, 0, 0), (from + 1) % 3);
      CHECK_EQ(carousel::pick(3, from, Wake::Prev, p, 0, 0, 0), (from + 2) % 3);
    }
  }
  // One screen: every button shows it.
  Plan one;
  CHECK_EQ(carousel::pick(1, 0, Wake::Next, one, 0, 0, 0), 0);
  CHECK_EQ(carousel::pick(1, 0, Wake::Prev, one, 0, 0, 0), 0);
  CHECK_EQ(carousel::pick(1, 0, Wake::Home, one, 0, 0, 0), 0);
}

static void bundleParses() {
  std::ifstream f("test/fixtures/bundles/kitchen.json");
  std::stringstream ss;
  ss << f.rdbuf();
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, ss.str());
  if (err) fprintf(stderr, "bundle: %s\n", err.c_str());
  CHECK_EQ(static_cast<bool>(err), false);
  CHECK_EQ(doc["layout"]["screens"].size(), 3);
  CHECK_EQ(strcmp(doc["layout"]["carousel"]["buttons"] | "", "direct"), 0);
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

static void batteryReading() {
  // One wild reading either way doesn't move it: the middle half, averaged.
  int mv[16] = {1950, 1952, 1948, 1951, 1949, 1950, 2400, 1500, 1953, 1947, 1950, 1951, 1949, 1950, 1952, 1948};
  CHECK_EQ(battery::trimmedMeanMv(mv, 16), 1950);
  int one[1] = {1900};
  CHECK_EQ(battery::trimmedMeanMv(one, 1), 1900);
  CHECK_EQ(battery::trimmedMeanMv(one, 0), 0);
  // The curve: its ends, a point on it, and between two.
  CHECK_EQ(battery::percent(4.20f), 100);
  CHECK_EQ(battery::percent(3.20f), 0);
  CHECK_EQ(battery::percent(3.80f), 60);
  CHECK_EQ(battery::percent(3.825f), 65);
}

static void takingOver() {
  // A screen whose rule holds takes over, and the one before comes back after.
  carousel::Takeover t;
  char show[64];
  auto is = [&](const char* want) { CHECK_EQ(strcmp(show, want), 0); };
  carousel::takeover(t, "home", "", false, show, sizeof(show));
  is("home");
  carousel::takeover(t, "home", "music", false, show, sizeof(show));
  is("music");
  // While it holds, it stays, whatever the carousel picked.
  carousel::takeover(t, "home", "music", false, show, sizeof(show));
  is("music");
  // Another one starts: it goes there, still coming back to home after.
  carousel::takeover(t, "music", "alert", false, show, sizeof(show));
  is("alert");
  carousel::takeover(t, "alert", "", false, show, sizeof(show));
  is("home");
  // A press while it's up: where the press put it, and it doesn't take
  // over again until its rule has ended.
  carousel::takeover(t, "home", "music", false, show, sizeof(show));
  is("music");
  carousel::takeover(t, "heating", "music", true, show, sizeof(show));
  is("heating");
  carousel::takeover(t, "heating", "music", false, show, sizeof(show));
  is("heating");
  carousel::takeover(t, "heating", "", false, show, sizeof(show));
  is("heating");
  carousel::takeover(t, "heating", "music", false, show, sizeof(show));
  is("music");
}

int main() {
  takingOver();
  buttons();
  kitchenPanel();
  homeNextPrevious();
  bundleParses();
  advancing();
  staying();
  batteryReading();
  printf("%d checks, %d failed\n", g_run, g_failed);
  return g_failed ? 1 : 0;
}
