#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
  size_t print(int v) {
    char b[16];
    snprintf(b, sizeof(b), "%d", v);
    return print(b);
  }
};
