// =============================================================================
// store.h — what the device keeps between wakes.
//
//   NVS (Preferences, "sbview")   Wi-Fi networks, the server's address, the
//                                 pairing token, versions and ETags
//   flash (LittleFS)              the cache: the bundle, each screen's last
//                                 state, the theme packs, fetched icons and
//                                 pictures — so a screen can be redrawn, and
//                                 the theme used, without the network
// =============================================================================
#pragma once
#include <Arduino.h>
#include <vector>

namespace store {

bool begin();

String get(const char* key, const char* fallback = "");
void put(const char* key, const String& value);
int32_t getInt(const char* key, int32_t fallback = 0);
void putInt(const char* key, int32_t value);
void remove(const char* key);

bool exists(const char* path);
bool read(const char* path, std::vector<uint8_t>& out);
String readText(const char* path);
// Written to a temporary file first, then moved over the old one.
bool write(const char* path, const uint8_t* data, size_t len);
bool writeText(const char* path, const String& text);
void erase(const char* path);
// Every file in a folder (not its folders).
void eraseDir(const char* dir);
// Room left on the cache, in bytes.
size_t freeBytes();

// A file name from anything (a screen id, an icon name): letters, digits,
// '-' and '_' kept, the rest '_'.
String safeName(const char* s);

// --- Wi-Fi networks ------------------------------------------------------------
// The ones set up on the device itself come first (newest first), then the
// ones Switchboard Server lists for every device.
struct Network {
  String ssid;
  String password;
};
std::vector<Network> networks();
void addNetwork(const String& ssid, const String& password);
void setServerNetworks(const std::vector<Network>& list);

}  // namespace store
