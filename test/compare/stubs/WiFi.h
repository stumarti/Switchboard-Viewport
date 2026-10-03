#pragma once
#include "kd_env.h"
#define WL_CONNECTED 3
#define WIFI_STA 1
#define WIFI_OFF 0
struct HostWiFi {
  void mode(int) {}
  void begin(const char*, const char*) {}
  int status() { return WL_CONNECTED; }
  const char* localIP() { return "10.0.0.2"; }
  void disconnect(bool = false) {}
};
extern HostWiFi WiFi;
class WiFiClient {};
