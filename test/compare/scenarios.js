#!/usr/bin/env node
'use strict';
/**
 * The pixel comparison (compare.sh) on many houses, not just the captured
 * one: each scenario changes house.json (an alarm going off, a sensor gone,
 * a frost, no events, midnight...), Switchboard Server computes the screens
 * for it, and the panel's own firmware and this one draw it. Any differing
 * pixel fails.
 *
 *   SERVER_DIR=../Switchboard-Server node test/compare/scenarios.js [name...]
 *
 * Needs the comparison built (compare.sh builds test/host/build/kd-compare).
 */
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const ROOT = path.join(__dirname, '..', '..');
const SERVER = path.resolve(process.env.SERVER_DIR || path.join(ROOT, '..', 'Switchboard-Server'));
const dashboard = require(path.join(SERVER, 'lib', 'dashboard'));
const { buildScreens, refreshPlan } = require(path.join(SERVER, 'lib', 'dashboard-state'));
const BASE = JSON.parse(fs.readFileSync(path.join(__dirname, 'house.json'), 'utf8'));

const ago = (h, min = 0) => (now) => new Date(Date.parse(now) - (h * 60 + min) * 60000).toISOString();

// Helpers on a house: set a state (and attributes), drop an entity.
function tools(house) {
  const find = (id) => house.states.find((s) => s.entity_id === id);
  return {
    // (Every copy: the demo house lists a few entities twice, and the panel
    // reads the first where the server reads the last.)
    set(id, state, attrs, changed) {
      if (!find(id)) house.states.push({ entity_id: id, state, attributes: {}, last_changed: house.now, last_updated: house.now });
      for (const s of house.states.filter((x) => x.entity_id === id)) {
        if (state !== undefined) s.state = state;
        if (attrs) s.attributes = { ...s.attributes, ...attrs };
        if (changed) s.last_changed = s.last_updated = typeof changed === 'function' ? changed(house.now) : changed;
      }
    },
    drop(id) {
      house.states = house.states.filter((s) => s.entity_id !== id);
    },
    zones: ['kitchen', 'living_room', 'hall', 'bathroom', 'landing', 'bedroom', 'bedroom_2', 'office'].map((z) => `climate.${z}`)
  };
}

const SCENARIOS = {
  base: () => {},
  // --- Weather -------------------------------------------------------------------
  frost: (h, t) => t.set('weather.home', 'snowy', { temperature: -3.6, wind_bearing: 10, uv_index: 0, humidity: 93.5 }),
  heatwave: (h, t) => t.set('weather.home', 'sunny', { temperature: 27.5, wind_speed: 3.4, wind_bearing: 359, uv_index: 8.4 }),
  'zero-degrees': (h, t) => t.set('weather.home', 'fog', { temperature: -0.4 }),
  'twenty-boundary': (h, t) => t.set('weather.home', 'cloudy', { temperature: 20.5 }),
  'no-bearing': (h, t) => t.set('weather.home', 'windy', { wind_bearing: null, uv_index: null, humidity: null }),
  'weather-unavailable': (h, t) => t.set('weather.home', 'unavailable', { temperature: null }),
  'weather-gone': (h, t) => t.drop('weather.home'),
  'no-forecast': (h) => {
    h.forecasts.daily['weather.home'] = [];
    h.forecasts.hourly['weather.home'] = [];
  },
  'odd-conditions': (h, t) => {
    t.set('weather.home', 'lightning-rainy');
    h.forecasts.daily['weather.home'].forEach((f, i) => (f.condition = ['hail', 'exceptional', 'snowy-rainy', 'windy-variant', 'clear-night'][i % 5]));
  },
  'rain-now': (h) => h.forecasts.hourly['weather.home'].forEach((f) => (f.precipitation = 1.5)),
  'dry-all-day': (h) => h.forecasts.hourly['weather.home'].forEach((f) => (f.precipitation = 0)),
  'drizzle-threshold': (h) => h.forecasts.hourly['weather.home'].forEach((f, i) => (f.precipitation = i === 3 ? 0.1 : 0.04)),
  'solar-forecast-gone': (h, t) => {
    h.forecasts.hourly['weather.home'].forEach((f) => (f.precipitation = 0));
    t.drop('sensor.solar_forecast_today');
  },
  // --- Energy and the battery ------------------------------------------------------
  'energy-zero': (h, t) => ['sensor.solar_generation', 'sensor.load_today', 'sensor.grid_import', 'sensor.grid_export'].forEach((id) => t.set(id, '0')),
  'energy-boundary': (h, t) => {
    t.set('sensor.solar_generation', '0.99');
    t.set('sensor.load_today', '1.0');
    t.set('sensor.grid_import', '123.456');
    t.set('sensor.grid_export', '0.05');
  },
  'energy-unavailable': (h, t) => {
    t.set('sensor.solar_generation', 'unavailable');
    t.set('sensor.load_today', 'unknown');
    t.drop('sensor.grid_import');
  },
  'battery-discharging': (h, t) => {
    t.set('sensor.battery_power', '850');
    t.set('sensor.battery_soc', '43');
    t.set('sensor.battery_charge_eta', 'unknown');
    t.set('sensor.battery_discharge_eta', new Date(Date.parse(h.now) + 7.5 * 3600000).toISOString());
  },
  'battery-idle': (h, t) => {
    t.set('sensor.battery_power', '12');
    t.set('sensor.battery_soc', '100');
  },
  'battery-low': (h, t) => {
    t.set('sensor.battery_power', '400');
    t.set('sensor.battery_soc', '8');
  },
  'power-at-idle-edge': (h, t) => t.set('sensor.battery_power', '-100'),
  'power-just-charging': (h, t) => {
    t.set('sensor.battery_power', '-101');
    t.set('sensor.battery_charge_eta', new Date(Date.parse(h.now) + 95 * 60000).toISOString());
  },
  'power-just-discharging': (h, t) => {
    t.set('sensor.battery_power', '101');
    t.set('sensor.battery_discharge_eta', 'unknown');
  },
  'discharge-eta-offset': (h, t) => {
    t.set('sensor.battery_power', '2400');
    t.set('sensor.battery_soc', '100');
    t.set('sensor.battery_discharge_eta', '2026-10-04T03:05:00+00:00');
  },
  'battery-eta-tomorrow': (h, t) => t.set('sensor.battery_charge_eta', new Date(Date.parse(h.now) + 20 * 3600000).toISOString()),
  'battery-unavailable': (h, t) => {
    t.set('sensor.battery_soc', 'unavailable');
    t.set('sensor.battery_power', 'unavailable');
  },
  // --- Now / alarm / heating -------------------------------------------------------
  'alarm-armed': (h, t) => {
    t.set('sensor.home_alarm_state', 'armed_away', null, ago(2, 15));
    t.set('alarm_control_panel.home_alarm', 'armed_away', null, ago(2, 15));
    t.set('sensor.home_alarm_event', 'Armed away by Sam');
  },
  'alarm-triggered': (h, t) => {
    t.set('sensor.home_alarm_state', 'triggered', null, ago(0, 4));
    t.set('alarm_control_panel.home_alarm', 'triggered', null, ago(0, 4));
    t.set('sensor.home_alarm_event', 'Alarm triggered: Back door');
  },
  'alarm-arming': (h, t) => {
    t.set('sensor.home_alarm_state', 'arming', null, ago(0, 1));
    t.set('alarm_control_panel.home_alarm', 'arming', null, ago(0, 1));
  },
  'alarm-night': (h, t) => {
    t.set('sensor.home_alarm_state', 'armed_night', null, ago(30));
    t.set('alarm_control_panel.home_alarm', 'armed_night', null, ago(30));
  },
  'alarm-unavailable': (h, t) => {
    t.set('sensor.home_alarm_state', 'unavailable');
    t.set('alarm_control_panel.home_alarm', 'unavailable');
    t.set('sensor.home_alarm_event', 'unknown');
  },
  'all-quiet': (h, t) => {
    t.set('climate.whole_house', 'off');
    t.set('water_heater.home_tank', 'off', { operation_mode: 'off' });
    t.set('binary_sensor.front_door', 'off');
    t.set('binary_sensor.window_living_room', 'off');
    t.set('sensor.plant_soil_moisture', 'Moist');
    t.set('vacuum.vacuum1', 'docked');
    t.zones.forEach((z) => t.set(z, 'heat', { hvac_action: 'idle', temperature: 15 }));
  },
  'everything-open': (h, t) => {
    h.states.filter((s) => /^binary_sensor\.(window_|front_door$|back_door$)/.test(s.entity_id)).forEach((s) => t.set(s.entity_id, 'on', null, ago(0, 30)));
    t.set('vacuum.vacuum2', 'cleaning');
    t.set('lawn_mower.mower', 'mowing');
  },
  'long-texts': (h, t) => {
    t.set('sensor.home_alarm_event', 'Disarmed by somebody with a remarkably long name at the side gate keypad');
    t.set('sensor.plant_soil_moisture', 'Almost Dry', { friendly_name: 'The very large fiddle-leaf fig in the conservatory' });
  },
  'robots-returning': (h, t) => {
    t.set('vacuum.vacuum1', 'returning');
    t.set('vacuum.vacuum2', 'error');
    t.set('lawn_mower.mower', 'error');
  },
  // --- Heating -----------------------------------------------------------------------
  'heating-off': (h, t) => {
    t.set('climate.whole_house', 'off');
    t.zones.forEach((z) => t.set(z, 'off'));
  },
  'all-calling': (h, t) => t.zones.forEach((z, i) => t.set(z, 'heat', { current_temperature: 15 + i * 0.3, temperature: 21 })),
  'zones-out-of-range': (h, t) => {
    t.set('climate.kitchen', 'heat', { current_temperature: 31.2, temperature: 22 });
    t.set('climate.hall', 'heat', { current_temperature: 9.5, temperature: 26 });
    t.set('climate.office', 'heat', { current_temperature: null, temperature: null });
    t.set('climate.landing', 'unavailable');
    t.drop('climate.bedroom_2');
  },
  'hot-water-off': (h, t) => t.set('water_heater.home_tank', 'off', { operation_mode: 'off', current_temperature: 38.2 }),
  'hot-water-gone': (h, t) => t.drop('water_heater.home_tank'),
  // --- Security times ----------------------------------------------------------------
  'yesterday': (h) => h.states.forEach((s) => {
    if (/^binary_sensor\./.test(s.entity_id)) s.last_changed = s.last_updated = new Date(Date.parse(h.now) - 30 * 3600000).toISOString();
  }),
  'sensors-unavailable': (h) => h.states.forEach((s) => {
    if (/^binary_sensor\.(.*motion|window_side|back_door)/.test(s.entity_id)) s.state = 'unavailable';
  }),
  // --- Calendars -----------------------------------------------------------------------
  'no-events': (h) => Object.keys(h.calendars).forEach((c) => (h.calendars[c] = [])),
  'many-events': (h) => {
    const day = h.now.slice(0, 10);
    h.calendars['calendar.work'] = Array.from({ length: 9 }, (_, i) => ({
      start: { dateTime: `${day}T${String(8 + i).padStart(2, '0')}:00:00+01:00` },
      end: { dateTime: `${day}T${String(9 + i).padStart(2, '0')}:00:00+01:00` },
      summary: `Meeting number ${i + 1} about something quite important indeed`,
      description: i % 2 ? 'Agenda: budgets, the roadmap, and whatever else turns up on the day' : ''
    }));
  },
  'all-day-only': (h) => {
    const day = h.now.slice(0, 10);
    const next = new Date(Date.parse(day) + 86400000).toISOString().slice(0, 10);
    Object.keys(h.calendars).forEach((c) => (h.calendars[c] = []));
    h.calendars['calendar.holidays'] = [{ start: { date: day }, end: { date: next }, summary: 'Bank holiday' }];
    h.calendars['calendar.birthdays'] = [{ start: { date: day }, end: { date: next }, summary: "Gran's birthday" }];
  },
  'multi-day-event': (h) => {
    const d = (n) => new Date(Date.parse(h.now) + n * 86400000).toISOString().slice(0, 10);
    h.calendars['calendar.home_schedule'] = [
      { start: { date: d(-2) }, end: { date: d(3) }, summary: 'Half term' },
      { start: { dateTime: `${d(-1)}T20:00:00+01:00` }, end: { dateTime: `${d(0)}T18:00:00+01:00` }, summary: 'Overnight shift' }
    ];
  },
  // --- Numbers as the panel wrote them ---------------------------------------------
  'rounding-halves': (h, t) => {
    t.set('weather.home', 'cloudy', { temperature: -2.5, humidity: 5, wind_speed: 3.4, uv_index: 0.25 });
    h.forecasts.daily['weather.home'].forEach((f, i) => (f.temperature = [12.46, -2.46, 0.5, -0.5, 7.5][i % 5]));
  },
  'minus-half': (h, t) => t.set('weather.home', 'snowy', { temperature: -0.5 }),
  'setpoint-halves': (h, t) => {
    t.set('climate.kitchen', 'heat', { current_temperature: 19.45, temperature: 20.5 });
    t.set('climate.hall', 'heat', { current_temperature: 4.96, temperature: 5 });
    t.set('climate.bathroom', 'heat', { current_temperature: 25.03, temperature: 21.5 });
    t.set('climate.office', 'heat', { current_temperature: 14.97, temperature: 18.5 });
  },
  'soc-fraction': (h, t) => t.set('sensor.battery_soc', '85.6'),
  'solar-forecast-unavailable': (h, t) => {
    h.forecasts.hourly['weather.home'].forEach((f) => (f.precipitation = 0));
    t.set('sensor.solar_forecast_today', 'unavailable');
  },
  'solar-forecast-rounding': (h, t) => {
    h.forecasts.hourly['weather.home'].forEach((f) => (f.precipitation = 0));
    t.set('sensor.solar_forecast_today', '8.25');
  },
  'robot-battery-unavailable': (h, t) => {
    t.set('sensor.vacuum1_battery', 'unavailable');
    t.set('sensor.vacuum2_battery', 'unavailable');
  },
  'hot-water-no-temps': (h, t) => t.set('water_heater.home_tank', 'eco', { operation_mode: 'heating', current_temperature: null, temperature: 60 }),
  'calendar-ties-and-overnight': (h) => {
    const d = (n) => new Date(Date.parse(h.now) + n * 86400000).toISOString().slice(0, 10);
    Object.keys(h.calendars).forEach((c) => (h.calendars[c] = []));
    h.calendars['calendar.work'] = [{ start: { dateTime: `${d(0)}T09:00:00+01:00` }, end: { dateTime: `${d(0)}T10:00:00+01:00` }, summary: 'Alpha standup' }];
    h.calendars['calendar.home_schedule'] = [
      { start: { dateTime: `${d(0)}T09:00:00+01:00` }, end: { dateTime: `${d(0)}T09:30:00+01:00` }, summary: 'Zebra feeding' },
      { start: { dateTime: `${d(-1)}T20:00:00+01:00` }, end: { dateTime: `${d(0)}T18:00:00+01:00` }, summary: 'Overnight shift' }
    ];
    h.calendars['calendar.holidays_in_ireland'] = [{ start: { date: d(0) }, end: { date: d(1) }, summary: 'Aardvark day' }];
    h.calendars['calendar.birthdays'] = [{ start: { date: d(0) }, end: { date: d(1) }, summary: "Zoe's birthday" }];
  },
  'description-crlf': (h) => {
    const d = h.now.slice(0, 10);
    h.calendars['calendar.work'] = [{ start: { dateTime: `${d}T08:00:00+01:00` }, end: { dateTime: `${d}T09:00:00+01:00` }, summary: 'Review', description: 'One two\r\nthree four five six seven' }];
  },
  // --- Times of day --------------------------------------------------------------------
  'late-evening': (h) => (h.now = h.now.slice(0, 10) + 'T22:40:00.000Z'),
  'small-hours': (h, t) => {
    h.now = h.now.slice(0, 10) + 'T01:20:00.000Z';
    t.set('weather.home', 'partlycloudy');
  },
  'clear-night': (h, t) => {
    h.now = h.now.slice(0, 10) + 'T21:10:00.000Z';
    t.set('weather.home', 'clear-night');
  },
  // (Home Assistant's daily forecast starts with the new day after midnight.)
  'just-after-midnight': (h) => {
    h.now = h.now.slice(0, 10) + 'T23:05:00.000Z';
    h.forecasts.daily['weather.home'] = h.forecasts.daily['weather.home'].slice(1);
  }
};

// Where this firmware means to differ from the panel's: what the panel drew
// there was wrong or misleading. Anything else that differs fails.
const NIGHT = { heating: 'the quiet-hours bed icon is in every screen\'s footer (the panel drew it on Status only)', security: 'same' };
const KNOWN = {
  'weather-gone': { status: '"--" for a weather entity that isn\'t there (the panel left them blank)' },
  'energy-unavailable': { status: '"n/a" (the panel printed "unavailable kWh")' },
  'energy-zero': { status: '"0.0 kWh", every total to one decimal place (the panel printed Home Assistant\'s "0 kWh")' },
  'energy-boundary': { status: '"1.0 kWh", "123.5 kWh", "0.1 kWh": to one decimal place (the panel printed "0.99", "123.456", "0.05")' },
  'battery-unavailable': { status: '"n/a" (the panel printed "unavailable %")' },
  'alarm-triggered': { status: '"Alarm triggered" in red (the panel had no case for it: a black "Alarm" with a tick)' },
  'alarm-unavailable': { status: 'no second line (the panel printed Home Assistant\'s "unknown")' },
  'hot-water-gone': { heating: 'no hot water temperatures (the panel printed "0C set 0C")' },
  'hot-water-no-temps': { status: '"--C" (the panel printed "0C")', heating: 'same' },
  'solar-forecast-unavailable': { status: 'no solar line (the panel printed "Expecting 0.0kWh today")' },
  'robot-battery-unavailable': { status: '"--% battery", and a docked vacuum isn\'t "charging" (the panel read the battery as 0%)' },
  'late-evening': NIGHT,
  'small-hours': NIGHT,
  'just-after-midnight': NIGHT
};

const want = process.argv.slice(2);
const names = want.length ? want : Object.keys(SCENARIOS);
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'kd-scenarios-'));
const outRoot = path.join(__dirname, 'out', 'scenarios');
fs.mkdirSync(outRoot, { recursive: true });
let failed = 0;
for (const name of names) {
  const house = JSON.parse(JSON.stringify(BASE));
  SCENARIOS[name](house, tools(house));
  const dir = path.join(tmp, name);
  fs.mkdirSync(dir);
  fs.writeFileSync(path.join(dir, 'house.json'), JSON.stringify(house));
  const states = Object.fromEntries(house.states.map((s) => [s.entity_id, s]));
  const layout = dashboard.defaultLayout();
  const screens = buildScreens(layout, { states, forecasts: house.forecasts, calendars: house.calendars, now: new Date(house.now), timeZone: house.timeZone });
  for (const s of layout.screens) fs.writeFileSync(path.join(dir, `server-${s.id}.json`), JSON.stringify({ quiet: refreshPlan(layout, new Date(house.now), house.timeZone).quiet, data: screens[s.id] }));
  const out = path.join(outRoot, name);
  fs.mkdirSync(out, { recursive: true });
  let text;
  try {
    text = execFileSync(path.join(ROOT, 'test', 'host', 'build', 'kd-compare'), [out], {
      cwd: ROOT,
      env: { ...process.env, HOUSE: path.join(dir, 'house.json'), STATE_DIR: dir, SCREENS_ONLY: '1' },
      encoding: 'utf8'
    });
  } catch (e) {
    text = e.stdout || String(e);
  }
  const known = KNOWN[name] || {};
  const diffs = text.trim().split('\n').filter((l) => !/\b0 pixels/.test(l));
  const unexpected = diffs.filter((l) => !known[l.split(/\s+/)[0]]);
  const expected = diffs.filter((l) => known[l.split(/\s+/)[0]]);
  if (unexpected.length) failed++;
  const tag = unexpected.length ? 'DIFF' : expected.length ? 'meant' : 'ok  ';
  console.log(`${tag} ${name}`);
  for (const l of unexpected) console.log(`     ${l}`);
  for (const l of expected) console.log(`     ${l.split(/\s+/)[0]}: ${known[l.split(/\s+/)[0]]}`);
}
console.log(`${names.length - failed}/${names.length} scenarios as intended`);
process.exit(failed ? 1 : 0);
