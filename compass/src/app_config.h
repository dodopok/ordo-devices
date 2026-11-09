#pragma once

#include <stdint.h>

// What the compass talks to and how often. Nothing here is secret: the
// device gets its own token by the link flow, and the API meters that token.

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

namespace cfg {
// The API tells the device how often to come back (refresh.interval_seconds);
// this is only the value used before the first answer arrives.
constexpr uint32_t kDefaultRefreshSeconds = 300;
// Failed network calls back off to this and double, up to the cap.
constexpr uint32_t kRetryMinSeconds = 30;
constexpr uint32_t kRetryMaxSeconds = 600;
// A 429 waits longer than a plain failure.
constexpr uint32_t kRateLimitedSeconds = 900;

constexpr uint8_t kBrightnessNormal = 200;
constexpr uint8_t kBrightnessDim = 24;
constexpr uint32_t kDimAfterMs = 60 * 1000;
// Choices offered on the settings screen for the standby timeout, in
// minutes; 0 means the screen never switches off by itself.
constexpr uint8_t kStandbyChoices[] = {0, 2, 5, 10, 30};
constexpr uint8_t kStandbyDefaultMinutes = 10;
// CPU clock while the screen is off; Wi-Fi needs at least 80 MHz.
constexpr uint32_t kStandbyCpuMhz = 80;
constexpr uint32_t kActiveCpuMhz = 240;

// Holding BOOT this long wipes Wi-Fi credentials and the device token.
constexpr uint32_t kFactoryResetHoldMs = 10 * 1000;

constexpr uint32_t kHttpTimeoutMs = 15 * 1000;
}  // namespace cfg
