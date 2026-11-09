#include "settings.h"

#include <Preferences.h>
#include <nvs_flash.h>

#include "../app_config.h"

namespace store {
namespace {

const char* kNamespace = "ordo";
const char* kCachePartition = "cache";
const char* kCacheNamespace = "day";
Preferences nvs;
Preferences cache;
bool cacheOk = false;

String readString(Preferences& p, const char* key) {
  if (!p.isKey(key)) return String();
  return p.getString(key, "");
}

// The cache partition holds whatever was in that stretch of flash before the
// partition table changed: if NVS cannot make sense of it, start it afresh.
bool openCache() {
  esp_err_t err = nvs_flash_init_partition(kCachePartition);
  if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
    // Nothing in this partition is precious: it only holds what the API
    // will send again.
    nvs_flash_erase_partition(kCachePartition);
    err = nvs_flash_init_partition(kCachePartition);
  }
  if (err != ESP_OK) return false;
  return cache.begin(kCacheNamespace, false, kCachePartition);
}

}  // namespace

void Settings::begin() {
  nvs.begin(kNamespace, false);
  token_ = readString(nvs, "token");
  name_ = readString(nvs, "name");

  networks_.clear();
  const int count = nvs.getUChar("wn", 0);
  for (int i = 0; i < count && i < cfg::kMaxNetworks; i++) {
    char key[8];
    WifiNetwork network;
    snprintf(key, sizeof(key), "ws%d", i);
    network.ssid = readString(nvs, key);
    snprintf(key, sizeof(key), "wp%d", i);
    network.pass = readString(nvs, key);
    if (!network.ssid.isEmpty()) networks_.push_back(network);
  }

  cacheOk = openCache();
  if (cacheOk) etag_ = readString(cache, "etag");
  // Firmware 0.1 kept the payload as a string in the main partition.
  if (nvs.isKey("payload")) {
    if (cacheOk && !cache.isKey("payload")) {
      const String old = nvs.getString("payload", "");
      if (!old.isEmpty()) cache.putBytes("payload", old.c_str(), old.length());
    }
    nvs.remove("payload");
    nvs.remove("etag");
    nvs.remove("standby");
  }
}

void Settings::saveLink(const String& token, const String& name) {
  token_ = token;
  name_ = name;
  nvs.putString("token", token_);
  nvs.putString("name", name_);
}

void Settings::clearLink() {
  token_ = "";
  name_ = "";
  etag_ = "";
  nvs.remove("token");
  nvs.remove("name");
  if (cacheOk) cache.clear();
}

String Settings::payload() {
  if (!cacheOk || !cache.isKey("payload")) return String();
  const size_t length = cache.getBytesLength("payload");
  if (length == 0 || length > 32 * 1024) return String();
  std::vector<char> buffer(length + 1, 0);
  cache.getBytes("payload", buffer.data(), length);
  return String(buffer.data());
}

void Settings::saveToday(const String& etag, const String& payload) {
  etag_ = etag;
  if (!cacheOk) return;
  cache.putBytes("payload", payload.c_str(), payload.length());
  cache.putString("etag", etag_);
}

ui::Prefs Settings::prefs() {
  ui::Prefs p;
  p.screenSeconds = nvs.getUChar("scr", p.screenSeconds);
  p.brightness = nvs.getUChar("bri", p.brightness);
  p.nightDim = nvs.getBool("night", p.nightDim);
  p.homeFace = nvs.getUChar("face", p.homeFace);
  p.touchWakes = nvs.getBool("tapwake", p.touchWakes);
  p.deskAlwaysOn = nvs.getBool("desk", p.deskAlwaysOn);
  p.sound = nvs.getBool("snd", p.sound);
  p.volume = nvs.getUChar("vol", p.volume);
  p.hourBell = nvs.getBool("bell", p.hourBell);
  for (int i = 0; i < 4; i++) {
    char key[8];
    snprintf(key, sizeof(key), "rem%d", i);
    p.reminder[i] = nvs.getShort(key, p.reminder[i]);
  }
  p.syncHours = nvs.getUChar("sync", p.syncHours);
  p.beadsMode = nvs.getUChar("beads", p.beadsMode);
  p.animations = nvs.getBool("anim", p.animations);
  if (p.homeFace > 4) p.homeFace = 0;
  if (p.brightness > 2) p.brightness = 1;
  if (p.volume > 2) p.volume = 1;
  return p;
}

void Settings::savePrefs(const ui::Prefs& p) {
  // Preferences skips a write when the value is unchanged, so saving the lot
  // after one tap costs one flash write, not twelve.
  auto& s = nvs;
  if (s.getUChar("scr", 255) != p.screenSeconds) s.putUChar("scr", p.screenSeconds);
  if (s.getUChar("bri", 255) != p.brightness) s.putUChar("bri", p.brightness);
  if (!s.isKey("night") || s.getBool("night") != p.nightDim) s.putBool("night", p.nightDim);
  if (s.getUChar("face", 255) != p.homeFace) s.putUChar("face", p.homeFace);
  if (!s.isKey("tapwake") || s.getBool("tapwake") != p.touchWakes) s.putBool("tapwake", p.touchWakes);
  if (!s.isKey("desk") || s.getBool("desk") != p.deskAlwaysOn) s.putBool("desk", p.deskAlwaysOn);
  if (!s.isKey("snd") || s.getBool("snd") != p.sound) s.putBool("snd", p.sound);
  if (s.getUChar("vol", 255) != p.volume) s.putUChar("vol", p.volume);
  if (!s.isKey("bell") || s.getBool("bell") != p.hourBell) s.putBool("bell", p.hourBell);
  for (int i = 0; i < 4; i++) {
    char key[8];
    snprintf(key, sizeof(key), "rem%d", i);
    if (!s.isKey(key) || s.getShort(key) != p.reminder[i]) s.putShort(key, p.reminder[i]);
  }
  if (s.getUChar("sync", 255) != p.syncHours) s.putUChar("sync", p.syncHours);
  if (s.getUChar("beads", 255) != p.beadsMode) s.putUChar("beads", p.beadsMode);
  if (!s.isKey("anim") || s.getBool("anim") != p.animations) s.putBool("anim", p.animations);
}

void Settings::addNetwork(const String& ssid, const String& pass) {
  if (ssid.isEmpty()) return;
  for (size_t i = 0; i < networks_.size(); i++) {
    if (networks_[i].ssid == ssid) {
      networks_.erase(networks_.begin() + static_cast<long>(i));
      break;
    }
  }
  WifiNetwork network;
  network.ssid = ssid;
  network.pass = pass;
  networks_.insert(networks_.begin(), network);
  while (networks_.size() > static_cast<size_t>(cfg::kMaxNetworks)) networks_.pop_back();
  saveNetworks();
}

void Settings::removeNetwork(int index) {
  if (index < 0 || index >= static_cast<int>(networks_.size())) return;
  networks_.erase(networks_.begin() + index);
  saveNetworks();
}

void Settings::saveNetworks() {
  nvs.putUChar("wn", static_cast<uint8_t>(networks_.size()));
  for (int i = 0; i < cfg::kMaxNetworks; i++) {
    char ssidKey[8];
    char passKey[8];
    snprintf(ssidKey, sizeof(ssidKey), "ws%d", i);
    snprintf(passKey, sizeof(passKey), "wp%d", i);
    if (i < static_cast<int>(networks_.size())) {
      nvs.putString(ssidKey, networks_[static_cast<size_t>(i)].ssid);
      nvs.putString(passKey, networks_[static_cast<size_t>(i)].pass);
    } else {
      nvs.remove(ssidKey);
      nvs.remove(passKey);
    }
  }
}

void Settings::factoryReset() {
  token_ = "";
  name_ = "";
  etag_ = "";
  networks_.clear();
  nvs.clear();
  if (cacheOk) cache.clear();
}

}  // namespace store
