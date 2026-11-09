#pragma once

#include <Arduino.h>

namespace store {

// What survives a reboot: the device token (the only secret on the device),
// the ETag and body of the last answer so the compass shows yesterday's
// evening until the network comes back, and the name the owner gave it.
//
// Lives in NVS. Flash encryption, when enabled on the board, covers it.
class Settings {
 public:
  void begin();

  String deviceToken() const { return token_; }
  String deviceName() const { return name_; }
  String etag() const { return etag_; }
  String lastPayload() const { return payload_; }
  // Minutes without a touch before the screen switches off; 0 = never.
  uint8_t standbyMinutes() const { return standbyMinutes_; }
  void setStandbyMinutes(uint8_t minutes);

  void saveLink(const String& token, const String& name);
  void saveToday(const String& etag, const String& payload);
  void clearLink();
  // Wipes everything the compass knows. Wi-Fi credentials are the
  // portal's own and are cleared separately.
  void factoryReset();

 private:
  String token_;
  String name_;
  String etag_;
  String payload_;
  uint8_t standbyMinutes_ = 10;
};

}  // namespace store
