#include "render/icons.h"

#include <stdio.h>
#include <string.h>

#include "render/pgm.h"
#include "render/system_icons.h"
// The kitchen panel's own art: the built-in theme.
#include "kd/weather_icons.h"
#include "kd/mini_weather_icons.h"
#include "kd/small_icons.h"
#include "kd/dashboard_icons.h"
#include "kd/status_icons.h"
#include "kd/status_icons_sm.h"
#include "kd/heating_icons.h"
#include "kd/security_icons.h"
#include "kd/error_icons.h"
#include "kd/charge_screen.h"

namespace icons {

namespace {

struct Entry {
  const char* key;
  const uint8_t* bits;
  uint16_t size;
  bool fourBit;
};

// Slots (Switchboard Server's viewport-slots.js) -> the panel's bitmaps.
// Weather falls back as the panel did: hail is snowy, lightning thundery,
// windy-variant windy, exceptional cloudy.
const Entry SLOTS[] = {
    {"vwx_sunny", icon_sunny, 88, true},
    {"vwx_clear_night", icon_night, 88, true},
    {"vwx_partlycloudy", icon_partly_cloudy, 88, true},
    {"vwx_night_partlycloudy", icon_night_partly_cloudy, 88, true},
    {"vwx_cloudy", icon_cloudy, 88, true},
    {"vwx_rainy", icon_rainy, 88, true},
    {"vwx_pouring", icon_pouring, 88, true},
    {"vwx_lightning", icon_lightning_rainy, 88, true},
    {"vwx_lightning_rainy", icon_lightning_rainy, 88, true},
    {"vwx_snowy", icon_snowy, 88, true},
    {"vwx_snowy_rainy", icon_snowy_rainy, 88, true},
    {"vwx_hail", icon_snowy, 88, true},
    {"vwx_fog", icon_fog, 88, true},
    {"vwx_windy", icon_windy, 88, true},
    {"vwx_windy_variant", icon_windy, 88, true},
    {"vwx_exceptional", icon_cloudy, 88, true},
    {"vwxm_sunny", mi_sunny, 32, true},
    {"vwxm_clear_night", mi_night, 32, true},
    {"vwxm_partlycloudy", mi_partly_cloudy, 32, true},
    {"vwxm_night_partlycloudy", mi_night_partly_cloudy, 32, true},
    {"vwxm_cloudy", mi_cloudy, 32, true},
    {"vwxm_rainy", mi_rainy, 32, true},
    {"vwxm_pouring", mi_pouring, 32, true},
    {"vwxm_lightning", mi_lightning_rainy, 32, true},
    {"vwxm_lightning_rainy", mi_lightning_rainy, 32, true},
    {"vwxm_snowy", mi_snowy, 32, true},
    {"vwxm_snowy_rainy", mi_snowy_rainy, 32, true},
    {"vwxm_hail", mi_snowy, 32, true},
    {"vwxm_fog", mi_fog, 32, true},
    {"vwxm_windy", mi_windy, 32, true},
    {"vwxm_windy_variant", mi_windy, 32, true},
    {"vwxm_exceptional", mi_cloudy, 32, true},

    {"vd_humidity", si_humidity, 24, true},
    {"vd_wind", si_wind, 24, true},
    {"vd_uv", si_uv, 24, true},
    {"vd_wind_n", si_wind_n, 24, true},
    {"vd_wind_ne", si_wind_ne, 24, true},
    {"vd_wind_e", si_wind_e, 24, true},
    {"vd_wind_se", si_wind_se, 24, true},
    {"vd_wind_s", si_wind_s, 24, true},
    {"vd_wind_sw", si_wind_sw, 24, true},
    {"vd_wind_w", si_wind_w, 24, true},
    {"vd_wind_nw", si_wind_nw, 24, true},
    {"vd_rain", si_water, 24, true},
    {"vd_solar", si_solar_panel, 24, true},

    // The solar tile has two built-in looks: its own yellow when it has made
    // 1 kWh or more (ve_solar), plain black before (ve_solar_off).
    {"ve_solar", di_solar, 44, true},
    {"ve_solar_off", di_solar_black, 44, true},
    {"ve_load", di_load, 44, true},
    {"ve_import", di_grid_import, 44, true},
    {"ve_export", di_grid_export, 44, true},
    {"ve_battery", si_battery, 24, true},

    {"vf_battery", si_dev_battery, 24, true},
    {"vf_refresh", si_refresh, 16, true},
    {"vf_quiet", si_bed_clock, 16, true},

    {"vh_flame", hi_flame_lg, 64, true},
    {"vh_water", hi_drop_lg, 64, true},
    {"vh_calling", hi_flame_sm, 20, true},

    {"vs_disarmed", sec_shield_check, 64, true},
    {"vs_armed", sec_shield_alert, 64, true},
    {"vs_door", sec_door_sm, 20, true},
    {"vs_window", sec_window_sm, 20, true},
    {"vs_motion", sec_motion_sm, 20, true},
    {"vs_camera", sec_camera_sm, 20, true},

    {"vx_wifi_off", err_wifi_off, 96, true},
    {"vx_cloud_off", err_cloud_off, 96, true},
    {"vx_plug", cs_plug, 96, true},
    {"vx_wifi", sysicons::WIFI, sysicons::WIFI_W, false},
    {"vx_link", sysicons::LINK, sysicons::LINK_W, false},
    {"vx_update", sysicons::UPDATE, sysicons::UPDATE_W, false},
    {"vx_logo", sysicons::LOGO, sysicons::LOGO_W, false},
    {"vx_server_off", sysicons::SERVER_OFF, sysicons::SERVER_OFF_W, false},
    {"vx_not_set_up", sysicons::NOT_SET_UP, sysicons::NOT_SET_UP_W, false},
};

// Icons a layout can pick that the panel had art for, at the size it drew
// them: the status bar's (32 px) and the "now" list's (24 px).
struct Named {
  const char* name;
  uint16_t size;
  const uint8_t* bits;
};
const Named NAMED[] = {
    {"shield-check", 32, sb_shield_check},
    {"shield-alert", 32, sb_shield_alert},
    {"door-open", 32, sb_door_open},
    {"door-closed", 32, sb_door_closed},
    {"window-open", 32, sb_window_open},
    {"window-closed", 32, sb_window_closed},
    {"radiator", 32, sb_radiator},
    {"radiator-off", 32, sb_radiator_off},
    {"water-boiler", 32, sb_water_boiler},
    {"water-boiler-off", 32, sb_water_boiler_off},
    {"flower", 32, sb_flower},
    {"robot-vacuum", 32, sb_robot_vacuum},
    {"robot-vacuum-variant", 32, sb_robot_vacuum_variant},
    {"robot-mower", 32, sb_robot_mower},
    {"shield-check", 24, sb_shield_check_sm},
    {"shield-alert", 24, sb_shield_alert_sm},
    {"radiator", 24, sb_radiator_sm},
    {"water-boiler", 24, sb_water_boiler_sm},
    {"door-open", 24, sb_door_open_sm},
    {"window-open", 24, sb_window_open_sm},
    {"robot-vacuum", 24, sb_robot_vacuum_sm},
    // The panel drew both vacuums' "now" items with the one small vacuum.
    {"robot-vacuum-variant", 24, sb_robot_vacuum_sm},
    {"robot-mower", 24, sb_robot_mower_sm},
    {"watering-can", 24, si_watering_can},
};

PackLookup g_pack = nullptr;
NamedLookup g_named = nullptr;
PictureLookup g_picture = nullptr;

constexpr int MAX_NEEDS = 48;
constexpr int MAX_PICTURE_NEEDS = 4;
bool g_collecting = false;
Need g_needs[MAX_NEEDS];
int g_needCount = 0;
PictureNeed g_pictureNeeds[MAX_PICTURE_NEEDS];
int g_pictureNeedCount = 0;

void noteNeed(const char* name, int size) {
  if (!g_collecting || g_needCount >= MAX_NEEDS || strlen(name) >= sizeof(g_needs[0].name)) return;
  for (int i = 0; i < g_needCount; ++i)
    if (g_needs[i].size == size && !strcmp(g_needs[i].name, name)) return;
  snprintf(g_needs[g_needCount].name, sizeof(g_needs[0].name), "%s", name);
  g_needs[g_needCount].size = static_cast<uint16_t>(size);
  ++g_needCount;
}

const Entry* builtInSlot(const char* key) {
  for (const auto& e : SLOTS)
    if (!strcmp(e.key, key)) return &e;
  return nullptr;
}

const Named* builtInName(const char* name, int size) {
  for (const auto& n : NAMED)
    if (n.size == size && !strcmp(n.name, name)) return &n;
  return nullptr;
}

}  // namespace

void setLookups(PackLookup pack, NamedLookup named) {
  g_pack = pack;
  g_named = named;
}

bool themed(const char* key) {
  draw::Icon ic;
  return g_pack && g_pack(key, ic);
}

draw::Icon slot(const char* key) {
  draw::Icon ic;
  if (g_pack && g_pack(key, ic)) return ic;
  if (const Entry* e = builtInSlot(key)) {
    ic.w = ic.h = e->size;
    ic.bits = e->bits;
    ic.fourBit = e->fourBit;
  }
  return ic;
}

bool builtInNamed(const char* name, int size) { return name && builtInName(name, size); }

draw::Icon named(const char* name, int size) {
  draw::Icon ic;
  if (!name || !*name) return ic;
  if (const Named* n = builtInName(name, size)) {
    ic.w = ic.h = n->size;
    ic.bits = n->bits;
    ic.fourBit = true;
    return ic;
  }
  // Fetched from the server: at the panel's finer resolution where it has
  // one (draw::fine), drawn without enlarging.
  const int fetched = size * draw::fine.scale;
  if (!(g_named && g_named(name, fetched, ic))) noteNeed(name, fetched);
  ic.fine = ic.bits && draw::fine.scale > 1;
  return ic;
}

void setPictureLookup(PictureLookup lookup) { g_picture = lookup; }

const uint8_t* picture(const char* src, int w, int h) {
  if (!src || !*src) return nullptr;
  // Fetched (and kept) at the panel's own resolution where it's finer than
  // the screen's (draw::fine): the E1004's full-resolution photos.
  w *= draw::fine.scale;
  h *= draw::fine.scale;
  const uint8_t* p = g_picture ? g_picture(src, w, h) : nullptr;
  if (!p && g_collecting && g_pictureNeedCount < MAX_PICTURE_NEEDS && strlen(src) < sizeof(g_pictureNeeds[0].src)) {
    // Once each (a section over a photo is measured, then drawn).
    for (int i = 0; i < g_pictureNeedCount; ++i)
      if (g_pictureNeeds[i].w == w && g_pictureNeeds[i].h == h && !strcmp(g_pictureNeeds[i].src, src)) return p;
    PictureNeed& n = g_pictureNeeds[g_pictureNeedCount++];
    snprintf(n.src, sizeof(n.src), "%s", src);
    n.w = static_cast<uint16_t>(w);
    n.h = static_cast<uint16_t>(h);
  }
  return p;
}

void beginCollect() {
  g_collecting = true;
  g_needCount = 0;
  g_pictureNeedCount = 0;
}
void endCollect() { g_collecting = false; }
int needCount() { return g_needCount; }
const Need& need(int i) { return g_needs[i]; }
int pictureNeedCount() { return g_pictureNeedCount; }
const PictureNeed& pictureNeed(int i) { return g_pictureNeeds[i]; }

void weatherKey(char* out, size_t cap, const char* condition, bool small) {
  char cond[32];
  snprintf(cond, sizeof(cond), "%s", condition && *condition ? condition : "cloudy");
  for (char* p = cond; *p; ++p)
    if (*p == '-') *p = '_';
  snprintf(out, cap, "%s_%s", small ? "vwxm" : "vwx", cond);
  if (!builtInSlot(out) && !themed(out)) snprintf(out, cap, "%s_cloudy", small ? "vwxm" : "vwx");
}

const char* windKey(float bearing) {
  static const char* const K[] = {"vd_wind_n", "vd_wind_ne", "vd_wind_e", "vd_wind_se",
                                  "vd_wind_s", "vd_wind_sw", "vd_wind_w", "vd_wind_nw"};
  // The panel's own rule: (bearing + 22.5) / 45, eight ways.
  int sector = static_cast<int>((bearing + 22.5f) / 45.0f) % 8;
  if (sector < 0) sector += 8;
  return K[sector];
}

}  // namespace icons
