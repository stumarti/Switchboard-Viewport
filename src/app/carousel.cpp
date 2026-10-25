#include "app/carousel.h"

#include <string.h>

namespace carousel {

Mode modeOf(const char* s) {
  if (s && !strcmp(s, "advance")) return Mode::Advance;
  if (s && !strcmp(s, "returnFirst")) return Mode::ReturnFirst;
  return Mode::Stay;
}

static void put(char* dst, size_t cap, const char* s) {
  if (!cap) return;
  strncpy(dst, s ? s : "", cap - 1);
  dst[cap - 1] = 0;
}

void takeover(Takeover& t, const char* picked, const char* switchTo, bool pressed, char* out, size_t cap) {
  put(out, cap, picked);
  const char* sw = switchTo ? switchTo : "";
  if (pressed) {
    // Where the press put it, and that screen doesn't take over again
    // until its rule ends.
    if (t.active[0]) put(t.dismissed, sizeof(t.dismissed), t.active);
    t.active[0] = t.returnTo[0] = 0;
    return;
  }
  if (!sw[0]) {
    // Nothing wants the display: back to where it was.
    t.dismissed[0] = 0;
    if (t.active[0]) {
      if (t.returnTo[0]) put(out, cap, t.returnTo);
      t.active[0] = t.returnTo[0] = 0;
    }
    return;
  }
  if (!strcmp(sw, t.dismissed)) return;
  if (!t.active[0]) put(t.returnTo, sizeof(t.returnTo), picked);
  put(t.active, sizeof(t.active), sw);
  put(out, cap, sw);
}

int pick(int count, int current, Wake wake, const Plan& plan, int64_t now, int64_t lastPress, int64_t lastChange) {
  if (count <= 0) return 0;
  if (current < 0 || current >= count) current = 0;
  const int64_t every = static_cast<int64_t>(plan.everyMin) * 60;
  switch (wake) {
    case Wake::Next:
      return (current + 1) % count;
    case Wake::Prev:
      return (current + count - 1) % count;
    case Wake::Home:
      return 0;
    case Wake::Boot:
      return current;
    case Wake::Timer:
      break;
  }
  if (plan.mode == Mode::Advance && count > 1) {
    // Not knowing when it last moved: move now.
    if (!lastChange || !now || now - lastChange >= every) return (current + 1) % count;
  }
  if (plan.mode == Mode::ReturnFirst && current != 0) {
    if (!lastPress || !now || now - lastPress >= every) return 0;
  }
  return current;
}

uint32_t sleepSec(uint32_t serverSec, int current, const Plan& plan, int64_t now, int64_t lastPress, int64_t lastChange,
                  uint32_t minSec, uint32_t maxSec) {
  int64_t s = serverSec ? serverSec : maxSec;
  const int64_t every = static_cast<int64_t>(plan.everyMin) * 60;
  if (now) {
    int64_t due = -1;
    if (plan.mode == Mode::Advance && lastChange) due = lastChange + every - now;
    if (plan.mode == Mode::ReturnFirst && current != 0 && lastPress) due = lastPress + every - now;
    if (due >= 0 && due < s) s = due;
  }
  if (s < minSec) s = minSec;
  if (s > maxSec) s = maxSec;
  return static_cast<uint32_t>(s);
}

}  // namespace carousel
