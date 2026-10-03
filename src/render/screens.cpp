// screens.cpp — every screen and section, drawn from Switchboard Server's
// state. The kitchen panel's own sections keep its coordinates and its
// constants (named as it named them: LC_X, RC_X, ITEM_H...), relative to the
// column they're placed in; see screens.h.

#include "render/screens.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "render/icons.h"

using draw::Gfx;
using draw::Icon;
using draw::Mode;

namespace screens {

namespace {

// A column a section is drawn in: its left edge and width; y is where the
// section starts (sections stack down a column).
struct Box {
  int x;
  int y;
  int w;
};

const char* str(JsonVariantConst v, const char* fallback = "") {
  const char* s = v.as<const char*>();
  return s ? s : fallback;
}
bool has(JsonVariantConst v) { return !v.isNull(); }

void upper(char* out, size_t cap, const char* s) {
  size_t i = 0;
  for (; s[i] && i + 1 < cap; ++i) out[i] = (s[i] >= 'a' && s[i] <= 'z') ? s[i] - 32 : s[i];
  out[i] = 0;
}

uint16_t col(JsonVariantConst v, int fallback = 1) { return draw::color(v.isNull() ? fallback : v.as<int>()); }

// A section's own heading, for the sections that print one ("NOW", "TODAY",
// "DOORS"): its title in capitals, or the panel's word.
void heading(char* out, size_t cap, JsonObjectConst s, const char* fallback) {
  const char* t = str(s["title"]);
  upper(out, cap, *t ? t : fallback);
}

// =============================================================================
// Status screen, left column (the panel's LC_X = 5, LC_W = 235)
// =============================================================================

// Weather: big temperature, icon, humidity / wind / UV, the rain or solar
// line, and the next days. 268 px tall (the panel's ENERGY_Y).
int weather(Gfx& g, JsonObjectConst d, Box b) {
  const int LC_X = b.x + 5;
  const int y0 = b.y;
  const int TEMP_Y = y0 + 70, DETAIL_Y = y0 + 106, RAIN_SOLAR_Y = y0 + 132, MINI_FC_Y = y0 + 152;
  const int CELL = (b.w - 10) / 3;  // the panel's (LC_W + LC_X) / 3 = 80
  const int MINI_ICON_S = 32;
  if (d.isNull()) {
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, LC_X, y0 + 40, "No weather entity");
    return 268;
  }

  // ---- Big temperature: blue below 0, red above 20 ----
  {
    const char* t = str(d["temperatureText"], "--");
    const float v = static_cast<float>(atof(t));
    uint16_t tc = GxEPD_BLACK;
    if (v < 0.0f) tc = GxEPD_BLUE;
    if (v > 20.0f) tc = GxEPD_RED;
    draw::face(g, draw::BOLD82, tc);
    draw::text(g, LC_X, TEMP_Y, t);
    // The degree ring, drawn (the font has no degree sign).
    const draw::Bounds bb = draw::bounds(g, t, LC_X, TEMP_Y);
    const int cx = LC_X + bb.w + 15;
    const int cy = TEMP_Y - bb.h + 3;
    for (int r = 8; r >= 5; r--) g.drawCircle(cx, cy, r, tc);
  }

  // ---- Weather icon ----
  {
    char key[40];
    icons::weatherKey(key, sizeof(key), str(d["icon"], str(d["condition"])), false);
    draw::icon(g, icons::slot(key), b.x + 152, y0 + 2, GxEPD_BLACK, Mode::Opaque);
  }

  // ---- Detail row: humidity · wind · UV ----
  {
    const int DX_OFF = 5;
    const int dy = DETAIL_Y;
    char buf[24];
    draw::face(g, draw::REG18, GxEPD_BLACK);
    int dx = LC_X + DX_OFF;
    draw::icon(g, icons::slot("vd_humidity"), dx, dy - 18, GxEPD_BLACK);
    snprintf(buf, sizeof(buf), "%s%%", str(d["humidityText"], "--"));
    draw::text(g, dx + 26, dy, buf);

    dx = LC_X + CELL + DX_OFF;
    draw::icon(g, icons::slot("vd_wind"), dx, dy - 18, GxEPD_BLACK);
    const char* wind = str(d["windText"], "--");
    draw::text(g, dx + 26, dy, wind);
    const draw::Bounds wb = draw::bounds(g, wind);
    draw::icon(g, icons::slot(icons::windKey(d["bearing"] | 0.0f)), dx + 26 + wb.w + 4, dy - 18, GxEPD_BLACK);

    dx = LC_X + CELL * 2 + DX_OFF;
    draw::icon(g, icons::slot("vd_uv"), dx, dy - 18, GxEPD_BLACK);
    draw::text(g, dx + 26, dy, str(d["uvText"], "--"));
  }

  // ---- Rain / solar row: rain first, else the solar forecast ----
  {
    draw::face(g, draw::BOLD18, GxEPD_BLACK);
    JsonObjectConst rain = d["rain"];
    const char* rs = str(rain["state"], "dry");
    if (!rain.isNull() && strcmp(rs, "dry") != 0) {
      draw::icon(g, icons::slot("vd_rain"), LC_X, RAIN_SOLAR_Y - 16, GxEPD_BLUE);
      g.setTextColor(GxEPD_BLUE);
      draw::text(g, LC_X + 28, RAIN_SOLAR_Y, str(rain["text"]));
    } else if (has(d["solar"])) {
      draw::icon(g, icons::slot("vd_solar"), LC_X, RAIN_SOLAR_Y - 16, GxEPD_BLACK);
      g.setTextColor(GxEPD_BLACK);
      draw::text(g, LC_X + 28, RAIN_SOLAR_Y, str(d["solar"]["text"]));
    }
  }

  // ---- The next days: label | icon | high ----
  {
    JsonArrayConst fc = d["forecast"];
    const int FCELL_W = CELL;
    int i = 0;
    for (JsonObjectConst f : fc) {
      if (i >= 3) break;
      const int cx = LC_X + i * FCELL_W;
      const int fy = MINI_FC_Y;
      draw::face(g, draw::BOLD18, GxEPD_BLACK);
      draw::textCentered(g, str(f["label"]), cx, FCELL_W, fy + 11);
      char key[40];
      icons::weatherKey(key, sizeof(key), str(f["condition"]), true);
      draw::icon(g, icons::slot(key), cx + (FCELL_W - MINI_ICON_S) / 2, fy + 14, GxEPD_BLACK);
      char hi[16];
      // "18°": the font has no degree sign, so it prints as "18", as it did.
      if (f["high"].isNull()) snprintf(hi, sizeof(hi), "--\xC2\xB0");
      else snprintf(hi, sizeof(hi), "%d\xC2\xB0", static_cast<int>(roundf(f["high"].as<float>())));
      draw::textCentered(g, hi, cx, FCELL_W, fy + 14 + MINI_ICON_S + 12);
      ++i;
    }
  }
  return 268;
}

// Energy, as a 2x2 grid: solar | used / imported | exported. An icon turns
// its colour from 1 kWh. 164 px tall (BATT_Y - ENERGY_Y).
void energyCell(Gfx& g, const Icon& ic, const char* value, uint16_t iconColor, int x, int y, int cellW) {
  const int ICON = 44;
  draw::icon(g, ic, x + (cellW - ICON) / 2, y, iconColor, Mode::Opaque);
  draw::face(g, draw::REG18, GxEPD_BLACK);
  const draw::Bounds b = draw::bounds(g, value);
  g.setCursor(x + (cellW - static_cast<int>(b.w)) / 2 - b.x, y + ICON + 13);
  g.print(value);
}

const char* energyText(JsonVariantConst m) { return m.isNull() ? "n/a" : str(m["text"], "n/a"); }
float energyValue(JsonVariantConst m) { return m.isNull() ? 0.0f : static_cast<float>(atof(energyText(m))); }

int energyList(Gfx& g, JsonObjectConst d, Box b);

int energy(Gfx& g, JsonObjectConst d, Box b) {
  if (!strcmp(str(d["style"], "grid"), "list")) return energyList(g, d, b);
  const int LC_X = b.x + 5;
  const int CELL_W = (b.w - 15) / 2;  // the panel's LC_W / 2 = 117
  const int rowGap = 44 + 13 + 14;
  int ey = b.y;
  const int cellL = LC_X, cellR = LC_X + CELL_W;
  auto accent = [](float v, uint16_t c) { return v >= 1.0f ? c : static_cast<uint16_t>(GxEPD_BLACK); };

  // Solar: the built-in panel has its own yellow (1 kWh or more) or plain
  // black; a themed icon is tinted yellow from 1 kWh, like the others.
  const float sv = energyValue(d["solarToday"]);
  if (icons::themed("ve_solar")) energyCell(g, icons::slot("ve_solar"), energyText(d["solarToday"]), accent(sv, GxEPD_YELLOW), cellL, ey, CELL_W);
  else energyCell(g, icons::slot(sv >= 1.0f ? "ve_solar" : "ve_solar_off"), energyText(d["solarToday"]), GxEPD_BLACK, cellL, ey, CELL_W);
  energyCell(g, icons::slot("ve_load"), energyText(d["loadToday"]), accent(energyValue(d["loadToday"]), GxEPD_BLUE), cellR, ey, CELL_W);
  ey += rowGap;
  energyCell(g, icons::slot("ve_import"), energyText(d["gridImport"]), accent(energyValue(d["gridImport"]), GxEPD_RED), cellL, ey, CELL_W);
  energyCell(g, icons::slot("ve_export"), energyText(d["gridExport"]), accent(energyValue(d["gridExport"]), GxEPD_GREEN), cellR, ey, CELL_W);
  return 164;
}

// The home battery: charge, which way it's going and when it'll be full or
// empty, and a battery-shaped bar. 48 px tall.
int battery(Gfx& g, JsonObjectConst d, Box b) {
  const int LC_X = b.x + 5;
  const int BAR_W = b.w - 25;  // the panel's LC_W - 10
  const int BAR_H = 22, BAR_NUB_W = 6, BAR_NUB_H = 12;
  const int ey = b.y;
  const int BAR_Y = ey + 23;
  if (d.isNull()) return 48;
  const uint16_t stateCol = col(d["color"]);
  const int barLeft = LC_X, barRight = LC_X + BAR_W;

  // The status and the time are raised from the panel's (ey+8, ey+22),
  // where the status touched the time and the time touched the bar: the
  // time now ends 3 px above the bar and the status clears it. The icon and
  // the % stay where the panel had them.
  const int STATUS_Y = ey + 1, ETA_Y = ey + 19;
  draw::icon(g, icons::slot("ve_battery"), barLeft, ey - 4, GxEPD_BLACK);
  draw::face(g, draw::BOLD24, GxEPD_BLACK);
  draw::text(g, barLeft + 27, ey + 16, str(d["socText"], "n/a"));
  draw::face(g, draw::BOLD18, stateCol);
  draw::textRight(g, str(d["statusText"], "Idle"), barRight, STATUS_Y);
  const char* eta = str(d["eta"]);
  if (*eta) {
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::textRight(g, eta, barRight, ETA_Y);
  }

  const float soc = d["soc"].isNull() ? 0.0f : d["soc"].as<float>();
  const int bx = barLeft, by = BAR_Y, bw = BAR_W, bh = BAR_H;
  g.drawRect(bx, by, bw, bh, GxEPD_BLACK);
  g.fillRect(bx + 1, by + 1, bw - 2, bh - 2, GxEPD_WHITE);
  const int nubX = bx + bw, nubY = by + (bh - BAR_NUB_H) / 2;
  g.drawRect(nubX, nubY, BAR_NUB_W, BAR_NUB_H, GxEPD_BLACK);
  g.fillRect(nubX + 1, nubY + 1, BAR_NUB_W - 2, BAR_NUB_H - 2, GxEPD_BLACK);
  const int fillW = static_cast<int>((bw - 6) * soc / 100.0f);
  if (fillW > 0) g.fillRect(bx + 3, by + 3, fillW, bh - 6, stateCol);
  return 48;
}

// =============================================================================
// Status screen, main column (the panel's RC_X = 260)
// =============================================================================

// The status bar: up to nine 32 px icons across, a dotted rule under it.
// 50 px tall (RC_STATUS_H).
int statusIcons(Gfx& g, JsonObjectConst d, Box b) {
  const int RC_X = b.x;
  const int RC_STATUS_H = 50;
  const int ICON_Y = b.y + (RC_STATUS_H - 32) / 2;
  const int LEAD = 14, STEP = 59;
  int i = 0;
  for (JsonObjectConst ic : d["icons"].as<JsonArrayConst>()) {
    const int x = RC_X + LEAD + i * STEP;
    if (x + 32 > b.x + b.w) break;
    draw::icon(g, icons::named(str(ic["icon"], "help"), 32), x, ICON_Y, col(ic["color"]), Mode::Opaque);
    ++i;
  }
  draw::dottedH(g, RC_X, draw::PANEL_W, b.y + RC_STATUS_H);
  return RC_STATUS_H;
}

// A two-line item: a colour bar, a 24 px icon, a bold coloured line and a
// plain one under it (the panel's drawNowItem).
const int ITEM_H = 40, ITEM_GAP = 5, RC_BAR_W = 4;

void nowItem(Gfx& g, int RC_X, int top, const Icon& ic, uint16_t c, const char* line1, const char* line2) {
  const int ICON_W = 24;
  const int NX = RC_X + 4;
  const int TEXTX = NX + ICON_W + 8;
  g.fillRect(RC_X, top, RC_BAR_W, ITEM_H, c);
  draw::icon(g, ic, NX + 2, top + (ITEM_H - ICON_W) / 2, c, Mode::Opaque);
  draw::face(g, draw::BOLD24, c);
  draw::text(g, TEXTX, top + 17, line1);
  if (line2 && *line2) {
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, TEXTX, top + 33, line2);
  }
}

// "NOW": the alarm, then whatever is going on.
int now(Gfx& g, JsonObjectConst s, JsonObjectConst d, Box b) {
  const int RC_X = b.x;
  char h[32];
  heading(h, sizeof(h), s, "Now");
  draw::face(g, draw::BOLD18, GxEPD_BLACK);
  draw::text(g, RC_X + 8, b.y + 16, h);
  int itemTop = b.y + 24;
  for (JsonObjectConst it : d["items"].as<JsonArrayConst>()) {
    if (itemTop + ITEM_H > draw::PANEL_H) break;
    nowItem(g, RC_X, itemTop, icons::named(str(it["icon"], "alert"), 24), col(it["color"]), str(it["line1"]), str(it["line2"]));
    itemTop += ITEM_H + ITEM_GAP;
  }
  return itemTop + 10 - b.y;
}

// "TODAY": each event with its calendar's colour, the time and title, and
// the first words of its description under the title; as many as fit.
int calendar(Gfx& g, JsonObjectConst s, JsonObjectConst d, Box b) {
  const int RC_X = b.x, RC_TEXT_X = RC_X + 10, RC_LINE_H = 40;
  const int CAL_SUMMARY_MAX_CHARS = 60;
  const int MAX_Y = draw::PANEL_H - 22;  // the panel's FOOTER_Y - FOOTER_GAP
  char h[32];
  heading(h, sizeof(h), s, "Today");
  draw::face(g, draw::BOLD18, GxEPD_BLACK);
  draw::text(g, RC_X + 8, b.y + 16, h);
  int y = b.y + 21;  // the panel's TODAY_GAP
  JsonArrayConst lines = d["lines"];
  if (lines.size() == 0) {
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, RC_TEXT_X, y + 10, strcmp(h, "TODAY") ? "No events" : "No events today");
    return y + 20 - b.y;
  }
  for (JsonObjectConst ev : lines) {
    if (y > MAX_Y) break;
    const uint16_t c = col(ev["color"]);
    g.fillRect(RC_X, y, RC_BAR_W, RC_LINE_H, c);
    draw::face(g, draw::BOLD24, c);
    g.setCursor(RC_TEXT_X, y + 17);
    int budget = CAL_SUMMARY_MAX_CHARS;
    const char* time = str(ev["time"]);
    if (*time) {
      g.print(time);
      g.print(" ");
      budget -= static_cast<int>(strlen(time)) + 1;
    } else {
      g.print("\xC2\xB7 ");  // "· " — the dot isn't in the font, so a space
      budget -= 2;
    }
    const int titleX = g.getCursorX();
    char title[160];
    const char* t = str(ev["title"]);
    if (static_cast<int>(strlen(t)) > budget && budget > 3) draw::cut(title, sizeof(title), t, budget, budget - 1);
    else snprintf(title, sizeof(title), "%s", t);
    g.print(title);
    const char* desc = str(ev["description"]);
    if (*desc) {
      char dbuf[96];
      draw::cut(dbuf, sizeof(dbuf), desc, 84, 81);
      draw::face(g, draw::REG18, GxEPD_BLACK);
      draw::text(g, titleX, y + 33, dbuf);
    }
    y += RC_LINE_H + 5;
  }
  return y - b.y;
}

// =============================================================================
// Heating screen (the whole panel): the house on the left, flooded red while
// any zone is calling; hot water under it; each zone's bar on the right.
// =============================================================================

int heatingCompact(Gfx& g, JsonObjectConst d, Box b);

int heating(Gfx& g, JsonObjectConst d, Box b) {
  if (b.w < 700) return heatingCompact(g, d, b);
  const int HP_LEFT_W = 300, HP_LEFT_SPLIT_Y = 360;
  const int RPANEL_X = HP_LEFT_W + 24;
  const int HP_RIGHT_W = draw::PANEL_W - RPANEL_X - 20;
  const int HP_ROW_TOP = 18, HP_ROW_H = 52, TBAR_H = 12, HP_FLAME_W = 22;
  const int TBAR_X = RPANEL_X;
  const int TBAR_W = HP_RIGHT_W - HP_FLAME_W - 6;
  const bool heatOn = d["on"] | false;
  JsonObjectConst w = d["water"];
  const bool waterOn = !w.isNull() && (w["on"] | false);
  const float scaleMin = d["scaleMin"] | 5.0f;
  const float scaleMax = d["scaleMax"] | 25.0f;
  const float scaleSpan = (scaleMax - scaleMin) > 0 ? (scaleMax - scaleMin) : 1.0f;
  char buf[48];

  // ---- Heating block ----
  {
    g.fillRect(0, 0, HP_LEFT_W, HP_LEFT_SPLIT_Y, heatOn ? GxEPD_RED : GxEPD_BLACK);
    draw::icon(g, icons::slot("vh_flame"), (HP_LEFT_W - 64) / 2, 24, GxEPD_WHITE, Mode::Ink);
    draw::face(g, draw::BOLD24, GxEPD_WHITE);
    draw::textCentered(g, "HEATING", 0, HP_LEFT_W, 116);
    draw::face(g, draw::BOLD82, GxEPD_WHITE);
    draw::textCentered(g, heatOn ? "ON" : "OFF", 0, HP_LEFT_W, 200);
    draw::face(g, draw::BOLD24, GxEPD_WHITE);
    if (!d["current"].isNull()) {
      char v[16];
      snprintf(buf, sizeof(buf), "%sC now", draw::fixed(v, sizeof(v), d["current"].as<float>(), 1));
      draw::textCentered(g, buf, 0, HP_LEFT_W, 250);
    }
    if (!d["target"].isNull()) {
      char v[16];
      snprintf(buf, sizeof(buf), "Set %sC", draw::fixed(v, sizeof(v), d["target"].as<float>(), 1));
      draw::textCentered(g, buf, 0, HP_LEFT_W, 282);
    }
    snprintf(buf, sizeof(buf), "%d of %d calling", d["calling"] | 0, d["total"] | 0);
    draw::textCentered(g, buf, 0, HP_LEFT_W, 322);
  }
  // ---- Hot water block ----
  {
    g.fillRect(0, HP_LEFT_SPLIT_Y, HP_LEFT_W, draw::PANEL_H - HP_LEFT_SPLIT_Y, waterOn ? GxEPD_RED : GxEPD_BLACK);
    draw::icon(g, icons::slot("vh_water"), 14, HP_LEFT_SPLIT_Y + 28, GxEPD_WHITE, Mode::Ink);
    draw::face(g, draw::BOLD24, GxEPD_WHITE);
    draw::text(g, 90, HP_LEFT_SPLIT_Y + 50, "HOT WATER");
    draw::face(g, draw::BOLD18, GxEPD_WHITE);
    if (!w.isNull()) {
      char cur[8] = "--", tgt[8] = "--";
      if (!w["current"].isNull()) snprintf(cur, sizeof(cur), "%d", w["current"].as<int>());
      if (!w["target"].isNull()) snprintf(tgt, sizeof(tgt), "%d", w["target"].as<int>());
      snprintf(buf, sizeof(buf), "%sC  \xC2\xB7  set %sC", cur, tgt);
      draw::text(g, 90, HP_LEFT_SPLIT_Y + 78, buf);
    }
  }
  // ---- Scale, once at the top ----
  {
    draw::face(g, draw::BOLD18, GxEPD_BLACK);
    snprintf(buf, sizeof(buf), "%dC", static_cast<int>(scaleMin));
    draw::text(g, TBAR_X, HP_ROW_TOP - 2, buf);
    snprintf(buf, sizeof(buf), "%dC", static_cast<int>(scaleMax));
    draw::textRight(g, buf, TBAR_X + TBAR_W, HP_ROW_TOP - 2);
  }
  // ---- Each zone ----
  int i = 0;
  for (JsonObjectConst z : d["zones"].as<JsonArrayConst>()) {
    const int y = HP_ROW_TOP + 10 + i * HP_ROW_H;
    if (y + 40 > draw::PANEL_H) break;
    const bool active = z["active"] | false;
    const uint16_t nameCol = active ? GxEPD_RED : GxEPD_BLACK;
    // The zone's icon, if it has one: 24 px beside the name, centred on its
    // capitals (15 px tall above y + 14), in the name's colour.
    int nameX = RPANEL_X;
    const char* zoneIcon = str(z["icon"]);
    if (*zoneIcon) {
      draw::icon(g, icons::named(zoneIcon, 24), RPANEL_X, y - 5, nameCol, Mode::Opaque);
      nameX += 30;
    }
    draw::face(g, draw::BOLD24, nameCol);
    draw::text(g, nameX, y + 14, str(z["name"]));
    char temps[40] = "";
    char v[16];
    if (!z["current"].isNull()) snprintf(temps, sizeof(temps), "%sC", draw::fixed(v, sizeof(v), z["current"].as<float>(), 1));
    if (!z["target"].isNull()) {
      const size_t n = strlen(temps);
      snprintf(temps + n, sizeof(temps) - n, "  set %sC", draw::fixed(v, sizeof(v), z["target"].as<float>(), 0));
    }
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::textRight(g, temps, TBAR_X + TBAR_W, y + 14);

    const int barY = y + 24;
    g.drawRect(TBAR_X, barY, TBAR_W, TBAR_H, GxEPD_BLACK);
    if (!z["current"].isNull()) {
      auto tempToX = [&](float t) {
        float f = (t - scaleMin) / scaleSpan;
        if (f < 0) f = 0;
        if (f > 1) f = 1;
        return TBAR_X + static_cast<int>(f * (TBAR_W - 1));
      };
      const float cur = z["current"].as<float>();
      const int xCur = tempToX(cur);
      if (!z["target"].isNull()) {
        const float tgt = z["target"].as<float>();
        const int xSet = tempToX(tgt);
        const int xL = xCur < xSet ? xCur : xSet;
        const int xR = xCur < xSet ? xSet : xCur;
        if (xR > xL) g.fillRect(xL + 1, barY + 1, xR - xL - 1, TBAR_H - 2, cur < tgt ? GxEPD_RED : GxEPD_BLUE);
        g.fillRect(xSet, barY - 3, 2, TBAR_H + 6, GxEPD_BLACK);
      } else {
        g.fillRect(xCur - 1, barY - 3, 3, TBAR_H + 6, GxEPD_BLACK);
      }
    }
    draw::icon(g, icons::slot("vh_calling"), TBAR_X + TBAR_W + 4, barY - 4, active ? GxEPD_RED : GxEPD_BLACK, Mode::Ink);
    ++i;
  }
  return draw::PANEL_H - b.y;
}

// The heating in a column: each zone's name, temperatures and bar.
int heatingCompact(Gfx& g, JsonObjectConst d, Box b) {
  const int x = b.x + 8, w = b.w - 16;
  const float lo = d["scaleMin"] | 5.0f, hi = d["scaleMax"] | 25.0f;
  const float span = hi - lo > 0 ? hi - lo : 1.0f;
  char buf[48];
  int y = b.y + 4;
  draw::face(g, draw::BOLD18, (d["on"] | false) ? GxEPD_RED : GxEPD_BLACK);
  snprintf(buf, sizeof(buf), "HEATING %s  %d of %d calling", (d["on"] | false) ? "ON" : "OFF", d["calling"] | 0, d["total"] | 0);
  draw::text(g, x, y + 16, buf);
  y += 26;
  for (JsonObjectConst z : d["zones"].as<JsonArrayConst>()) {
    if (y + 34 > draw::PANEL_H - 24) break;
    const bool active = z["active"] | false;
    draw::face(g, draw::BOLD18, active ? GxEPD_RED : GxEPD_BLACK);
    draw::text(g, x, y + 14, str(z["name"]));
    if (!z["current"].isNull()) {
      snprintf(buf, sizeof(buf), "%.1fC", z["current"].as<float>());
      draw::face(g, draw::REG18, GxEPD_BLACK);
      draw::textRight(g, buf, x + w, y + 14);
    }
    g.drawRect(x, y + 20, w, 8, GxEPD_BLACK);
    if (!z["current"].isNull() && !z["target"].isNull()) {
      auto px = [&](float t) {
        const float f = fminf(1.0f, fmaxf(0.0f, (t - lo) / span));
        return x + static_cast<int>(f * (w - 1));
      };
      const int a = px(z["current"].as<float>()), c = px(z["target"].as<float>());
      if (a != c) g.fillRect((a < c ? a : c) + 1, y + 21, abs(a - c) - 1, 6, a < c ? GxEPD_RED : GxEPD_BLUE);
      g.fillRect(c, y + 18, 2, 12, GxEPD_BLACK);
    }
    y += 36;
  }
  return y - b.y;
}

// =============================================================================
// Security screen: the alarm | doors and windows | motion and cameras.
// =============================================================================

bool isWindows(JsonObjectConst s) {
  const char* t = str(s["title"]);
  return strstr(t, "indow") != nullptr;
}

// The alarm's badge, flooded green (disarmed) or red, and under it a count
// line for each doors / windows / motion / cameras section on the screen.
int alarm(Gfx& g, JsonObjectConst d, Box b, JsonArrayConst siblings) {
  const int x0 = b.x, y0 = b.y;
  if (d.isNull()) {
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, x0 + 8, y0 + 30, "No alarm panel");
    return 40;
  }
  const bool disarmed = !strcmp(str(d["state"]), "disarmed");
  g.fillRect(x0, y0, b.w, 100, disarmed ? GxEPD_GREEN : GxEPD_RED);
  draw::icon(g, icons::slot(disarmed ? "vs_disarmed" : "vs_armed"), x0 + 12, y0 + 18, GxEPD_WHITE, Mode::Mask);
  draw::face(g, draw::BOLD24, GxEPD_WHITE);
  draw::text(g, x0 + 86, y0 + 46, disarmed ? "DISARMED" : "ARMED");
  draw::face(g, draw::REG18, GxEPD_WHITE);
  char buf[96];
  snprintf(buf, sizeof(buf), "since %s", str(d["sinceTime"]));
  draw::text(g, x0 + 86, y0 + 68, buf);

  const int SX = x0 + 12;
  int sy = y0 + 116;
  const int SH = 30;
  auto row = [&](const char* key, const char* text, uint16_t c) {
    draw::icon(g, icons::slot(key), SX, sy, c, Mode::Mask);
    draw::face(g, draw::REG18, c);
    draw::text(g, SX + 26, sy + 15, text);
    sy += SH;
  };
  if (d["summary"] | false) {
    for (JsonObjectConst s : siblings) {
      const char* type = str(s["type"]);
      JsonObjectConst sd = s["data"];
      if (!strcmp(type, "openings")) {
        const int n = sd["total"] | 0, open = sd["open"] | 0;
        // As the panel worded them (the dash isn't in the font).
        if (isWindows(s)) {
          snprintf(buf, sizeof(buf), "%d window%s%s", open, open == 1 ? "" : "s", open == 0 ? " \xE2\x80\x94 all closed" : " open");
          row("vs_window", buf, open > 0 ? GxEPD_RED : GxEPD_BLACK);
        } else {
          char what[32];
          snprintf(what, sizeof(what), "%s", *str(s["title"]) ? str(s["title"]) : "doors");
          for (char* p = what; *p; ++p)
            if (*p >= 'A' && *p <= 'Z') *p += 32;
          if (open == 0) snprintf(buf, sizeof(buf), "%d %s \xE2\x80\x94 all closed", n, what);
          else snprintf(buf, sizeof(buf), "%d %s \xE2\x80\x94 %d open", n, what, open);
          row("vs_door", buf, open > 0 ? GxEPD_RED : GxEPD_BLACK);
        }
      } else if (!strcmp(type, "motion")) {
        snprintf(buf, sizeof(buf), "%d motion sensors", static_cast<int>(sd["sensors"].as<JsonArrayConst>().size()));
        row("vs_motion", buf, GxEPD_BLACK);
      } else if (!strcmp(type, "cameras")) {
        snprintf(buf, sizeof(buf), "%d cameras", static_cast<int>(sd["cameras"].as<JsonArrayConst>().size()));
        row("vs_camera", buf, GxEPD_BLACK);
      }
    }
  } else {
    // Without the summary: when it was last armed, disarmed, triggered.
    draw::face(g, draw::REG18, GxEPD_BLACK);
    JsonObjectConst last = d["last"];
    const char* const K[] = {"armed", "disarmed", "triggered"};
    for (const char* k : K) {
      const char* v = str(last[k]);
      if (!*v) continue;
      snprintf(buf, sizeof(buf), "Last %s %s", k, v);
      draw::text(g, SX, sy + 15, buf);
      sy += 26;
    }
  }
  return sy - y0;
}

// A heading, underlined, then one row per sensor: a colour bar, its icon
// and name in red (open, or motion now) or green, the time it changed.
struct SecRow {
  const char* name;
  bool on;
  uint16_t color;
  const char* time;
};

int secRows(Gfx& g, Box b, const char* title, const char* iconKey, JsonArrayConst items, SecRow (*rowOf)(JsonObjectConst)) {
  const int SC_ROW_H = 28, SC_ICON_W = 20, SC_BAR_W = 4;
  const int panelX = b.x;
  // Heading
  draw::face(g, draw::BOLD18, GxEPD_BLACK);
  draw::text(g, panelX, b.y + 44, title);
  const draw::Bounds hb = draw::bounds(g, title);
  g.drawFastHLine(panelX, b.y + 44 + 4, hb.w + 2, GxEPD_BLACK);
  int i = 0;
  for (JsonObjectConst it : items) {
    const int y = b.y + 60 + i * SC_ROW_H;
    if (y + SC_ROW_H > draw::PANEL_H) break;
    const SecRow r = rowOf(it);
    g.fillRect(panelX, y + 2, SC_BAR_W, SC_ROW_H - 4, r.color);
    draw::icon(g, icons::slot(iconKey), panelX + SC_BAR_W + 4, y + 4, r.color, Mode::Mask);
    draw::face(g, draw::BOLD18, r.color);
    draw::text(g, panelX + SC_BAR_W + SC_ICON_W + 8, y + 18, r.name);
    if (r.time && *r.time) {
      draw::face(g, draw::REG18, GxEPD_BLACK);
      const draw::Bounds tb = draw::bounds(g, r.time);
      draw::text(g, panelX + b.w - tb.w - 4, y + 18, r.time);
    }
    ++i;
  }
  return SC_ROW_H * (static_cast<int>(items.size()) + 1);
}

int openings(Gfx& g, JsonObjectConst s, JsonObjectConst d, Box b) {
  char h[32];
  heading(h, sizeof(h), s, "Doors");
  return secRows(g, b, h, isWindows(s) ? "vs_window" : "vs_door", d["items"], [](JsonObjectConst it) {
    const bool open = it["open"] | false;
    return SecRow{str(it["name"]), open, col(it["color"], open ? 2 : 4), str(it["time"])};
  });
}

int motion(Gfx& g, JsonObjectConst s, JsonObjectConst d, Box b) {
  char h[32];
  heading(h, sizeof(h), s, "Motion");
  return secRows(g, b, h, "vs_motion", d["sensors"], [](JsonObjectConst it) {
    const bool on = it["on"] | false;
    return SecRow{str(it["name"]), on, on ? static_cast<uint16_t>(GxEPD_RED) : static_cast<uint16_t>(GxEPD_GREEN), str(it["time"])};
  });
}

int cameras(Gfx& g, JsonObjectConst s, JsonObjectConst d, Box b) {
  char h[32];
  heading(h, sizeof(h), s, "Cameras");
  return secRows(g, b, h, "vs_camera", d["cameras"], [](JsonObjectConst it) {
    const bool on = it["on"] | false;
    return SecRow{str(it["name"]), on, on ? static_cast<uint16_t>(GxEPD_RED) : static_cast<uint16_t>(GxEPD_GREEN), str(it["time"])};
  });
}

// =============================================================================
// The other sections, as the server's preview draws them, in the panel's
// fonts and colours.
// =============================================================================

// A small capitalised label over a section that doesn't word its own.
int label(Gfx& g, JsonObjectConst s, Box b) {
  const char* t = str(s["title"]);
  if (!*t) return 0;
  char h[48];
  upper(h, sizeof(h), t);
  draw::face(g, draw::BOLD18, GxEPD_BLACK);
  draw::text(g, b.x + 8, b.y + 16, h);
  return 24;
}

int energyList(Gfx& g, JsonObjectConst d, Box b) {
  struct R {
    const char* icon;
    const char* label;
    const char* key;
    int color;
  };
  const R rows[] = {{"weather-sunny-alert", "PREDICTED", "solarExpected", 1},
                    {"solar-power-variant", "GENERATED", "solarToday", 3},
                    {"home-lightning-bolt-outline", "HOUSE USED", "loadToday", 1},
                    {"transmission-tower-import", "FROM GRID", "gridImport", 2},
                    {"transmission-tower-export", "TO GRID", "gridExport", 4}};
  int y = b.y + 6;
  for (const R& r : rows) {
    draw::icon(g, icons::named(r.icon, 24), b.x + 8, y + 14, GxEPD_BLACK, Mode::Ink);
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, b.x + 42, y + 16, r.label);
    JsonVariantConst m = d[r.key];
    char v[32];
    if (m.isNull()) snprintf(v, sizeof(v), "--");
    else snprintf(v, sizeof(v), "%s", str(m["text"], "--"));
    draw::face(g, draw::BOLD24, draw::color(r.color));
    draw::text(g, b.x + 42, y + 42, v);
    if (!strcmp(r.key, "solarToday") && !d["solarPct"].isNull()) {
      // Beside the value: "64% of predicted" where it fits, else "64%".
      const int valueEnd = g.getCursorX() + 8, right = b.x + b.w - 8;
      char pct[32];
      draw::face(g, draw::REG18, GxEPD_BLACK);
      for (const char* fmt : {"%d%% of predicted", "%d%%"}) {
        snprintf(pct, sizeof(pct), fmt, d["solarPct"].as<int>());
        int16_t bx, by;
        uint16_t w, h;
        g.getTextBounds(pct, 0, 0, &bx, &by, &w, &h);
        if (valueEnd + static_cast<int>(w) <= right) {
          draw::textRight(g, pct, right, y + 42);
          break;
        }
      }
    }
    g.drawFastHLine(b.x + 8, y + 54, b.w - 16, GxEPD_BLACK);
    y += 60;
  }
  return y - b.y;
}

// Solar against its forecast on top; below, use stacked by source (grid,
// battery, solar) above the line, and charging the battery then export to
// the grid under it.
int energyGraph(Gfx& g, JsonObjectConst d, Box b) {
  const int x0 = b.x + 8, W = b.w - 16;
  JsonArrayConst labels = d["labels"];
  const int n = labels.size() ? static_cast<int>(labels.size()) : 24;
  const float bw = static_cast<float>(W) / n;
  JsonObjectConst c = d["colors"];
  char buf[64];
  int y = b.y + 4;

  // ---- Solar ----
  JsonObjectConst sol = d["solar"];
  const float tMax = fmaxf(0.5f, sol["max"] | 0.0f);
  const int TH = 120;
  draw::face(g, draw::BOLD18, GxEPD_BLACK);
  draw::text(g, x0, y + 16, "Solar");
  JsonObjectConst t = d["totals"];
  draw::face(g, draw::REG18, GxEPD_BLACK);
  snprintf(buf, sizeof(buf), "%.1f kW", tMax);
  draw::textRight(g, buf, x0 + W, y + 16);
  if (!t["solar"].isNull() || !t["forecast"].isNull()) {
    snprintf(buf, sizeof(buf), "actual %.1f / predicted %.1f kWh", t["solar"] | 0.0f, t["forecast"] | 0.0f);
    draw::text(g, x0 + 60, y + 16, buf);
  }
  y += 22;
  const int top = y;
  auto ty = [&](float v) { return top + TH - static_cast<int>((v / tMax) * (TH - 6)); };
  int i = 0;
  for (JsonVariantConst v : sol["actual"].as<JsonArrayConst>()) {
    if (!v.isNull() && v.as<float>() > 0) {
      const int yy = ty(v.as<float>());
      g.fillRect(x0 + static_cast<int>(i * bw) + 1, yy, static_cast<int>(bw) - 2 > 1 ? static_cast<int>(bw) - 2 : 1, top + TH - yy, col(c["solar"], 3));
    }
    ++i;
  }
  // The forecast: dots along its line.
  i = 0;
  for (JsonVariantConst v : sol["forecast"].as<JsonArrayConst>()) {
    if (!v.isNull()) {
      const int cx = x0 + static_cast<int>((i + 0.5f) * bw);
      g.fillRect(cx - 2, ty(v.as<float>()) - 1, 4, 3, col(c["forecast"], 5));
    }
    ++i;
  }
  g.drawFastHLine(x0, top + TH, W, GxEPD_BLACK);
  y = top + TH + 4;
  draw::face(g, draw::REG18, GxEPD_BLACK);
  i = 0;
  for (JsonVariantConst l : labels) {
    if (i % ((n + 7) / 8) == 0) draw::text(g, x0 + static_cast<int>(i * bw), y + 14, str(l));
    ++i;
  }
  y += 22;

  // ---- Use ----
  JsonObjectConst u = d["usage"];
  const float upMax = fmaxf(0.5f, u["max"] | 0.0f);
  const float downMax = u["exportMax"] | 0.0f;
  const int BH = 140;
  draw::face(g, draw::BOLD18, GxEPD_BLACK);
  draw::text(g, x0, y + 16, "Use");
  y += 22;
  const int up = static_cast<int>((BH - 4) * (upMax / (upMax + downMax)));
  const int axisY = y + 2 + up;
  const float scale = up / upMax;
  JsonArrayConst grid = u["fromGrid"], batt = u["fromBattery"], solar = u["fromSolar"], exp_ = u["export"], charge = u["toBattery"];
  for (int k = 0; k < n; ++k) {
    int yTop = axisY;
    const int bx = x0 + static_cast<int>(k * bw) + 1;
    const int bwi = static_cast<int>(bw) - 2 > 1 ? static_cast<int>(bw) - 2 : 1;
    const struct {
      JsonArrayConst a;
      const char* key;
      int fallback;
    } parts[] = {{grid, "fromGrid", 2}, {batt, "fromBattery", 5}, {solar, "fromSolar", 3}};
    for (const auto& p : parts) {
      const float v = p.a[k] | 0.0f;
      if (v <= 0) continue;
      const int h = static_cast<int>(v * scale);
      yTop -= h;
      g.fillRect(bx, yTop, bwi, h, col(c[p.key], p.fallback));
    }
    // Below the line: into the battery next to it, then out to the grid.
    int yBelow = axisY;
    const struct {
      JsonArrayConst a;
      const char* key;
      int fallback;
    } down[] = {{charge, "toBattery", 5}, {exp_, "gridExport", 4}};
    for (const auto& p : down) {
      const float v = p.a[k] | 0.0f;
      if (v <= 0) continue;
      const int h = static_cast<int>(v * scale);
      g.fillRect(bx, yBelow, bwi, h, col(c[p.key], p.fallback));
      yBelow += h;
    }
  }
  g.drawFastHLine(x0, axisY, W, GxEPD_BLACK);
  return y + BH + 4 - b.y;
}

int alerts(Gfx& g, JsonObjectConst d, Box b) {
  int y = b.y;
  for (JsonObjectConst l : d["lines"].as<JsonArrayConst>()) {
    draw::face(g, draw::BOLD24, col(l["color"]));
    draw::text(g, b.x + 8, y + 22, str(l["text"]));
    y += 30;
  }
  if ((d["overflow"] | 0) > 0) {
    char buf[32];
    snprintf(buf, sizeof(buf), "+ %d more...", d["overflow"].as<int>());
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, b.x + 8, y + 18, buf);
    y += 24;
  }
  if (d["allClear"] | false) {
    draw::face(g, draw::BOLD24, GxEPD_GREEN);
    draw::text(g, b.x + 8, y + 22, str(d["allClearText"], "All clear"));
    y += 30;
  }
  return y - b.y + 4;
}

int heatPump(Gfx& g, JsonObjectConst d, Box b) {
  if (d.isNull()) return 0;
  const int x = b.x + 8, w = b.w - 16;
  g.drawRect(x, b.y + 4, w, 92, GxEPD_BLACK);
  g.drawRect(x + 1, b.y + 5, w - 2, 90, GxEPD_BLACK);
  draw::icon(g, icons::named("heat-pump-outline", 24), x + 10, b.y + 12, GxEPD_BLACK, Mode::Ink);
  draw::face(g, draw::BOLD24, GxEPD_BLACK);
  draw::text(g, x + 42, b.y + 32, str(d["name"]));
  const char* labels[] = {"MODE", "OUTSIDE", "SETPOINT", "COP"};
  char vals[4][24];
  snprintf(vals[0], 24, "%s", *str(d["action"]) ? str(d["action"]) : *str(d["mode"]) ? str(d["mode"]) : "-");
  if (d["outside"].isNull()) snprintf(vals[1], 24, "--");
  else snprintf(vals[1], 24, "%.1f", d["outside"].as<float>());
  if (d["setpoint"].isNull()) snprintf(vals[2], 24, "--");
  else snprintf(vals[2], 24, "%.0f", d["setpoint"].as<float>());
  if (d["cop"].isNull()) snprintf(vals[3], 24, "--");
  else snprintf(vals[3], 24, "%.1f", d["cop"].as<float>());
  const int cw = (w - 20) / 4;
  for (int i = 0; i < 4; ++i) {
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, x + 10 + i * cw, b.y + 58, labels[i]);
    draw::face(g, draw::BOLD24, GxEPD_BLACK);
    draw::text(g, x + 10 + i * cw, b.y + 84, vals[i]);
  }
  return 104;
}

int roomClimate(Gfx& g, JsonObjectConst d, Box b) {
  int y = b.y;
  char buf[48];
  const int x = b.x + 8, w = b.w - 16;
  if ((d["total"] | 0) > 0) {
    draw::face(g, draw::BOLD18, GxEPD_BLACK);
    snprintf(buf, sizeof(buf), "%d of %d rooms calling", d["calling"] | 0, d["total"] | 0);
    draw::text(g, x, y + 18, buf);
    y += 26;
  }
  for (JsonObjectConst r : d["rooms"].as<JsonArrayConst>()) {
    if (y + 30 > draw::PANEL_H - 24) break;
    const uint16_t c = col(r["color"]);
    draw::face(g, draw::BOLD18, GxEPD_BLACK);
    draw::text(g, x, y + 20, str(r["name"]));
    if (r["temperature"].isNull()) snprintf(buf, sizeof(buf), "--");
    else if (r["target"].isNull()) snprintf(buf, sizeof(buf), "%.1f", r["temperature"].as<float>());
    else snprintf(buf, sizeof(buf), "%.1f > %.1f", r["temperature"].as<float>(), r["target"].as<float>());
    draw::face(g, draw::BOLD18, c);
    draw::text(g, x + 140, y + 20, buf);
    // Delta bar: -5..+5 around the middle.
    const int bx = x + 280, bwid = w - 280;
    if (bwid > 40) {
      g.drawRect(bx, y + 6, bwid, 16, GxEPD_BLACK);
      g.drawFastVLine(bx + bwid / 2, y + 6, 16, GxEPD_BLACK);
      if (!r["delta"].isNull()) {
        const float dl = fmaxf(-5.0f, fminf(5.0f, r["delta"].as<float>()));
        const int half = bwid / 2;
        const int len = static_cast<int>(fabsf(dl) / 5.0f * half);
        if (len > 0) g.fillRect(dl < 0 ? bx + half - len : bx + half, y + 9, len, 10, c);
      }
    }
    y += 30;
  }
  return y - b.y + 4;
}

int roomList(Gfx& g, JsonObjectConst d, Box b) {
  int y = b.y;
  const int x = b.x + 8, w = b.w - 16;
  auto row = [&](JsonObjectConst r) {
    if (y + 28 > draw::PANEL_H - 24) return;
    draw::icon(g, icons::named(str(r["icon"], "home-outline"), 20), x, y + 4, GxEPD_BLACK, Mode::Ink);
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, x + 28, y + 19, str(r["name"]));
    char buf[24];
    if (r["temperature"].isNull()) snprintf(buf, sizeof(buf), "--");
    else snprintf(buf, sizeof(buf), "%d\xC2\xB0", static_cast<int>(roundf(r["temperature"].as<float>())));
    draw::face(g, draw::BOLD18, GxEPD_BLACK);
    draw::textRight(g, buf, x + w - 48, y + 19);
    if (!r["humidity"].isNull()) {
      snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(roundf(r["humidity"].as<float>())));
      draw::face(g, draw::REG18, GxEPD_BLUE);
      draw::textRight(g, buf, x + w, y + 19);
    }
    g.drawFastHLine(x, y + 26, w, GxEPD_BLACK);
    y += 30;
  };
  JsonArrayConst rooms = d["rooms"];
  if (!(d["byFloor"] | true)) {
    for (JsonObjectConst r : rooms) row(r);
    return y - b.y;
  }
  for (const char* floor : {"upstairs", "downstairs"}) {
    bool any = false;
    for (JsonObjectConst r : rooms)
      if (!strcmp(str(r["floor"]), floor)) any = true;
    if (!any) continue;
    char h[16];
    upper(h, sizeof(h), floor);
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, x, y + 18, h);
    y += 24;
    for (JsonObjectConst r : rooms)
      if (!strcmp(str(r["floor"]), floor)) row(r);
  }
  return y - b.y;
}

int people(Gfx& g, JsonObjectConst d, Box b) {
  int x = b.x + 8;
  int y = b.y;
  for (JsonObjectConst p : d["people"].as<JsonArrayConst>()) {
    const bool home = p["home"] | false;
    const uint16_t c = home ? col(p["color"], 4) : GxEPD_BLACK;
    draw::face(g, home ? draw::BOLD18 : draw::REG18, c);
    const char* name = str(p["name"]);
    const int w = 26 + draw::bounds(g, name).w + 18;
    if (x + w > b.x + b.w) {
      x = b.x + 8;
      y += 30;
    }
    draw::icon(g, icons::named(home ? "account" : "account-outline", 20), x, y + 6, c, Mode::Ink);
    draw::text(g, x + 24, y + 22, name);
    x += w;
  }
  return y + 32 - b.y;
}

int media(Gfx& g, JsonObjectConst d, Box b) {
  int y = b.y;
  JsonArrayConst players = d["players"];
  if (players.size() == 0) {
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, b.x + 8, y + 20, "Nothing playing");
    return 28;
  }
  for (JsonObjectConst m : players) {
    const char* art = str(m["art"]);
    int tx = b.x + 8;
    if (*art) {
      if (const uint8_t* pic = icons::picture(art, 72, 72)) draw::picture(g, pic, 72, 72, b.x + 8, y + 4);
      else g.drawRect(b.x + 8, y + 4, 72, 72, GxEPD_BLACK);
      tx += 82;
    } else {
      draw::icon(g, icons::named(str(m["icon"], "music"), 24), b.x + 8, y + 6, GxEPD_BLUE, Mode::Ink);
      tx += 32;
    }
    char head[96];
    snprintf(head, sizeof(head), "%s%s%s", str(m["room"]), *str(m["app"]) ? " \xC2\xB7 " : "", str(m["app"]));
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, tx, y + 20, head);
    draw::face(g, draw::BOLD24, GxEPD_BLACK);
    draw::textWrapped(g, str(m["title"]), tx, y + 46, b.x + b.w - tx - 8, 26, 1);
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, tx, y + 68, str(m["artist"]));
    y += *art ? 84 : 76;
  }
  return y - b.y;
}

int transport(Gfx& g, JsonObjectConst d, Box b) {
  int y = b.y;
  char buf[24];
  for (JsonObjectConst t : d["routes"].as<JsonArrayConst>()) {
    const uint16_t c = col(t["color"], 4);
    draw::icon(g, icons::named(str(t["icon"], "bus"), 24), b.x + 8, y + 6, c, Mode::Ink);
    int rx = b.x + b.w - 8;
    JsonArrayConst deps = t["departures"];
    // The route and stop stop short of the departures (76 px each).
    const int nameW = rx - static_cast<int>(deps.size()) * 76 - (b.x + 40) - 4;
    char fitted[96];
    draw::face(g, draw::BOLD18, GxEPD_BLACK);
    draw::text(g, b.x + 40, y + 20, draw::fit(g, fitted, sizeof(fitted), str(t["name"]), nameW));
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, b.x + 40, y + 40, draw::fit(g, fitted, sizeof(fitted), str(t["stop"]), nameW));
    for (int i = static_cast<int>(deps.size()) - 1; i >= 0; --i) {
      JsonObjectConst x = deps[i];
      draw::face(g, draw::REG18, GxEPD_BLACK);
      draw::textRight(g, str(x["time"]), rx, y + 18);
      draw::face(g, draw::BOLD18, col(x["color"], 1));
      snprintf(buf, sizeof(buf), "%s", str(x["text"]));
      draw::textRight(g, buf, rx, y + 40);
      rx -= 76;
    }
    y += 50;
  }
  return y - b.y;
}

int announcements(Gfx& g, JsonObjectConst d, Box b) {
  int y = b.y;
  JsonArrayConst items = d["items"];
  const int w = b.w - 16;
  if (items.size() == 0) {
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, b.x + 8, y + 20, str(d["empty"], "No announcements"));
    return 28;
  }
  for (JsonObjectConst it : items) {
    if (y + 40 > draw::PANEL_H - 24) break;
    draw::face(g, draw::BOLD24, col(d["color"]));
    y += 26 * draw::textWrapped(g, str(it["title"]), b.x + 8, y + 22, w, 26, 2);
    if (*str(it["summary"])) {
      draw::face(g, draw::REG18, GxEPD_BLACK);
      y += 20 * draw::textWrapped(g, str(it["summary"]), b.x + 8, y + 18, w, 20, 3);
    }
    if (*str(it["when"])) {
      draw::face(g, draw::REG18, GxEPD_BLACK);
      draw::text(g, b.x + 8, y + 18, str(it["when"]));
      y += 22;
    }
    g.drawFastHLine(b.x + 8, y + 4, w, GxEPD_BLACK);
    y += 10;
  }
  return y - b.y;
}

// =============================================================================
// A section: its type's drawing, in its box; returns the height it used.
// =============================================================================

int section(Gfx& g, JsonObjectConst s, Box b, JsonArrayConst siblings) {
  const char* type = str(s["type"]);
  JsonObjectConst d = s["data"];
  // The panel's own sections word or place their own headings.
  if (!strcmp(type, "weather")) return weather(g, d, b);
  if (!strcmp(type, "energy") && strcmp(str(d["style"]), "list")) return energy(g, d, b);
  if (!strcmp(type, "battery")) return battery(g, d, b);
  if (!strcmp(type, "statusIcons")) return statusIcons(g, d, b);
  if (!strcmp(type, "now")) return now(g, s, d, b);
  if (!strcmp(type, "calendar")) return calendar(g, s, d, b);
  if (!strcmp(type, "heating")) return heating(g, d, b);
  if (!strcmp(type, "alarm")) return alarm(g, d, b, siblings);
  if (!strcmp(type, "openings")) return openings(g, s, d, b);
  if (!strcmp(type, "motion")) return motion(g, s, d, b);
  if (!strcmp(type, "cameras")) return cameras(g, s, d, b);
  // The rest: an optional label, then the content.
  const int lh = label(g, s, b);
  Box c{b.x, b.y + lh, b.w};
  int h = 0;
  if (!strcmp(type, "energy")) h = energyList(g, d, c);
  else if (!strcmp(type, "energyGraph")) h = energyGraph(g, d, c);
  else if (!strcmp(type, "alerts")) h = alerts(g, d, c);
  else if (!strcmp(type, "heatPump")) h = heatPump(g, d, c);
  else if (!strcmp(type, "roomClimate")) h = roomClimate(g, d, c);
  else if (!strcmp(type, "roomList")) h = roomList(g, d, c);
  else if (!strcmp(type, "people")) h = people(g, d, c);
  else if (!strcmp(type, "media")) h = media(g, d, c);
  else if (!strcmp(type, "transport")) h = transport(g, d, c);
  else if (!strcmp(type, "announcements")) h = announcements(g, d, c);
  return lh + h + 6;
}

// =============================================================================
// Screens
// =============================================================================

// The template's columns: the panel's sidebar (a 250 px column, a dotted
// divider, the main column from RC_X = 260), its security page's three
// (0 / 248 / 534 with rules between), two halves, or the whole panel.
int columnsOf(const char* tpl, Box* out) {
  if (!strcmp(tpl, "sidebar")) {
    out[0] = {0, 0, 250};
    out[1] = {260, 0, 540};
    return 2;
  }
  if (!strcmp(tpl, "triple")) {
    out[0] = {0, 0, 240};
    out[1] = {248, 0, 278};
    out[2] = {534, 0, 266};
    return 3;
  }
  if (!strcmp(tpl, "columns")) {
    out[0] = {0, 0, 400};
    out[1] = {410, 0, 390};
    return 2;
  }
  out[0] = {0, 0, 800};
  return 1;
}

void sectionsScreen(Gfx& g, JsonObjectConst d) {
  const char* tpl = str(d["template"], "single");
  Box cols[3];
  const int n = columnsOf(tpl, cols);
  // Every section on the screen, for the alarm's summary.
  JsonDocument all;
  JsonArray flat = all.to<JsonArray>();
  for (JsonArrayConst column : d["columns"].as<JsonArrayConst>())
    for (JsonObjectConst s : column) flat.add(s);
  const JsonArrayConst siblings = flat;

  if (!strcmp(tpl, "sidebar")) draw::dottedV(g, 250, 0, draw::PANEL_H);
  if (!strcmp(tpl, "columns")) draw::dottedV(g, 404, 0, draw::PANEL_H);
  if (!strcmp(tpl, "triple")) {
    g.drawFastVLine(244, 0, draw::PANEL_H, GxEPD_BLACK);
    g.drawFastVLine(530, 0, draw::PANEL_H, GxEPD_BLACK);
  }
  int ci = 0;
  for (JsonArrayConst column : d["columns"].as<JsonArrayConst>()) {
    if (ci >= n) break;
    Box b = cols[ci];
    // A single column's sections sit 10 px in (the heating page fills it).
    if (n == 1) {
      b.x = 10;
      b.w = 780;
    }
    for (JsonObjectConst s : column) {
      if (b.y >= draw::PANEL_H) break;
      Box sb = b;
      if (n == 1 && !strcmp(str(s["type"]), "heating")) sb = {0, b.y, 800};
      b.y += section(g, s, sb, siblings);
    }
    ++ci;
  }
}

// --- Meeting room and room finder, as the server's preview draws them -------

void meetingRoom(Gfx& g, JsonObjectConst d) {
  const uint16_t band = col(d["color"], 4);
  g.fillRect(0, 0, draw::PANEL_W, 160, band);
  draw::icon(g, icons::named(str(d["icon"], "door-open"), 84), 28, 38, GxEPD_WHITE, Mode::Mask);
  draw::face(g, draw::BOLD24, GxEPD_WHITE);
  draw::text(g, 136, 46, str(d["name"]));
  draw::face(g, draw::BOLD24, GxEPD_WHITE, 2);
  draw::text(g, 136, 100, str(d["label"]));
  draw::face(g, draw::BOLD24, GxEPD_WHITE);
  draw::text(g, 136, 136, str(d["until"]));
  int y = 172;

  JsonObjectConst tl = d["timeline"];
  if (!tl.isNull()) {
    const int x0 = 28, w = 744;
    g.drawRect(x0, y, w, 34, GxEPD_BLACK);
    g.drawRect(x0 + 1, y + 1, w - 2, 32, GxEPD_BLACK);
    for (JsonObjectConst bl : tl["blocks"].as<JsonArrayConst>()) {
      const int bx = x0 + static_cast<int>((bl["from"] | 0.0f) * w);
      const int bw = static_cast<int>(((bl["to"] | 0.0f) - (bl["from"] | 0.0f)) * w);
      g.fillRect(bx, y, bw, 34, col(bl["color"], 2));
      g.drawFastVLine(bx + bw - 1, y, 34, GxEPD_WHITE);
      draw::face(g, draw::BOLD18, GxEPD_WHITE);
      draw::textWrapped(g, str(bl["title"]), bx + 6, y + 23, bw - 10 > 10 ? bw - 10 : 10, 20, 1);
    }
    draw::face(g, draw::BOLD18, GxEPD_BLACK);
    JsonArrayConst ticks = tl["ticks"];
    int i = 0;
    for (JsonObjectConst k : ticks) {
      const int tx = x0 + static_cast<int>((k["at"] | 0.0f) * w);
      const char* l = str(k["label"]);
      const int lw = draw::bounds(g, l).w;
      const int px = i == 0 ? tx : (i == static_cast<int>(ticks.size()) - 1 ? tx - lw : tx - lw / 2);
      draw::text(g, px, y + 54, l);
      ++i;
    }
    y += 64;
  }

  const int bodyW = d["climate"].isNull() ? 744 : 540;
  JsonObjectConst cur = d["current"];
  JsonObjectConst next = d["next"];
  char buf[96];
  if (!cur.isNull()) {
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, 28, y + 18, "NOW");
    draw::face(g, draw::BOLD24, GxEPD_BLACK);
    draw::textWrapped(g, str(cur["title"]), 28, y + 46, bodyW, 28, 1);
    snprintf(buf, sizeof(buf), "%s \xC2\xB7 ends in %d min", str(cur["time"]), cur["endsInMin"] | 0);
    draw::face(g, draw::BOLD18, GxEPD_BLACK);
    draw::text(g, 28, y + 70, buf);
    y += 84;
  } else if (!next.isNull()) {
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, 28, y + 18, "NEXT");
    draw::face(g, draw::BOLD24, GxEPD_BLACK);
    draw::textWrapped(g, str(next["title"]), 28, y + 46, bodyW, 28, 1);
    snprintf(buf, sizeof(buf), "%s %s", str(next["day"]), str(next["time"]));
    draw::face(g, draw::BOLD18, GxEPD_BLACK);
    draw::text(g, 28, y + 70, buf);
    y += 84;
  }
  JsonArrayConst upcoming = d["upcoming"];
  if (upcoming.size()) {
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::text(g, 28, y + 18, "LATER TODAY");
    y += 24;
    for (JsonObjectConst u : upcoming) {
      if (y + 28 > draw::PANEL_H) break;
      draw::face(g, draw::BOLD18, GxEPD_BLACK);
      draw::text(g, 28, y + 20, str(u["time"]));
      draw::textWrapped(g, str(u["title"]), 168, y + 20, bodyW - 140, 22, 1);
      y += 28;
    }
  }

  JsonObjectConst cl = d["climate"];
  if (!cl.isNull()) {
    int cy = draw::PANEL_H - 18;
    if (!cl["co2"].isNull()) {
      snprintf(buf, sizeof(buf), "%d ppm", cl["co2"].as<int>());
      const uint16_t c = col(cl["co2Color"]);
      draw::face(g, draw::BOLD18, c);
      draw::textRight(g, buf, draw::PANEL_W - 28, cy);
      draw::icon(g, icons::named("molecule-co2", 24), draw::PANEL_W - 28 - draw::bounds(g, buf).w - 30, cy - 19, c, Mode::Ink);
      cy -= 32;
    }
    if (!cl["humidity"].isNull()) {
      snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(roundf(cl["humidity"].as<float>())));
      draw::face(g, draw::BOLD18, GxEPD_BLACK);
      draw::textRight(g, buf, draw::PANEL_W - 28, cy);
      draw::icon(g, icons::named("water-percent", 24), draw::PANEL_W - 28 - draw::bounds(g, buf).w - 30, cy - 19, GxEPD_BLACK, Mode::Ink);
      cy -= 32;
    }
    if (!cl["temperature"].isNull()) {
      snprintf(buf, sizeof(buf), "%.1f%s", cl["temperature"].as<float>(), str(cl["unit"]));
      draw::face(g, draw::BOLD24, GxEPD_BLACK);
      draw::textRight(g, buf, draw::PANEL_W - 28, cy);
      draw::icon(g, icons::named("thermometer", 24), draw::PANEL_W - 28 - draw::bounds(g, buf).w - 30, cy - 21, GxEPD_BLACK, Mode::Ink);
    }
  }
}

void roomFinder(Gfx& g, JsonObjectConst d, const char* title) {
  draw::face(g, draw::BOLD24, GxEPD_BLACK);
  draw::text(g, 28, 46, title);
  draw::face(g, draw::BOLD18, (d["available"] | false) ? GxEPD_GREEN : GxEPD_RED);
  draw::textRight(g, str(d["summary"]), draw::PANEL_W - 28, 46);
  g.fillRect(28, 58, draw::PANEL_W - 56, 3, GxEPD_BLACK);
  int y = 70;
  JsonArrayConst rooms = d["rooms"];
  if (rooms.size() == 0) {
    draw::face(g, draw::BOLD18, GxEPD_BLACK);
    draw::text(g, 28, y + 24, (d["total"] | 0) ? "No rooms free right now" : "Add rooms to this screen");
    return;
  }
  for (JsonObjectConst r : rooms) {
    if (y + 56 > draw::PANEL_H) break;
    const uint16_t c = col(r["color"], 4);
    g.fillRect(28, y + 6, 44, 44, c);
    draw::icon(g, icons::named(str(r["icon"], "door-open"), 30), 35, y + 13, GxEPD_WHITE, Mode::Mask);
    draw::face(g, draw::BOLD24, GxEPD_BLACK);
    draw::textWrapped(g, str(r["name"]), 88, y + 36, 440, 26, 1);
    draw::face(g, draw::BOLD18, c);
    draw::textRight(g, str(r["label"]), draw::PANEL_W - 28, y + 24);
    draw::face(g, draw::REG18, GxEPD_BLACK);
    draw::textRight(g, str(r["until"]), draw::PANEL_W - 28, y + 46);
    g.drawFastHLine(28, y + 56, draw::PANEL_W - 56, GxEPD_BLACK);
    y += 60;
  }
}

}  // namespace

// The panel's footer: [refresh] time [battery icon] percentage, ending 12 px
// from the right edge along y = 468; the bed-and-clock before it in quiet
// hours.
void footer(Gfx& g, const Ctx& ctx) {
  const int FOOTER_Y = 468, FOOTER_ICON_Y = FOOTER_Y - 18, FOOTER_TINY_Y = FOOTER_Y - 13;
  const int FOOTER_MARGIN_R = 12, FOOTER_GAP = 10;
  const int pct = ctx.battPct;
  const uint16_t batCol = pct < 0 ? GxEPD_BLACK : pct > BATT_GREEN_PCT ? GxEPD_GREEN : pct > BATT_RED_PCT ? GxEPD_BLACK : GxEPD_RED;
  char pctStr[16];
  if (pct < 0) snprintf(pctStr, sizeof(pctStr), "--%%");
  else snprintf(pctStr, sizeof(pctStr), "%d%%", pct);
  draw::face(g, draw::REG18, batCol);
  const draw::Bounds pb = draw::bounds(g, pctStr);
  const int pctX = draw::PANEL_W - FOOTER_MARGIN_R - pb.w;
  const int iconX = pctX - 24 - 2;
  draw::icon(g, icons::slot("vf_battery"), iconX, FOOTER_ICON_Y, batCol, Mode::Opaque);
  draw::text(g, pctX, FOOTER_Y, pctStr);

  draw::face(g, draw::REG18, GxEPD_BLACK);
  const draw::Bounds tb = draw::bounds(g, ctx.time);
  const int timeX = iconX - FOOTER_GAP - tb.w;
  draw::text(g, timeX, FOOTER_Y, ctx.time);
  const int refX = timeX - 16 - 4;
  draw::icon(g, icons::slot("vf_refresh"), refX, FOOTER_TINY_Y, GxEPD_BLACK, Mode::Opaque);
  int x = refX;
  if (ctx.quiet) {
    x -= 16 + 4;
    draw::icon(g, icons::slot("vf_quiet"), x, FOOTER_TINY_Y, GxEPD_BLUE, Mode::Opaque);
  }
  // The carousel: every screen's icon, the one showing underlined.
  if (ctx.markCount > 1) {
    x -= 8;
    const int n = ctx.markCount;
    const int step = 16 + 6;
    const int x0 = x - n * step + 6;
    for (int i = 0; i < n; ++i) {
      const int ix = x0 + i * step;
      draw::icon(g, icons::named(ctx.marks[i] && *ctx.marks[i] ? ctx.marks[i] : "view-dashboard-outline", 16), ix, FOOTER_TINY_Y, GxEPD_BLACK, Mode::Opaque);
      if (i == ctx.current) g.fillRect(ix, FOOTER_TINY_Y + 18, 16, 2, GxEPD_BLACK);
    }
  }
}

void drawScreen(Gfx& g, JsonObjectConst screen, const Ctx& ctx) {
  g.fillScreen(GxEPD_WHITE);
  const char* kind = str(screen["kind"], "sections");
  if (!strcmp(kind, "meetingRoom")) {
    meetingRoom(g, screen["data"]);
    return;
  }
  if (!strcmp(kind, "roomFinder")) {
    roomFinder(g, screen["data"], str(screen["title"], "Other rooms"));
    return;
  }
  sectionsScreen(g, screen);
  footer(g, ctx);
}

}  // namespace screens
