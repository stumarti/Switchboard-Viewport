// =============================================================================
// wifi.h — joining Wi-Fi, and setting it up without a keyboard.
//
// Setup: the display opens its own hotspot (Switchboard-XXXX, with a
// password shown on the panel), and shows two QR codes — one joins the
// hotspot, the other opens its page (a phone usually opens it by itself: the
// display answers every name, as a captive portal). The page lists the
// networks it can see; pick one, type its password (and, if mDNS can't find
// the server on your network, its address), save. The display restarts and
// joins it.
// =============================================================================
#pragma once
#include <Arduino.h>

namespace wifi {

// Tries the saved networks (the device's own, then the server's list),
// strongest first among those in range. True once connected.
bool connect();
String ssid();
int rssi();
String mac();          // "a0:b1:c2:00:01:01", as the server keys it
String shortId();      // "3F2A", the MAC's last four hex digits
void off();

// Runs setup until a network is saved (then restarts), `SETUP_TIMEOUT_MS`
// passes, or the green button is pressed; draws its own screen. Returns
// only on time-out or cancel.
void setup(const char* reason);

}  // namespace wifi
