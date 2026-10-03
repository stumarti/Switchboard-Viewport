#!/usr/bin/env node
'use strict';
/**
 * What Switchboard Server sends the display for house.json: its own
 * buildScreens() over the kitchen dashboard (its default viewport layout),
 * at the moment the house was captured. Writes server-<screen>.json.
 *
 *   SERVER_DIR=../Switchboard-Server node test/compare/server-state.js
 */
const fs = require('fs');
const path = require('path');
const SERVER = path.resolve(process.env.SERVER_DIR || path.join(__dirname, '..', '..', '..', 'Switchboard-Server'));
const dashboard = require(path.join(SERVER, 'lib', 'dashboard'));
const { buildScreens, refreshPlan } = require(path.join(SERVER, 'lib', 'dashboard-state'));

const house = JSON.parse(fs.readFileSync(path.join(__dirname, 'house.json'), 'utf8'));
const states = Object.fromEntries(house.states.map((s) => [s.entity_id, s]));
const layout = dashboard.defaultLayout();
const screens = buildScreens(layout, { states, forecasts: house.forecasts, calendars: house.calendars, now: new Date(house.now), timeZone: house.timeZone });
for (const s of layout.screens) {
  fs.writeFileSync(path.join(__dirname, `server-${s.id}.json`), JSON.stringify({ quiet: refreshPlan(layout, new Date(house.now), house.timeZone).quiet, data: screens[s.id] }, null, 1));
}
console.log(`server states for ${layout.screens.map((s) => s.id).join(', ')}`);
