#pragma once

#include <stdint.h>

// What the compass talks to and how it spends its battery. Nothing here is
// secret: the device gets its own token by the link flow, and the API meters
// that token.

#ifndef ORDO_API_BASE_URL
#define ORDO_API_BASE_URL "https://api.caminhoanglicano.com.br/api/v1"
#endif
#ifndef ORDO_API_FALLBACK_URL
#define ORDO_API_FALLBACK_URL "https://api.oficio.app/api/v1"
#endif
#ifndef ORDO_DEVICE_CLIENT_ID
#define ORDO_DEVICE_CLIENT_ID "ordo-compass"
#endif
#ifndef ORDO_FIRMWARE_VERSION
#define ORDO_FIRMWARE_VERSION "0.0.0-dev"
#endif
#define ORDO_USER_AGENT "OrdoCompass/" ORDO_FIRMWARE_VERSION

// Serial logging is a debug-build thing: it costs boot time and a live USB
// peripheral, and a keychain has nobody listening.
#ifdef ORDO_DEBUG
#define ORDO_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define ORDO_LOG(...) \
  do {                \
  } while (0)
#endif

namespace cfg {

// ---- synchronising
// Looking at the screen refreshes the day when the last answer is older than this.
constexpr uint32_t kStaleAfterSeconds = 30 * 60;
// On mains power the compass behaves as a desk clock and asks this often.
constexpr uint32_t kDeskRefreshSeconds = 5 * 60;
// A background wake gives the network this long before going back to sleep.
constexpr uint32_t kBackgroundSyncMs = 20 * 1000;
// After the day turns, wait a little so the API is already on the new day.
constexpr uint32_t kAfterMidnightSeconds = 120;
// Failed foreground syncs are not retried before this.
constexpr uint32_t kRetrySeconds = 5 * 60;
constexpr uint32_t kRateLimitedSeconds = 15 * 60;
constexpr uint32_t kHttpTimeoutMs = 12 * 1000;

// ---- the screen
constexpr uint8_t kBrightness[3] = {70, 140, 230};
constexpr uint8_t kBrightnessNight = 45;  // between 22h and 6h, when asked
constexpr uint8_t kBrightnessDesk = 40;   // on mains, after a minute untouched
constexpr uint32_t kDeskDimAfterMs = 60 * 1000;
// Setup screens (Wi-Fi, the link code) stay lit this long without a touch.
constexpr uint32_t kSetupAwakeMs = 10 * 60 * 1000;
constexpr uint32_t kWakeMinimumMs = 4 * 1000;

// ---- the processor
constexpr uint32_t kCpuRadioMhz = 240;  // Wi-Fi and TLS
constexpr uint32_t kCpuScreenMhz = 160; // drawing, no radio

// ---- buttons
constexpr uint32_t kBootLongPressMs = 900;
constexpr uint32_t kFactoryResetHoldMs = 10 * 1000;
// A swipe is a drag of at least this many pixels, mostly along one axis.
constexpr int32_t kSwipePixels = 42;

// ---- battery
constexpr int kBatteryQuietPercent = 10;  // no bell, no torch
constexpr int kBatteryOffPercent = 3;     // save and switch off...
constexpr int kBatteryOffMillivolts = 3400;  // ...when the cell's voltage agrees

// ---- Wi-Fi
constexpr int kMaxNetworks = 5;
constexpr uint32_t kWifiFastConnectMs = 6 * 1000;
constexpr uint32_t kWifiConnectMs = 14 * 1000;
constexpr uint32_t kPortalTimeoutMs = 10 * 60 * 1000;

}  // namespace cfg
