#pragma once

#include <Arduino.h>

#include <functional>

#include "../model/content.h"
#include "site.h"

// Firmware over Wi-Fi. The site publishes a manifest and the binaries; the
// compass takes a newer one only if it is whole (its SHA-256 matches the
// manifest), signed when this build carries a public key, and written to the
// other app slot, so the firmware that is running stays intact until the new
// one has shown that it starts.
namespace net {

enum class OtaCheck { UpToDate, Available, Failed };
// Asks the site for its manifest. `offer` is filled when a newer firmware
// for this board is there.
OtaCheck otaCheck(Site& site, const String& language, model::FirmwareOffer& offer);

enum class OtaResult { Installed, Unreachable, BadImage, NoRoom };
// Downloads and writes `offer`. `progress` is called with 0..100 as it goes.
// On Installed the next restart boots the new firmware.
OtaResult otaInstall(Site& site, const model::FirmwareOffer& offer, const std::function<void(int)>& progress);

// Tells the bootloader the firmware now running is good. Until this is
// called after an update, a reset puts the previous firmware back.
void otaConfirmRunning();

// True when this build refuses firmware without a valid signature.
bool otaRequiresSignature();

}  // namespace net
