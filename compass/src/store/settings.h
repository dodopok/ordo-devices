#pragma once

#include <Arduino.h>

#include <vector>

#include "../ui/ui.h"

namespace store {

struct WifiNetwork {
  String ssid;
  String pass;
};

// What the compass remembers across power cycles, in two NVS partitions:
//   nvs    the device token, the owner's settings and the Wi-Fi networks
//   cache  the last payload of /device/today and its ETag
// The payload has a partition of its own because it is close to 10 KB (an
// NVS string stops at 4000 bytes) and is the one thing rewritten every day.
// The token is never logged.
class Settings {
 public:
  void begin();

  const String& deviceToken() const { return token_; }
  const String& deviceName() const { return name_; }
  void saveLink(const String& token, const String& name);
  // Forgets the link and the cached day (after a 401 or an unlink).
  void clearLink();

  const String& etag() const { return etag_; }
  // The cached payload, read when asked for: nothing keeps 10 KB in RAM.
  String payload();
  void saveToday(const String& etag, const String& payload);

  ui::Prefs prefs();
  void savePrefs(const ui::Prefs& prefs);

  const std::vector<WifiNetwork>& networks() const { return networks_; }
  // Puts a network first in the list; a known name gets its password updated.
  void addNetwork(const String& ssid, const String& pass);
  void removeNetwork(int index);

  // Wipes everything, back to a fresh device.
  void factoryReset();

 private:
  void saveNetworks();

  String token_;
  String name_;
  String etag_;
  std::vector<WifiNetwork> networks_;
};

}  // namespace store
