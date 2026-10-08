// =============================================================================
// icons.h — where every icon comes from, the Switchboard way:
//
//   slot(key)          a fixed icon the firmware draws (weather, footer,
//                      heating, security, system screens), by its theme slot
//                      (Switchboard Server's lib/assets/viewport-slots.js):
//                      the theme pack's when one is loaded, else built in
//   named(name, size)  an icon a layout picks (status icons, "now" items,
//                      rooms...): the kitchen panel's own art for that name
//                      at that size when it had one, else the icon fetched
//                      from the server (/api/icons/mdi/<name>?size=) before
//                      drawing began
//
// The built-in art is the kitchen panel's own 4-bit bitmaps, so an untouched
// theme looks exactly as the panel did.
// =============================================================================
#pragma once
#include "render/draw.h"

namespace icons {

draw::Icon slot(const char* key);
// Whether the theme pack replaces `key` (some icons draw differently then:
// the built-in solar panel has its own yellow, a themed one is tinted).
bool themed(const char* key);
draw::Icon named(const char* name, int size);
// The kitchen panel's built-in art for `name` at `size`, if it had one —
// those never need fetching.
bool builtInNamed(const char* name, int size);

// The weather slot for a Home Assistant condition ("partlycloudy",
// "night-partlycloudy"...): "vwx_<cond>" (88 px) or "vwxm_<cond>" (32 px).
void weatherKey(char* out, size_t cap, const char* condition, bool small);
// The wind arrow slot for a bearing (degrees, 0 = north).
const char* windKey(float bearing);

// Hooks the theme (theme.cpp) fills: a pack's slot, a fetched icon.
using PackLookup = bool (*)(const char* key, draw::Icon& out);
using NamedLookup = bool (*)(const char* name, int size, draw::Icon& out);
void setLookups(PackLookup pack, NamedLookup named);

// Pictures (album art), in the server's 4-bit "spectra" format, by the
// media player's picture and the size drawn; fetched beforehand like icons.
using PictureLookup = const uint8_t* (*)(const char* src, int w, int h);
void setPictureLookup(PictureLookup lookup);
// (w, h: its size on the screen; it's fetched draw::fine.scale times that.)
const uint8_t* picture(const char* src, int w, int h);

// The collecting pass: while on, every icon and picture asked for that
// isn't built in or already fetched is noted, for fetching before the real
// draw (screens.h).
struct Need {
  char name[48];
  uint8_t size;
};
struct PictureNeed {
  char src[256];
  uint16_t w, h;
};
void beginCollect();
void endCollect();
int needCount();
const Need& need(int i);
int pictureNeedCount();
const PictureNeed& pictureNeed(int i);

}  // namespace icons
