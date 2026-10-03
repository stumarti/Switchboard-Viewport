# Switchboard Viewport

Firmware for the **Seeed reTerminal E1002** (7.3" Spectra 6 colour e-paper, ESP32-S3) that makes it a [Switchboard](https://github.com/stumarti/Switchboard) wall display. It pairs with [Switchboard Server](https://github.com/stumarti/Switchboard-Server) and pulls everything else from there: the layout, the theme, and every screen's values. It draws them, then sleeps until the server says to wake.

<p>
  <img src="https://raw.githubusercontent.com/stumarti/Switchboard/main/docs/manual/images/viewport/device/kitchen-panel-status.png" width="400" alt="Status">
  <img src="https://raw.githubusercontent.com/stumarti/Switchboard/main/docs/manual/images/viewport/device/kitchen-panel-heating.png" width="400" alt="Heating">
</p>

- **Setup without a keyboard:** the panel shows two QR codes. One joins the display's own hotspot, the other opens its page, where you pick your Wi-Fi.
- **Switchboard Server**, found by mDNS. The display pairs as a viewport, then fetches its layout, Wi-Fi list, clock and theme packs. Each screen comes with an ETag, and the panel is left alone when nothing changed.
- **The kitchen dashboard**, drawn pixel for pixel as the panel's original firmware drew it (`test/compare` checks this). Every other viewport section and screen works too: energy graph, presence, meeting room, room finder, announcements and more.
- **Buttons and carousel:** left previous, middle next, the green one (right) home, on every screen. Hold left for device info, middle to clear the panel, green for Wi-Fi setup.
- **Refresh and sleep** follow what the server says, including quiet hours. The footer shows the carousel marks, the refresh time and the battery.
- **Health:** every request carries the battery, temperature, humidity, signal, firmware and board.
- **Updates over Wi-Fi** from the server, checked by SHA-256 and board. A new version rolls back by itself if it can't reach the server.
- **Error screens** for no Wi-Fi, no Switchboard Server, no Home Assistant, and a flat battery.

Install it from the [browser flasher](https://stumarti.github.io/Switchboard/) (Chrome or Edge, USB-C). Read the manual's [viewport section](https://stumarti.github.io/Switchboard/manual/viewport/setup.html) to set one up.

## Build

```
pio run -e e1002 -t upload -t monitor
```

## Test

```
./test/host/run.sh        # logic tests + every screen rendered to test/host/out/
./test/compare/compare.sh # the original kitchen panel firmware vs this one, pixel for pixel
```

## Release

```
git tag v0.1.0 && git push origin v0.1.0
```

The release workflow publishes the whole flash image (`switchboard-viewport-<tag>.bin`, for USB) and the over-the-air image (`switchboard-e1002-app-<tag>.bin` and its `.sha256`). The server lists this repository's releases under **Settings → Remote updates**.
