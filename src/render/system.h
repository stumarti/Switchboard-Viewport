// =============================================================================
// system.h — the screens that aren't a layout's: starting up, Wi-Fi setup,
// pairing, "not set up", errors, "charge me", the button card, updating.
// Switchboard's look, in the kitchen panel's style: its fonts, its error and
// charge screens as they were.
// =============================================================================
#pragma once
#include "render/draw.h"

namespace sys {

// First power-on: the Switchboard logo, name and slogan.
void splash(draw::Gfx& g, const char* line);

// Wi-Fi setup, for a panel with no keyboard: two QR codes — join the
// device's hotspot, then open its page — with the same in words.
struct Setup {
  const char* apName;      // "Switchboard-AB12"
  const char* apPassword;  // shown, and in the QR code
  const char* url;         // "http://192.168.4.1"
  const char* reason;      // why it's here ("No saved Wi-Fi", "Couldn't join Home")
};
void setup(draw::Gfx& g, const Setup& s);

// Waiting to be approved on Switchboard Server; the QR code opens this
// display's page there.
struct Pairing {
  const char* name;       // what the server lists it as (its MAC)
  const char* serverUrl;  // "http://192.168.1.20:45678"
  const char* pageUrl;    // "<serverUrl>/#/viewports/<mac>"
  const char* status;     // "Waiting for approval"
};
void pairing(draw::Gfx& g, const Pairing& p);

// Approved, but no layout assigned yet.
void notSetUp(draw::Gfx& g, const char* pageUrl);

// The kitchen panel's error screen: a red icon, a big title, a line under
// it, and the date and time (an error can last for days).
void error(draw::Gfx& g, const char* iconKey, const char* title, const char* detail, const char* dateTime);

// The panel's "CHARGE ME" (battery at 2% or less: no Wi-Fi, no refresh).
void charge(draw::Gfx& g, int pct);

// The button card and device details (left button, held).
struct Info {
  int refreshMin;
  const char* time;
  int battPct;
  const char* server;    // address, or "not found"
  const char* name;      // its name on the server
  const char* layout;    // its layout's name
  const char* firmware;  // FIRMWARE_VERSION
  const char* mac;
  const char* wifi;      // network and signal
  // Buttons that go straight to a screen: the screens' titles.
  bool direct = false;
  const char* left = "";
  const char* middle = "";
  const char* right = "";
};
void info(draw::Gfx& g, const Info& i);

// Installing an update from Switchboard Server.
void updating(draw::Gfx& g, const char* version, const char* status);

// A QR code for `text`, `scale` px a module, with its white margin, top-left
// at (x, y); returns its side in px (0 if it couldn't be made).
int qr(draw::Gfx& g, const char* text, int x, int y, int scale);
int qrSide(const char* text, int scale);

}  // namespace sys
