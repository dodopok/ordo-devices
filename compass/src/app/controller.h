#pragma once

#include <Arduino.h>

#include "../board/power.h"
#include "schedule.h"
#include "../model/today.h"
#include "../net/ordo_api.h"
#include "../net/wifi.h"
#include "../store/settings.h"
#include "../ui/ui.h"

namespace app {

// The compass as a keychain: asleep almost always, awake for a glance.
//
//   deep sleep --PWR/BOOT--> glance (screen on, radio off) --idle--> deep sleep
//        |                       |
//        +--timer--> background  +--day on record is stale--> sync (Wi-Fi for seconds)
//
// A wake by a button draws the day from what is stored and only then, if it
// is stale, fetches a fresh one. A wake by the timer keeps the screen dark:
// it syncs, or lights up just long enough to ring a reminder. On mains
// power, by default, it stays on like a desk clock.
class Controller {
 public:
  void begin();
  void loop();

 private:
  enum class Flow { Day, WifiSetup, Linking };
  enum class Notice { None, Reminder, Bell, SilenceDone };

  // boot
  void loadDay();
  bool background();
  void startScreen();
  void showInitial();

  // flows
  void enterWifiSetup();
  void stepWifiSetup();
  void afterWifi();
  void enterLinking();
  void stepLinking();
  void stepDay();

  // the day
  model::Today currentView() const;
  void refreshView();
  bool linked() const { return !settings_.deviceToken().isEmpty(); }
  bool stale(uint32_t afterSeconds) const;
  void requestSync(bool force);
  void stepSync();
  void finishSync();
  bool syncBlocking();
  void fetchToday();
  void showBlocked();

  // time-driven
  Moment moment() const;
  Plan plan() const;
  Due takeDue();
  void syncFailed();
  void everySecond();
  void fire(const Due& due);
  void ringBell(int strikes);
  uint64_t secondsToNextWake() const;

  // input and power
  void handleInput();
  void applyPower();
  uint8_t targetBrightness(uint32_t idleMs) const;
  bool deskMode() const;
  void pushStatus();
  [[noreturn]] void goToSleep();
  void factoryReset();
  ui::Actions actions();

  store::Settings settings_;
  net::OrdoApi api_;
  model::Today today_;
  ui::Prefs prefs_;
  board::BatteryState battery_;
  board::Wake wake_ = board::Wake::ColdBoot;

  Flow flow_ = Flow::Day;
  bool screenUp_ = false;
  long shownDays_ = -1;
  Notice pending_ = Notice::None;
  int pendingValue_ = 0;

  // sync
  bool syncing_ = false;
  uint32_t syncStartedMs_ = 0;

  // Wi-Fi setup
  net::PortalState portalShown_ = net::PortalState::Off;
  bool portalDrawn_ = false;
  uint32_t portalDoneMs_ = 0;

  // linking
  net::LinkStart link_;
  uint32_t linkStartedAt_ = 0;
  uint32_t nextPollAt_ = 0;
  uint32_t pollIntervalMs_ = 5000;
  uint32_t lastCountdownAt_ = 0;
  uint32_t lastWifiTryMs_ = 0;

  // housekeeping
  uint32_t startedMs_ = 0;
  uint32_t lastSecondMs_ = 0;
  uint32_t lastBatteryMs_ = 0;
  int lastMinute_ = -1;
  uint32_t bootDownSince_ = 0;
  bool bootLongSent_ = false;
  bool bootIgnore_ = false;
};

}  // namespace app
