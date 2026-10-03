#pragma once
#include <string>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "WString.h"

class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t c) = 0;
  size_t write(const uint8_t* b, size_t n) {
    size_t k = 0;
    while (n--) k += write(*b++);
    return k;
  }
  size_t print(const char* s) { return write(reinterpret_cast<const uint8_t*>(s), strlen(s)); }
  size_t print(char c) { return write(static_cast<uint8_t>(c)); }
  size_t print(const String& s) { return print(s.c_str()); }
  size_t println(const String& s) { return print(s.c_str()) + print("\n"); }
  size_t println(const char* s = "") { return print(s) + print("\n"); }
  // As Arduino-ESP32's Print::printFloat: half away from zero, no padding.
  size_t print(double number, int digits = 2) {
    if (number != number) return print("nan");
    if (number - number != 0) return print("inf");
    std::string out;
    if (number < 0.0) {
      out += '-';
      number = -number;
    }
    double rounding = 0.5;
    for (int i = 0; i < digits; ++i) rounding /= 10.0;
    number += rounding;
    unsigned long intPart = static_cast<unsigned long>(number);
    double remainder = number - static_cast<double>(intPart);
    out += std::to_string(intPart);
    if (digits > 0) out += '.';
    while (digits-- > 0) {
      remainder *= 10.0;
      const unsigned toPrint = static_cast<unsigned>(remainder);
      out += std::to_string(toPrint);
      remainder -= toPrint;
    }
    return print(out.c_str());
  }
  size_t print(float v, int d = 2) { return print(static_cast<double>(v), d); }
  size_t print(unsigned v) {
    char b[16];
    snprintf(b, sizeof(b), "%u", v);
    return print(b);
  }
  size_t print(long v) {
    char b[24];
    snprintf(b, sizeof(b), "%ld", v);
    return print(b);
  }
  template <typename... A>
  size_t printf(const char* f, A... a) {
    char b[512];
    snprintf(b, sizeof(b), f, a...);
    return print(b);
  }
  size_t print(int v) {
    char b[16];
    snprintf(b, sizeof(b), "%d", v);
    return print(b);
  }
};
