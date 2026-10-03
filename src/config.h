// =============================================================================
// config.h — Switchboard Viewport for the Seeed reTerminal E1002
//
// Nothing about your house lives here: every screen, entity and icon comes
// from Switchboard Server. This is the hardware, the server's defaults and the
// device's own timings.
// =============================================================================
#pragma once
#include <stdint.h>

// The board, as Switchboard Server's updates know it (X-Board, and the
// image's SWITCHBOARD_FW:<board>:<version> marker). platformio.ini sets it.
#ifndef SWITCHBOARD_BOARD
#define SWITCHBOARD_BOARD "e1002"
#endif
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

#define SWITCHBOARD_NAME   "SWITCHBOARD"
#define SWITCHBOARD_SLOGAN "One tap. Every room."
#define DEVICE_KIND        "Viewport"

// -----------------------------------------------------------------------------
// Switchboard Server: found on the LAN by mDNS (its _switchboard._tcp service,
// else the host name below), then remembered.
// -----------------------------------------------------------------------------
#define SERVER_MDNS_NAME "switchboard"
#define SERVER_DEFAULT_PORT 45678

// -----------------------------------------------------------------------------
// Wi-Fi setup: the device's own hotspot and web page, for a panel with no
// keyboard or touchscreen. Its name ends in the last four hex digits of the
// MAC; its password is shown (and in the QR code) on the panel.
// -----------------------------------------------------------------------------
#define SETUP_AP_PREFIX "Switchboard-"
#define SETUP_TIMEOUT_MS (10UL * 60UL * 1000UL)   // then sleep until a button
#define WIFI_JOIN_TIMEOUT_MS 20000UL
#define WIFI_MAX_NETWORKS 6

// -----------------------------------------------------------------------------
// Sleep. The server says when to wake next (refreshInSec, quiet hours,
// departures turning imminent...); these are the fallbacks for when it can't.
// -----------------------------------------------------------------------------
const uint32_t SLEEP_DEFAULT_SEC   = 30 * 60;  // no answer from the server yet
const uint32_t SLEEP_ERROR_SEC     = 30 * 60;  // Wi-Fi / server / HA failed, before the server has said
const uint32_t SLEEP_PENDING_SEC   = 2 * 60;   // waiting to be approved
const uint32_t SLEEP_MIN_SEC       = 60;       // never sooner (panel protection)
const uint32_t SLEEP_MAX_SEC       = 12 * 3600;  // safety refresh, at the latest

// -----------------------------------------------------------------------------
// The device's own battery (shown in the footer):
//   > BATT_GREEN_PCT -> green   > BATT_RED_PCT -> black   else red
//   <= BATT_CRITICAL_PCT -> the full-screen "charge me", no Wi-Fi at all
// -----------------------------------------------------------------------------
const int BATT_GREEN_PCT    = 25;
const int BATT_RED_PCT      = 5;
const int BATT_CRITICAL_PCT = 2;

// The Spectra-6 panel can still be finishing its physical refresh when
// hibernate() returns; cutting power too soon leaves a partial image.
const int PRE_SLEEP_DELAY_MS = 2500;
// A press held this long is a long press.
const int LONG_PRESS_MS = 1000;

// -----------------------------------------------------------------------------
// Hardware pins — the reTerminal E1002.
// -----------------------------------------------------------------------------
#define EPD_SCK_PIN        7
#define EPD_MOSI_PIN       9
#define EPD_CS_PIN         10
#define EPD_DC_PIN         11
#define EPD_RES_PIN        12
#define EPD_BUSY_PIN       13
#define SD_EN_PIN          16
#define LED_PIN            6    // green LED, inverted (LOW = on)
#define BATTERY_ADC_PIN    1    // battery voltage via a /2 divider
#define BATTERY_ENABLE_PIN 21   // enables the battery monitor circuit
#define BTN_KEY0           3    // green, middle: refresh / (hold) setup
#define BTN_KEY1           4    // right: next screen / (hold) clear
#define BTN_KEY2           5    // left: previous screen / (hold) device info
#define SERIAL_RX          44
#define SERIAL_TX          43
#define I2C_SDA_PIN        19   // onboard SHT40
#define I2C_SCL_PIN        20
