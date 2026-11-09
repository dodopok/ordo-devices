#include "ota.h"

#include <Update.h>
#include <esp_ota_ops.h>
#include <mbedtls/base64.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include "../app_config.h"

// The public half of the key releases are signed with, when there is one:
// `tools/release.sh keygen` writes it to include/ota_public_key.h. A build
// without that file takes any firmware whose hash matches the manifest; a
// build with it also demands the signature.
#if __has_include("ota_public_key.h")
#include "ota_public_key.h"
#endif

// The Arduino core would declare the new firmware good before setup() runs.
// Returning true here leaves that to otaConfirmRunning(), called once the
// firmware has got as far as bringing up its screen.
extern "C" bool verifyRollbackLater() { return true; }

namespace net {
namespace {

String hexOf(const uint8_t* bytes, size_t length) {
  static const char* digits = "0123456789abcdef";
  String out;
  out.reserve(length * 2);
  for (size_t i = 0; i < length; i++) {
    out += digits[bytes[i] >> 4];
    out += digits[bytes[i] & 0x0f];
  }
  return out;
}

#ifdef ORDO_OTA_PUBLIC_KEY_PEM
bool signatureValid(const uint8_t hash[32], const String& signatureBase64) {
  uint8_t signature[160];
  size_t length = 0;
  if (signatureBase64.isEmpty() ||
      mbedtls_base64_decode(signature, sizeof(signature), &length, reinterpret_cast<const unsigned char*>(signatureBase64.c_str()),
                            signatureBase64.length()) != 0) {
    return false;
  }
  mbedtls_pk_context key;
  mbedtls_pk_init(&key);
  const char* pem = ORDO_OTA_PUBLIC_KEY_PEM;
  bool ok = mbedtls_pk_parse_public_key(&key, reinterpret_cast<const unsigned char*>(pem), strlen(pem) + 1) == 0;
  ok = ok && mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, hash, 32, signature, length) == 0;
  mbedtls_pk_free(&key);
  return ok;
}
#endif

}  // namespace

bool otaRequiresSignature() {
#ifdef ORDO_OTA_PUBLIC_KEY_PEM
  return true;
#else
  return false;
#endif
}

OtaCheck otaCheck(Site& site, const String& language, model::FirmwareOffer& offer) {
  offer = model::FirmwareOffer();
  std::vector<uint8_t> body;
  const int code = site.get("/firmware/manifest.json", body, cfg::kManifestMaxBytes);
  if (code != 200 || body.empty()) return OtaCheck::Failed;
  model::FirmwareOffer found;
  if (!model::parseManifest(reinterpret_cast<const char*>(body.data()), body.size(), ORDO_BOARD_KEY, language, found)) {
    return OtaCheck::Failed;
  }
  if (!model::versionNewer(found.version.c_str(), ORDO_FIRMWARE_VERSION)) return OtaCheck::UpToDate;
  // A signed build does not even offer what it would refuse to install.
  if (otaRequiresSignature() && found.signature.isEmpty()) return OtaCheck::UpToDate;
  offer = found;
  return OtaCheck::Available;
}

OtaResult otaInstall(Site& site, const model::FirmwareOffer& offer, const std::function<void(int)>& progress) {
  if (!offer.found || offer.size < cfg::kFirmwareMinBytes || offer.size > cfg::kFirmwareMaxBytes) return OtaResult::BadImage;
  if (!Update.begin(offer.size, U_FLASH)) return OtaResult::NoRoom;

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  size_t done = 0;
  int shown = -1;
  bool writeFailed = false;
  const std::function<bool(const uint8_t*, size_t)> take = [&](const uint8_t* data, size_t size) {
    // More than the manifest promised is not the file it described.
    if (done + size > offer.size) return false;
    mbedtls_sha256_update(&sha, data, size);
    // Update.write wants a mutable buffer; it only reads it.
    if (Update.write(const_cast<uint8_t*>(data), size) != size) {
      writeFailed = true;
      return false;
    }
    done += size;
    const int percent = static_cast<int>(done * 100 / offer.size);
    if (percent != shown && progress) {
      shown = percent;
      progress(percent);
    }
    return true;
  };

  size_t received = 0;
  const int code = site.stream(offer.path, take, received);
  uint8_t hash[32];
  mbedtls_sha256_finish(&sha, hash);
  mbedtls_sha256_free(&sha);

  if (code != 200 || done != offer.size) {
    Update.abort();
    if (writeFailed) return OtaResult::NoRoom;
    return code == 200 || code == kSiteBodyRefused ? OtaResult::BadImage : OtaResult::Unreachable;
  }
  bool good = hexOf(hash, sizeof(hash)) == offer.sha256;
#ifdef ORDO_OTA_PUBLIC_KEY_PEM
  good = good && signatureValid(hash, offer.signature);
#endif
  if (!good) {
    Update.abort();
    return OtaResult::BadImage;
  }
  // end(true): the size was known from the start, and the boot partition is
  // switched only now, after every check.
  if (!Update.end(true)) return OtaResult::BadImage;
  return OtaResult::Installed;
}

void otaConfirmRunning() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  if (running && esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_ota_mark_app_valid_cancel_rollback();
  }
}

}  // namespace net
