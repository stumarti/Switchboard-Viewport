// Host stand-in for Arduino's String: the API the firmware and the kitchen
// panel's own code use, over std::string.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

class String {
 public:
  String(const char* s = "") : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  String(char c) : s_(1, c) {}
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}
  String(long long v) : s_(std::to_string(v)) {}
  String(unsigned long long v) : s_(std::to_string(v)) {}
  String(float v, unsigned decimals = 2) : s_(fmt(v, decimals)) {}
  String(double v, unsigned decimals = 2) : s_(fmt(v, decimals)) {}

  const char* c_str() const { return s_.c_str(); }
  const char* begin() const { return s_.c_str(); }
  const char* end() const { return s_.c_str() + s_.size(); }
  unsigned length() const { return static_cast<unsigned>(s_.size()); }
  bool isEmpty() const { return s_.empty(); }
  char operator[](unsigned i) const { return i < s_.size() ? s_[i] : 0; }
  char& operator[](unsigned i) { return s_[i]; }
  char charAt(unsigned i) const { return (*this)[i]; }

  String substring(unsigned from) const { return from >= s_.size() ? String() : String(s_.substr(from)); }
  String substring(unsigned from, unsigned to) const {
    if (from > to) std::swap(from, to);
    if (from >= s_.size()) return String();
    return String(s_.substr(from, to - from));
  }
  int indexOf(char c, unsigned from = 0) const { return pos(s_.find(c, from)); }
  int indexOf(const char* t, unsigned from = 0) const { return pos(s_.find(t, from)); }
  int indexOf(const String& t, unsigned from = 0) const { return pos(s_.find(t.s_, from)); }
  int lastIndexOf(char c) const { return pos(s_.rfind(c)); }
  int lastIndexOf(const String& t) const { return pos(s_.rfind(t.s_)); }
  bool startsWith(const String& p) const { return s_.compare(0, p.s_.size(), p.s_) == 0 && s_.size() >= p.s_.size(); }
  bool endsWith(const String& p) const { return s_.size() >= p.s_.size() && s_.compare(s_.size() - p.s_.size(), p.s_.size(), p.s_) == 0; }
  void trim() {
    size_t a = 0, b = s_.size();
    while (a < b && isspace(static_cast<unsigned char>(s_[a]))) ++a;
    while (b > a && isspace(static_cast<unsigned char>(s_[b - 1]))) --b;
    s_ = s_.substr(a, b - a);
  }
  void replace(const String& from, const String& to) {
    if (from.s_.empty()) return;
    size_t p = 0;
    while ((p = s_.find(from.s_, p)) != std::string::npos) {
      s_.replace(p, from.s_.size(), to.s_);
      p += to.s_.size();
    }
  }
  void remove(unsigned index, unsigned count = 1) { if (index < s_.size()) s_.erase(index, count); }
  void toLowerCase() { for (char& c : s_) c = static_cast<char>(tolower(static_cast<unsigned char>(c))); }
  void toUpperCase() { for (char& c : s_) c = static_cast<char>(toupper(static_cast<unsigned char>(c))); }
  float toFloat() const { return static_cast<float>(atof(s_.c_str())); }
  long toInt() const { return atol(s_.c_str()); }
  bool reserve(unsigned n) { s_.reserve(n); return true; }
  bool concat(const String& o) { s_ += o.s_; return true; }
  bool concat(const char* o) { s_ += o ? o : ""; return true; }
  bool concat(const char* o, unsigned n) { s_.append(o, n); return true; }
  bool concat(char c) { s_ += c; return true; }

  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  String& operator+=(const char* o) { s_ += o ? o : ""; return *this; }
  String& operator+=(char c) { s_ += c; return *this; }
  String& operator+=(int v) { s_ += std::to_string(v); return *this; }
  friend String operator+(const String& a, const String& b) { return String(a.s_ + b.s_); }
  friend String operator+(const String& a, const char* b) { return String(a.s_ + (b ? b : "")); }
  friend String operator+(const char* a, const String& b) { return String(std::string(a ? a : "") + b.s_); }
  friend String operator+(const String& a, char b) { return String(a.s_ + b); }
  friend String operator+(const String& a, int b) { return String(a.s_ + std::to_string(b)); }
  bool operator==(const String& o) const { return s_ == o.s_; }
  bool operator==(const char* o) const { return s_ == (o ? o : ""); }
  bool operator!=(const String& o) const { return s_ != o.s_; }
  bool operator!=(const char* o) const { return s_ != (o ? o : ""); }
  bool operator<(const String& o) const { return s_ < o.s_; }
  bool operator>(const String& o) const { return s_ > o.s_; }
  const std::string& std() const { return s_; }

  // For ArduinoJson's String support.
  size_t write(uint8_t c) { s_ += static_cast<char>(c); return 1; }

 private:
  static int pos(size_t p) { return p == std::string::npos ? -1 : static_cast<int>(p); }
  static std::string fmt(double v, unsigned d) {
    char b[64];
    snprintf(b, sizeof(b), "%.*f", static_cast<int>(d), v);
    return b;
  }
  std::string s_;
};
