#pragma once

// ===========================================================================
// battery — the panel's battery level from its voltage (pure; host-tested).
// hw.cpp does the reading.
//
//   trimmedMeanMv  one ADC reading wanders by tens of millivolts, which on
//                  this curve is several percent: the server's battery-life
//                  estimate saw that as charging and draining. So the panel
//                  takes kSamples readings and averages the middle half.
//   percent        the voltage through the discharge curve, 0..100
// ===========================================================================

#include <stdint.h>

namespace battery {

constexpr int kSamples = 16;

// The mean of the middle half of `mv[0..n)`, once sorted (sorts `mv` in
// place). 0 for none.
inline int trimmedMeanMv(int* mv, int n) {
  if (n <= 0) return 0;
  for (int i = 1; i < n; i++) {
    const int v = mv[i];
    int j = i - 1;
    while (j >= 0 && mv[j] > v) {
      mv[j + 1] = mv[j];
      j--;
    }
    mv[j + 1] = v;
  }
  const int drop = n / 4;
  long sum = 0;
  for (int i = drop; i < n - drop; i++) sum += mv[i];
  return static_cast<int>((sum + (n - 2 * drop) / 2) / (n - 2 * drop));
}

inline int percent(float v) {
  static const float volts[] = {4.15f, 3.96f, 3.91f, 3.85f, 3.80f, 3.75f, 3.68f, 3.58f, 3.49f, 3.41f, 3.30f, 3.27f};
  static const float pcts[] = {100.f, 90.f, 80.f, 70.f, 60.f, 50.f, 40.f, 30.f, 20.f, 10.f, 5.f, 0.f};
  const int n = sizeof(volts) / sizeof(volts[0]);
  if (v >= volts[0]) return 100;
  if (v <= volts[n - 1]) return 0;
  for (int i = 0; i < n - 1; i++) {
    if (v <= volts[i] && v > volts[i + 1]) {
      const float frac = (v - volts[i + 1]) / (volts[i] - volts[i + 1]);
      return static_cast<int>(pcts[i + 1] + frac * (pcts[i] - pcts[i + 1]) + 0.5f);
    }
  }
  return 0;
}

}  // namespace battery
