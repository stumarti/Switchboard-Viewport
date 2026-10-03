#include "render/system.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "render/icons.h"
#include "qrcodegen.h"

using draw::Gfx;
using draw::Mode;

namespace sys {

namespace {

// The Switchboard header every setup screen opens with: the logo beside the
// name, and a rule under them.
void brand(Gfx& g, int y) {
  const draw::Icon logo = icons::slot("vx_logo");
  const int s = 48;
  // The logo at half size: every other pixel of the 120 px mask.
  if (logo && !logo.fourBit) {
    const int rowBytes = (logo.w + 7) / 8;
    for (int yy = 0; yy < s; ++yy)
      for (int xx = 0; xx < s; ++xx) {
        const int sx = xx * logo.w / s, sy = yy * logo.h / s;
        if (!(pgm_read_byte(&logo.bits[sy * rowBytes + sx / 8]) & (0x80 >> (sx & 7)))) g.drawPixel(40 + xx, y + yy, GxEPD_BLACK);
      }
  }
  draw::face(g, draw::BOLD24, GxEPD_BLACK);
  draw::text(g, 100, y + 32, SWITCHBOARD_NAME);
  draw::face(g, draw::REG18, GxEPD_BLACK);
  draw::textRight(g, DEVICE_KIND, draw::PANEL_W - 40, y + 32);
  g.fillRect(40, y + 58, draw::PANEL_W - 80, 2, GxEPD_BLACK);
}

bool encode(const char* text, uint8_t* qrcode) {
  uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
  return qrcodegen_encodeText(text, temp, qrcode, qrcodegen_Ecc_MEDIUM, 1, 10, qrcodegen_Mask_AUTO, true);
}

}  // namespace

int qrSide(const char* text, int scale) {
  uint8_t code[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
  if (!encode(text, code)) return 0;
  return (qrcodegen_getSize(code) + 8) * scale;
}

int qr(Gfx& g, const char* text, int x, int y, int scale) {
  uint8_t code[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
  if (!encode(text, code)) return 0;
  const int n = qrcodegen_getSize(code);
  const int side = (n + 8) * scale;  // a 4-module quiet zone all round
  g.fillRect(x, y, side, side, GxEPD_WHITE);
  for (int r = 0; r < n; ++r)
    for (int c = 0; c < n; ++c)
      if (qrcodegen_getModule(code, c, r)) g.fillRect(x + (c + 4) * scale, y + (r + 4) * scale, scale, scale, GxEPD_BLACK);
  return side;
}

void splash(Gfx& g, const char* line) {
  g.fillScreen(GxEPD_WHITE);
  draw::icon(g, icons::slot("vx_logo"), (draw::PANEL_W - 120) / 2, 70, GxEPD_BLACK, Mode::Mask);
  draw::face(g, draw::BOLD24, GxEPD_BLACK, 2);
  draw::textCentered(g, SWITCHBOARD_NAME, 0, draw::PANEL_W, 268);
  g.fillRect(draw::PANEL_W / 2 - 160, 292, 320, 2, GxEPD_BLACK);
  draw::face(g, draw::BOLD24, GxEPD_BLACK);
  draw::textCentered(g, SWITCHBOARD_SLOGAN, 0, draw::PANEL_W, 336);
  draw::face(g, draw::REG18, GxEPD_BLACK);
  draw::textCentered(g, line && *line ? line : "starting up", 0, draw::PANEL_W, 420);
}

void setup(Gfx& g, const Setup& s) {
  g.fillScreen(GxEPD_WHITE);
  brand(g, 20);
  draw::face(g, draw::BOLD24, GxEPD_BLACK);
  draw::text(g, 40, 118, "Connect this display to Wi-Fi");
  if (s.reason && *s.reason) {
    draw::face(g, draw::REG18, GxEPD_RED);
    draw::textRight(g, s.reason, draw::PANEL_W - 40, 118);
  }
  // Two steps side by side, each a QR code over its words: join the
  // device's hotspot (most phones join from the code), then open its page.
  char wifi[160];
  snprintf(wifi, sizeof(wifi), "WIFI:T:WPA;S:%s;P:%s;;", s.apName, s.apPassword);
  char pw[64];
  snprintf(pw, sizeof(pw), "Password: %s", s.apPassword);
  struct Step {
    const char* code;
    const char* title;
    const char* line1;
    const char* line2;
  } steps[] = {{wifi, "1. Join its Wi-Fi", s.apName, pw}, {s.url, "2. Open its page", s.url, "and pick your network"}};
  const int half = draw::PANEL_W / 2, scale = 5, top = 132;
  for (int i = 0; i < 2; ++i) {
    const int side = qrSide(steps[i].code, scale);
    qr(g, steps[i].code, i * half + (half - side) / 2, top, scale);
    const int ty = top + 190 + 22;
    draw::face(g, draw::BOLD24, GxEPD_BLACK);
    draw::textCentered(g, steps[i].title, i * half, half, ty);
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::textCentered(g, steps[i].line1, i * half, half, ty + 26);
    draw::textCentered(g, steps[i].line2, i * half, half, ty + 48);
  }
  g.drawFastVLine(half, top + 10, 250, GxEPD_BLACK);
  draw::face(g, draw::REG18, GxEPD_BLACK);
  draw::textCentered(g, "Setup stops after 10 minutes; hold the middle button to start it again.", 0, draw::PANEL_W, 462);
}

void pairing(Gfx& g, const Pairing& p) {
  g.fillScreen(GxEPD_WHITE);
  brand(g, 20);
  draw::icon(g, icons::slot("vx_link"), 40, 110, GxEPD_BLACK, Mode::Mask);
  draw::face(g, draw::BOLD24, GxEPD_BLACK);
  draw::text(g, 120, 140, "Approve this display");
  draw::face(g, draw::REG18, GxEPD_BLACK);
  draw::text(g, 120, 166, p.status && *p.status ? p.status : "Waiting for approval");

  const int scale = 5;
  const int side = qrSide(p.pageUrl, scale);
  const int qx = draw::PANEL_W - 40 - side;
  qr(g, p.pageUrl, qx, 110, scale);

  draw::face(g, draw::REG18, GxEPD_BLACK);
  const int w = qx - 60;
  int y = 214;
  y += 22 * draw::textWrapped(g, "On Switchboard Server, open Viewports and approve this display, then choose its layout.", 40, y, w, 22, 3);
  y += 14;
  char buf[96];
  snprintf(buf, sizeof(buf), "It's listed as %s", p.name);
  draw::text(g, 40, y, buf);
  y += 24;
  snprintf(buf, sizeof(buf), "Server: %s", p.serverUrl);
  draw::text(g, 40, y, buf);
  y += 36;
  draw::text(g, 40, y, "It checks again every 2 minutes, or press any button.");
}

void notSetUp(Gfx& g, const char* pageUrl) {
  g.fillScreen(GxEPD_WHITE);
  brand(g, 20);
  draw::icon(g, icons::slot("vx_not_set_up"), 40, 120, GxEPD_BLACK, Mode::Mask);
  draw::face(g, draw::BOLD24, GxEPD_BLACK);
  draw::text(g, 156, 156, "Not set up yet");
  draw::face(g, draw::REG18, GxEPD_BLACK);
  const int scale = 5;
  const int side = pageUrl && *pageUrl ? qrSide(pageUrl, scale) : 0;
  const int w = draw::PANEL_W - 156 - (side ? side + 60 : 40);
  draw::textWrapped(g, "On Switchboard Server, open this display's page and choose the layout it shows. It picks it up at its next refresh, or press any button.", 156, 186, w, 22, 5);
  if (side) qr(g, pageUrl, draw::PANEL_W - 40 - side, 110, scale);
}

// The kitchen panel's error screen, as it was.
void error(Gfx& g, const char* iconKey, const char* title, const char* detail, const char* dateTime) {
  g.fillScreen(GxEPD_WHITE);
  const int ICON_SZ = 96;
  draw::icon(g, icons::slot(iconKey), (draw::PANEL_W - ICON_SZ) / 2, 110, GxEPD_RED, Mode::Mask);
  draw::face(g, draw::BOLD24, GxEPD_BLACK, 2);
  draw::textCentered(g, title, 0, draw::PANEL_W, 290);
  if (detail && *detail) {
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::textCentered(g, detail, 0, draw::PANEL_W, 340);
  }
  draw::face(g, draw::REG18, GxEPD_BLACK);
  draw::textCentered(g, dateTime && *dateTime ? dateTime : "--:--", 0, draw::PANEL_W, 420);
}

// The kitchen panel's "CHARGE ME", as it was.
void charge(Gfx& g, int pct) {
  g.fillScreen(GxEPD_WHITE);
  draw::icon(g, icons::slot("vx_plug"), (draw::PANEL_W - 96) / 2, 150, GxEPD_RED, Mode::Opaque);
  draw::face(g, draw::BOLD24, GxEPD_RED, 2);
  draw::textCentered(g, "CHARGE ME", 0, draw::PANEL_W, 320);
  draw::face(g, draw::BOLD24, GxEPD_BLACK);
  char buf[24];
  snprintf(buf, sizeof(buf), "Battery %d%%", pct);
  draw::textCentered(g, buf, 0, draw::PANEL_W, 370);
}

// The kitchen panel's button card, for the carousel's buttons, with the
// device's details under it.
void info(Gfx& g, const Info& in) {
  g.fillScreen(GxEPD_WHITE);
  draw::face(g, draw::BOLD24, GxEPD_BLACK);
  draw::textCentered(g, "Switchboard Viewport", 0, draw::PANEL_W, 36);
  g.drawFastHLine(40, 48, draw::PANEL_W - 80, GxEPD_BLACK);

  const int COL_W = draw::PANEL_W / 3, COL_Y = 70, ROW_H = 90, BAR_W = 4;
  struct Entry {
    const char* key;
    const char* shortTitle;
    const char* shortDesc;
    uint16_t shortCol;
    const char* longTitle;
    const char* longDesc;
    uint16_t longCol;
  };
  const Entry entries[] = {
      {"LEFT", "Previous screen", "Step back through\nthe screens", GxEPD_BLACK, "This screen", "Buttons and\ndevice details", GxEPD_BLUE},
      {"MIDDLE", "Refresh", "Fetch the latest and\nredraw this screen", GxEPD_GREEN, "Wi-Fi setup", "Join another network\nfrom your phone", GxEPD_BLUE},
      {"RIGHT", "Next screen", "Step on through\nthe screens", GxEPD_BLACK, "Clear screen", "Fills the panel white\nNext wake redraws", GxEPD_BLACK},
  };
  auto lines = [&](const char* s, int x, int y) {
    const char* nl = strchr(s, '\n');
    if (!nl) {
      draw::text(g, x, y, s);
      return;
    }
    char a[64];
    snprintf(a, sizeof(a), "%.*s", static_cast<int>(nl - s), s);
    draw::text(g, x, y, a);
    draw::text(g, x, y + 14, nl + 1);
  };
  for (int c = 0; c < 3; ++c) {
    const int cx = c * COL_W;
    const Entry& e = entries[c];
    draw::face(g, draw::BOLD24, GxEPD_BLACK);
    draw::textCentered(g, e.key, cx, COL_W, COL_Y);
    if (c < 2) g.drawFastVLine(cx + COL_W, COL_Y - 10, 16 + ROW_H * 2 + 30, GxEPD_BLACK);
    int ry = COL_Y + 16;
    g.fillRect(cx + 8, ry, BAR_W, ROW_H - 4, e.shortCol);
    draw::face(g, draw::BOLD18, e.shortCol);
    draw::text(g, cx + 18, ry + 14, "Short press");
    draw::face(g, draw::BOLD24, GxEPD_BLACK);
    draw::text(g, cx + 18, ry + 34, e.shortTitle);
    draw::face(g, draw::REG18, GxEPD_BLACK);
    lines(e.shortDesc, cx + 18, ry + 52);
    draw::dottedH(g, cx + 8, cx + COL_W - 8, COL_Y + 16 + ROW_H);
    ry = COL_Y + 16 + ROW_H + 10;
    g.fillRect(cx + 8, ry, BAR_W, ROW_H - 4, e.longCol);
    draw::face(g, draw::BOLD18, e.longCol);
    draw::text(g, cx + 18, ry + 14, "Long press (1s)");
    draw::face(g, draw::BOLD24, GxEPD_BLACK);
    draw::text(g, cx + 18, ry + 34, e.longTitle);
    draw::face(g, draw::REG18, GxEPD_BLACK);
    lines(e.longDesc, cx + 18, ry + 52);
  }

  const int divY = COL_Y + 16 + ROW_H * 2 + 20;
  g.drawFastHLine(40, divY, draw::PANEL_W - 80, GxEPD_BLACK);
  char buf[96];
  draw::face(g, draw::REG18, GxEPD_BLACK);
  int iy = divY + 20;
  snprintf(buf, sizeof(buf), "Refreshes every %d min", in.refreshMin);
  draw::text(g, 40, iy, buf);
  draw::textCentered(g, in.time, 0, draw::PANEL_W, iy);
  snprintf(buf, sizeof(buf), "Battery %d%%", in.battPct);
  draw::textRight(g, buf, draw::PANEL_W - 40, iy);
  iy += 26;
  snprintf(buf, sizeof(buf), "%s  \xC2\xB7  %s", in.name, in.layout && *in.layout ? in.layout : "no layout");
  draw::text(g, 40, iy, buf);
  snprintf(buf, sizeof(buf), "Firmware %s", in.firmware);
  draw::textRight(g, buf, draw::PANEL_W - 40, iy);
  iy += 26;
  snprintf(buf, sizeof(buf), "Server %s", in.server);
  draw::text(g, 40, iy, buf);
  draw::textRight(g, in.wifi, draw::PANEL_W - 40, iy);
  iy += 26;
  draw::text(g, 40, iy, in.mac);
}

void updating(Gfx& g, const char* version, const char* status) {
  g.fillScreen(GxEPD_WHITE);
  brand(g, 20);
  draw::icon(g, icons::slot("vx_update"), (draw::PANEL_W - 64) / 2, 150, GxEPD_BLACK, Mode::Mask);
  draw::face(g, draw::BOLD24, GxEPD_BLACK);
  char buf[96];
  snprintf(buf, sizeof(buf), "Updating to %s", version);
  draw::textCentered(g, buf, 0, draw::PANEL_W, 270);
  draw::face(g, draw::REG18, GxEPD_BLACK);
  draw::textCentered(g, status && *status ? status : "Downloading from Switchboard Server", 0, draw::PANEL_W, 304);
  draw::textCentered(g, "It restarts by itself when it's done.", 0, draw::PANEL_W, 340);
}

}  // namespace sys
