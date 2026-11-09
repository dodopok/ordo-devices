#pragma once

#include <Arduino.h>

#include "../model/today.h"
#include "../net/ordo_api.h"
#include "../store/settings.h"

namespace app {

// The compass as a state machine, driven from loop():
//
//   Boot -> Wifi -> Linking -> Linked
//                     ^           |
//                     +-- 401 ----+
//
// Linking shows a code and polls for the token; Linked fetches today on the
// API's own schedule and at the owner's midnight. A lapsed subscription or a
// disabled client keeps the link and shows a notice; a revoked token goes
// back to Linking.
class Controller {
 public:
  void begin();
  void loop();

 private:
  enum class State { Boot, Wifi, Linking, Linked, Blocked };

  void enterWifi();
  void enterLinking();
  void enterLinked();
  void stepWifi();
  void stepLinking();
  void stepLinked();
  void fetchToday();
  void scheduleRetry();
  void handleButtons();
  void handleBrightness();
  void handleBattery();
  void showCachedDay();
  void factoryReset();

  State state_ = State::Boot;
  store::Settings settings_;
  net::OrdoApi api_;
  model::Today today_;

  // Linking
  net::LinkStart link_;
  uint32_t linkStartedAt_ = 0;
  uint32_t nextPollAt_ = 0;
  uint32_t pollIntervalMs_ = 5000;
  uint32_t lastCountdownAt_ = 0;

  // Linked
  uint32_t nextFetchAt_ = 0;
  uint32_t dayEndsAt_ = 0;
  uint32_t retrySeconds_ = 0;
  bool offline_ = false;

  // Housekeeping
  uint32_t bootHeldSince_ = 0;
  uint32_t lastBatteryAt_ = 0;
  bool dimmed_ = false;
  bool wifiNoticeShown_ = false;
  bool portalNoticeShown_ = false;
};

}  // namespace app
