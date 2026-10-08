// =============================================================================
// draw.h — what every screen draws with: the panel's six colours, the four
// font faces, text placement and icons.
//
// Everything draws onto an Adafruit_GFX: the GxEPD2 panel on the device, a
// plain canvas in the host tests (test/host), so a screen renders the same in
// both. Coordinates are the panel's: 800x480, y down; text y is the baseline
// (GFX fonts), exactly as the kitchen panel's own drawing code placed it.
// =============================================================================
#pragma once
#include <stdint.h>
#include <Adafruit_GFX.h>
#include "render/colors.h"

namespace draw {

using Gfx = Adafruit_GFX;

// The screen being drawn: 800x480 landscape, or 480x800 when its layout
// hangs portrait (screens::drawScreen sets it, and puts it back to
// landscape after; the system screens are always landscape).
extern int PANEL_W;
extern int PANEL_H;
void setPortrait(bool portrait);
// The screen's shape, landscape: 800x480 on the E1002. A board with another
// panel sets its own before drawing anything (the E1004's 4:3 panel draws
// 800x600, doubled to its 1600x1200; portrait, 600x800).
void setScreenShape(int longSide, int shortSide);

// The panel's palette, by the index Switchboard Server uses: 0 white,
// 1 black, 2 red, 3 yellow, 4 green, 5 blue.
uint16_t color(int idx);

// The four faces. Each is the theme's font from Switchboard Server when one
// is loaded (theme.h), else the panel's built-in Atkinson Hyperlegible:
//   REG18  9pt regular    BOLD18  9pt bold    BOLD24  12pt bold
//   BOLD82 42pt bold (the big temperature, ON/OFF)
enum Face : uint8_t { REG18 = 0, BOLD18, BOLD24, BOLD82, FACE_COUNT };
const GFXfont* font(Face f);
// The panel's own face (whatever the theme sets): a board with a finer
// panel draws text in the same face made at its resolution.
const GFXfont* builtInFont(Face f);
// Overrides (the theme's faces); nullptr restores the built-in one.
void setFont(Face f, const GFXfont* override_);

void face(Gfx& g, Face f, uint16_t col, uint8_t size = 1);

// Text extents as the kitchen panel measured them (getTextBounds).
struct Bounds { int16_t x, y; uint16_t w, h; };
// `s` (UTF-8) as the panel's ASCII fonts can draw it: "–" as "-", "’" as
// "'", "é" as "e", anything else left out. Every text function here does it.
const char* ascii(char* out, size_t cap, const char* s);

Bounds bounds(Gfx& g, const char* s, int x = 0, int y = 0);

void text(Gfx& g, int x, int y, const char* s);
// Right-aligned so it ends at `right` (the panel's printRight()).
void textRight(Gfx& g, const char* s, int right, int y);
// Centred in [x, x + w).
void textCentered(Gfx& g, const char* s, int x, int w, int y);
// Words wrapped to `w`, at most `maxLines` lines of `lineH`; returns the
// number of lines used.
int textWrapped(Gfx& g, const char* s, int x, int y, int w, int lineH, int maxLines);
// `src` when it's at most `limit` characters, else its first `keep` and
// "..." — the panel's `if (s.length() > limit) s = s.substring(0, keep) + "..."`.
void cut(char* out, size_t cap, const char* src, int limit, int keep);
// A number as the panel's String(v, decimals) wrote it (Arduino-ESP32's
// dtostrf(v, decimals + 2, decimals)): half away from zero, and padded on
// the left to that width, so String(5.0f, 0) is " 5".
const char* fixed(char* out, size_t cap, float v, int decimals);
// `s` cut, with "...", to fit `maxW` px in the current face.
const char* fit(Gfx& g, char* out, size_t cap, const char* s, int maxW);

// --- Icons ---------------------------------------------------------------------
//
// Two formats: the kitchen panel's own 4-bit bitmaps (each pixel a palette
// index, 1 = "the tint"), compiled in as the default look; and 1-bit masks
// (Switchboard's icon format: a theme pack's slots, and icons a layout picks
// that are fetched from the server), drawn in the tint.
struct Icon {
  uint16_t w = 0, h = 0;
  const uint8_t* bits = nullptr;
  bool fourBit = false;
  // Its bits are at the panel's finer resolution (draw::fine.scale times
  // the size it takes on the screen): fetched that size from the server.
  bool fine = false;
  explicit operator bool() const { return bits != nullptr; }
};

// How an icon covers what's under it, as each of the panel's draw helpers
// did:
//   Opaque  every pixel, white included (status bar, detail row, weather...)
//   Ink     only inked pixels, other colours kept (heating page on its flood)
//   Mask    only the tint (security and error screens)
enum class Mode : uint8_t { Opaque, Ink, Mask };

void icon(Gfx& g, const Icon& ic, int x, int y, uint16_t tint, Mode mode = Mode::Opaque);
// A 4-bit picture (album art, a photo: the server's "spectra" format), `w`
// by `h` on the screen at x, y.
void picture(Gfx& g, const uint8_t* bits, int w, int h, int x, int y);

// Pictures finer than the screen. The E1004 draws the viewport's screens at
// twice the size, but a photo blown up like that would look coarse: its
// pictures are fetched `scale` times the size they take on the screen, and
// `draw` puts them on the panel at its full resolution (bits: the picture,
// w x h; x, y: where it goes, in the screen's coordinates). The E1002:
// scale 1, no draw.
//
// Icons too: those fetched from the server come `scale` times the size
// (Icon::fine); `icon` draws any icon at the panel's resolution (a built-in
// one, at the screen's, it enlarges itself).
struct FinePictures {
  int scale = 1;
  void (*draw)(const uint8_t* bits, int w, int h, int x, int y) = nullptr;
  void (*icon)(const Icon& ic, int x, int y, uint16_t tint, Mode mode) = nullptr;
};
extern FinePictures fine;
// An icon's pixels on `g` exactly as they are, w x h at x, y (what icon()
// does without draw::fine): for a board drawing one at its own resolution.
void iconPixels(Gfx& g, const Icon& ic, int x, int y, uint16_t tint, Mode mode);

// The panel's dotted rules: 2 px of ink every 7 px.
void dottedV(Gfx& g, int x, int y0, int y1);
void dottedH(Gfx& g, int x0, int x1, int y);

}  // namespace draw
