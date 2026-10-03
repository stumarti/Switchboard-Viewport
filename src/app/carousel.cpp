#include "app/carousel.h"

#include <string.h>

namespace carousel {

Mode modeOf(const char* s) {
  if (s && !strcmp(s, "advance")) return Mode::Advance;
  if (s && !strcmp(s, "returnFirst")) return Mode::ReturnFirst;
  return Mode::Stay;
}

int pick(int count, int current, Wake wake, const Plan& plan, int64_t now, int64_t lastPress, int64_t lastChange) {
  if (count <= 0) return 0;
  if (current < 0 || current >= count) current = 0;
  const int64_t every = static_cast<int64_t>(plan.everyMin) * 60;
  if (plan.direct) {
    switch (wake) {
      case Wake::Refresh: return 0;
      case Wake::Next: return count > 1 ? 1 : 0;
      case Wake::Prev: return count - 1;
      default: break;
    }
  }
  switch (wake) {
    case Wake::Next:
      return (current + 1) % count;
    case Wake::Prev:
      return (current + count - 1) % count;
    case Wake::Refresh:
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
