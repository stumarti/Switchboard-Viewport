#!/usr/bin/env node
'use strict';
/**
 * Captures one moment of a Home Assistant (the server demo's pretend one by
 * default) as house.json: every state, the weather forecasts and today's
 * calendar events the kitchen dashboard reads. Both sides of the comparison
 * (compare.sh) are fed from this one file.
 *
 *   node test/compare/capture.js [http://127.0.0.1:48123] [token]
 */
const fs = require('fs');
const path = require('path');
const HA = process.argv[2] || 'http://127.0.0.1:48123';
const TOKEN = process.argv[3] || 'demo-token';
const H = { authorization: `Bearer ${TOKEN}`, 'content-type': 'application/json' };
const get = (p) => fetch(HA + p, { headers: H }).then((r) => r.json());
const forecast = (entity, type) =>
  fetch(`${HA}/api/services/weather/get_forecasts?return_response`, { method: 'POST', headers: H, body: JSON.stringify({ entity_id: entity, type }) })
    .then((r) => r.json())
    .then((j) => j.service_response[entity].forecast);

// Home Assistant writes timed events in local time with an offset
// ("2026-09-28T19:30:00+01:00"); the demo writes UTC. Rewrite them the way
// HA would, so a firmware that reads the wall-clock digits gets them right.
const TZ = 'Europe/London';
function local(iso) {
  const d = new Date(iso);
  const p = Object.fromEntries(new Intl.DateTimeFormat('en-GB', { timeZone: TZ, hourCycle: 'h23', year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit' })
    .formatToParts(d).map((x) => [x.type, x.value]));
  const wall = `${p.year}-${p.month}-${p.day}T${p.hour}:${p.minute}:${p.second}`;
  const off = Math.round((Date.parse(wall + 'Z') - d.getTime()) / 60000);
  const a = Math.abs(off);
  return `${wall}${off < 0 ? '-' : '+'}${String(Math.floor(a / 60)).padStart(2, '0')}:${String(a % 60).padStart(2, '0')}`;
}
const localise = (events) => events.map((e) => ({
  ...e,
  start: e.start.dateTime ? { dateTime: local(e.start.dateTime) } : e.start,
  end: e.end.dateTime ? { dateTime: local(e.end.dateTime) } : e.end
}));

(async () => {
  const now = new Date();
  const states = await get('/api/states');
  const daily = { 'weather.home': await forecast('weather.home', 'daily') };
  const hourly = { 'weather.home': await forecast('weather.home', 'hourly') };
  const calendars = {};
  for (const c of ['calendar.home_schedule', 'calendar.work', 'calendar.birthdays', 'calendar.holidays', 'calendar.holidays_in_ireland']) {
    calendars[c] = localise(await get(`/api/calendars/${c}?start=x&end=y`));
  }
  const out = { now: now.toISOString(), timeZone: TZ, states, forecasts: { daily, hourly }, calendars };
  fs.writeFileSync(path.join(__dirname, 'house.json'), JSON.stringify(out, null, 1));
  console.log(`house.json at ${out.now}: ${states.length} states`);
})();
