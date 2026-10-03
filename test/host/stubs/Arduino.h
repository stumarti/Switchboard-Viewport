// Host stand-in for the bits of Arduino the firmware's drawing code and the
// kitchen panel's asset headers use.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "pgmspace.h"
#include "Print.h"

// Arduino's String, as far as the panel's headers use it (comparisons).
class String {
 public:
  String(const char* s = "") : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
  bool operator==(const char* o) const { return s_ == (o ? o : ""); }
  bool operator==(const String& o) const { return s_ == o.s_; }

 private:
  std::string s_;
};

inline unsigned long millis() { return 0; }
