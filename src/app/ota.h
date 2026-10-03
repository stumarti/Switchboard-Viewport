// =============================================================================
// ota.h — updates from Switchboard Server, as a remote gets them.
//
// The bundle's `firmware` says whether updates are on, the offer (version,
// size, SHA-256) for this board, whether "Update now" was pressed, the
// nightly window and the lowest battery to try at. A due offer is
// downloaded straight into the other app slot, checked (size, SHA-256, and
// the image's SWITCHBOARD_FW:e1002:<version> marker), and booted.
//
// Rollback: new firmware starts "pending" (verifyRollbackLater() in
// main.cpp); confirm() marks it good once it has reached the server, and
// reports how the update went. A build that can't reach the server never
// confirms, and the bootloader goes back to the one before.
// =============================================================================
#pragma once
#include <ArduinoJson.h>

namespace ota {

// After the bundle came back: mark this firmware good, report a pending
// update's outcome.
void confirm();

// Installs the offer if it's due now (restarts on success; draws its own
// screen). `localHour` for the nightly window, -1 if unknown.
void maybeInstall(JsonObjectConst firmware, int battPct, int localHour, bool timerWake);

}  // namespace ota
