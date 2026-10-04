// Renders the system screens (test/host/run.sh): test/host/out/system-*.ppm.
#include <stdio.h>
#include <string>
#include "canvas.h"
#include "render/draw.h"
#include "render/system.h"
#include <string.h>

int main(int argc, char** argv) {
  const std::string out = argc > 1 ? argv[1] : ".";
  auto save = [&](Canvas& c, const char* name) { c.writePpm((out + "/system-" + name + ".ppm").c_str()); };
  {
    Canvas c;
    sys::splash(c, "starting up");
    save(c, "splash");
  }
  {
    Canvas c;
    sys::setup(c, {"Switchboard-3F2A", "k7m4q9tw", "http://192.168.4.1", "No saved Wi-Fi"});
    save(c, "setup");
  }
  {
    Canvas c;
    sys::pairing(c, {"A0:B1:C2:00:01:01", "http://192.168.1.20:45678", "http://192.168.1.20:45678/#/viewports/A0%3AB1%3AC2%3A00%3A01%3A01",
                     "Waiting for approval"});
    save(c, "pairing");
  }
  {
    Canvas c;
    sys::notSetUp(c, "http://192.168.1.20:45678/#/viewports/A0%3AB1");
    save(c, "not-set-up");
  }
  {
    Canvas c;
    sys::error(c, "vx_wifi_off", "No WiFi connection", "Check network or router", "Sat 03 Oct 2026  11:04");
    save(c, "error-wifi");
  }
  {
    Canvas c;
    sys::error(c, "vx_cloud_off", "Can't reach Home Assistant", "Switchboard Server can't reach it", "Sat 03 Oct 2026  11:04");
    save(c, "error-ha");
  }
  {
    Canvas c;
    sys::error(c, "vx_server_off", "Not connected to Switchboard", "Can't reach the server at 192.168.1.20:45678", "Sat 03 Oct 2026  11:04");
    save(c, "error-server");
  }
  {
    Canvas c;
    sys::charge(c, 2);
    save(c, "charge");
  }
  {
    Canvas c;
    sys::info(c, {30, "11:04", 76, "192.168.1.20:45678", "Kitchen panel", "Kitchen panel", "v0.1.0", "MAC A0:B1:C2:00:01:01", "Home -58 dBm"});
    save(c, "info");
  }
  {
    Canvas c;
    sys::Info in{30, "11:04", 76, "192.168.1.20:45678", "Kitchen panel", "Kitchen panel", "v0.1.0", "MAC A0:B1:C2:00:01:01", "Home -58 dBm"};
    in.home = "Status";
    sys::info(c, in);
    save(c, "info-home");
  }
  {
    Canvas c;
    sys::updating(c, "v0.2.0", "Downloading: 40%");
    save(c, "updating");
  }
  // Text from the server is UTF-8; the fonts are ASCII.
  int failed = 0;
  auto expect = [&](const char* in, const char* want) {
    char buf[64];
    draw::ascii(buf, sizeof(buf), in);
    if (strcmp(buf, want)) {
      printf("FAIL ascii(\"%s\") = \"%s\", want \"%s\"\n", in, buf, want);
      ++failed;
    }
  };
  expect("10:30\xE2\x80\x93" "11:30", "10:30-11:30");                // en dash
  expect("Sam\xE2\x80\x99s review", "Sam's review");                // Outlook's apostrophe
  expect("\xE2\x80\x9C" "All hands\xE2\x80\x9D", "\"All hands\"");
  expect("Caf\xC3\xA9 \xC3\x85lesund \xC5\x81\xC3\xB3" "d\xC5\xBA", "Cafe Alesund Lodz");
  expect("Stra\xC3\x9F" "e", "Strasse");
  expect("Wait\xE2\x80\xA6", "Wait...");
  expect("42 \xC2\xB7 City", "42  City");                              // left out, as before
  expect("21\xC2\xB0" "C", "21C");
  expect("broken \xE2\x80", "broken ");                              // a cut-off character
  if (failed) return 1;
  printf("system screens rendered, text checks passed\n");
  return 0;
}
