// =============================================================================
// server.h — talking to Switchboard Server.
//
// Finding it: an address typed in at Wi-Fi setup, else the last one that
// worked, else mDNS (its _switchboard._tcp service, then switchboard.local).
// Every request carries the device's health (X-Battery, X-Temperature,
// X-Humidity, X-RSSI) and what it runs (X-Firmware, X-Board), and, once
// paired, its token.
// =============================================================================
#pragma once
#include <Arduino.h>
#include <vector>

namespace server {

struct Health {
  int battery = -1;
  float temperature = NAN;
  float humidity = NAN;
  float voltage = NAN;  // the battery's, for Home Assistant (as the panel sent it)
};
void setHealth(const Health& h);

bool resolve();
String base();  // "http://192.168.1.20:45678", once resolved
String address();  // "192.168.1.20:45678"

String token();
void setToken(const String& t);
void forgetToken();

// Registering with the server (POST /api/pairing/register, as a viewport):
// approved hands back a token (kept); pending or revoked waits for the admin.
enum class Pairing : uint8_t { Approved, Pending, Revoked, Unreachable };
Pairing pair(String& status);

struct Response {
  int code = 0;          // HTTP status, or < 0 if it couldn't be reached
  String etag;
  String body;           // text responses
  std::vector<uint8_t> bytes;  // binary ones (asBytes)
  uint32_t refreshIn = 0;      // X-Refresh-In
  bool quiet = false;          // X-Quiet
};
// GET with the token and health headers; `ifNoneMatch` makes a 304
// possible. A failed connection re-finds the server once.
Response get(const String& path, const char* ifNoneMatch = nullptr, bool asBytes = false, uint32_t timeoutMs = 10000);
Response post(const String& path, const String& json);

String urlEncode(const String& s);

}  // namespace server
