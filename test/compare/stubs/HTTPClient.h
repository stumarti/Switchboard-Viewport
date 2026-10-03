// The panel's HTTPClient, answered from the captured Home Assistant
// (house.json): states, the weather service, calendars (by date range).
#pragma once
#include "kd_env.h"
#include "WiFi.h"
class HTTPClient {
 public:
  bool begin(const String& url) { url_ = url; return true; }
  void addHeader(const String&, const String&) {}
  int GET();
  int POST(const String& body);
  String getString() { return body_; }
  void end() {}
 private:
  String url_, body_;
};
