// =============================================================================
// theme.h — icons and fonts, the Switchboard way.
//
//   Theme packs   Switchboard Server's Theme page compiles a viewport icon
//                 pack (SBI1: every fixed slot, 1-bit) and font pack (SBF1:
//                 the four faces). The device checks /api/theme's versions
//                 each wake and downloads a pack only when it changed; it's
//                 kept on flash. No pack = the kitchen panel's built-in art
//                 and Atkinson Hyperlegible.
//   Layout icons  icons a layout picks (status icons, "now" items, rooms)
//                 come one by one from /api/icons/mdi/<name>?size=<px>, are
//                 kept on flash, and are only fetched again if they're gone.
//   Pictures      album art, from /api/art in the panel's 4-bit format.
//
// A font pack's glyph tables are field for field Adafruit GFX's, so a face is
// just a GFXfont over the pack's bytes.
// =============================================================================
#pragma once

namespace theme {

// The packs already on flash, and the hooks icons.h and draw.h use.
void load();
// Checks the server's theme versions; downloads and loads what changed.
void sync();
// Fetches what a collecting pass (screens.h) found missing.
void fetchNeeds();

}  // namespace theme
