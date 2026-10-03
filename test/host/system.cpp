// Renders the system screens (test/host/run.sh): test/host/out/system-*.ppm.
#include <stdio.h>
#include <string>
#include "canvas.h"
#include "render/system.h"

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
  printf("system screens rendered\n");
  return 0;
}
