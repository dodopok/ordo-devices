#pragma once

#include <atomic>
#include <functional>
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
  void applyToday(const net::TodayResult& result);
  bool startFetch();
  static void fetchTask(void* self);
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
  void applyBrightness();
  void settleScreen(uint32_t atLeastMs = 0);
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
  // With the screen on, the request to the API runs on a task of its own:
  // it takes a second or two, and the glass keeps answering meanwhile. The
  // task only fills `result`; everything that follows from it happens back
  // on the main loop, once `done` is set.
  struct Fetch {
    String token;
    String etag;
    net::TodayResult result;
    std::atomic<bool> done{false};
    bool running = false;
  };
  Fetch fetch_;

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

  // What a screen asked for that blocks, or that needs LVGL to draw first
  // (the bell, the last message before switching off). Screens ask from
  // inside LVGL's own callbacks, where lv_timer_handler() returns without
  // doing anything; the request waits here for the main loop.
  std::function<void()> later_;

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
