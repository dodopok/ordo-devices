#include "controller.h"

#include <Wire.h>
#include <esp_system.h>

#include "../app_config.h"
#include "../board/audio.h"
#include "../board/display.h"
#include "../board/touch.h"
#include "../sys/clock.h"
#include "../sys/rtc_state.h"
#include "board_pins.h"

namespace app {
namespace {

inline bool due(uint32_t at) { return static_cast<int32_t>(millis() - at) >= 0; }

// Flat means flat by both readings: the fuel gauge can answer 0 % for a
// moment after power-on, and switching off on that alone would leave a
// healthy compass unable to start.
bool batteryFlat(const board::BatteryState& battery) {
  return battery.present && !battery.onUsb && battery.percent >= 0 && battery.percent <= cfg::kBatteryOffPercent &&
         battery.millivolts > 2500 && battery.millivolts < cfg::kBatteryOffMillivolts;
}

// The owner's zone, worked out from an answer of an API that does not send
// `utc_offset_seconds` yet: the payload says how long until the owner's
// midnight, the `Date` header says what time it is in UTC.
int32_t offsetFromDayEnd(int64_t utc, uint32_t dayEndsInSeconds) {
  const int64_t localSecond = 86400 - static_cast<int64_t>(dayEndsInSeconds);
  int64_t offset = localSecond - (utc % 86400);
  while (offset > 14 * 3600) offset -= 86400;
  while (offset < -12 * 3600) offset += 86400;
  // Zones sit on quarter hours.
  const int64_t quarter = 900;
  offset = ((offset + (offset >= 0 ? quarter / 2 : -quarter / 2)) / quarter) * quarter;
  return static_cast<int32_t>(offset);
}

ui::Swipe swipeOf(board::TouchSwipe swipe) {
  switch (swipe) {
    case board::TouchSwipe::Up: return ui::Swipe::Up;
    case board::TouchSwipe::Down: return ui::Swipe::Down;
    case board::TouchSwipe::Left: return ui::Swipe::Left;
    default: return ui::Swipe::Right;
  }
}

ui::PortalStep stepOf(net::PortalState state) {
  switch (state) {
    case net::PortalState::PhoneConnected: return ui::PortalStep::PhoneConnected;
    case net::PortalState::Connecting: return ui::PortalStep::Connecting;
    case net::PortalState::Connected: return ui::PortalStep::Connected;
    case net::PortalState::Failed: return ui::PortalStep::Failed;
    default: return ui::PortalStep::WaitingPhone;
  }
}

}  // namespace

// ==================================================================== boot

void Controller::begin() {
  startedMs_ = millis();
  const bool warm = sys::rtcBegin();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  if (!board::powerInit()) {
    ORDO_LOG("[ordo] PMU did not answer; running without battery telemetry\n");
  }
  wake_ = board::wakeCause();
  settings_.begin();
  prefs_ = settings_.prefs();
  net::wifiBegin(&settings_);
  battery_ = board::batteryState();
  loadDay();
  ORDO_LOG("[ordo] %s firmware %s, wake %d, %s\n", ORDO_BOARD_NAME, ORDO_FIRMWARE_VERSION, static_cast<int>(wake_),
           warm ? "from sleep" : "cold");

  // For the "Sobre" screen: how often it woke and synchronised today.
  sys::RtcState& r = sys::rtc();
  const int32_t today = sys::clockValid() ? static_cast<int32_t>(sys::localDays()) : 0;
  if (r.countersDay != today) {
    r.countersDay = today;
    r.wakesToday = 0;
    r.syncsToday = 0;
  }
  r.wakesToday++;

  // A wake the firmware scheduled: stay dark unless something has to ring.
  if (wake_ == board::Wake::Timer && !background()) goToSleep();

  startScreen();
  showInitial();
}

void Controller::loadDay() {
  const String cached = settings_.payload();
  if (cached.isEmpty()) return;
  model::Today parsed;
  if (!model::parseToday(cached.c_str(), parsed)) return;
  today_ = parsed;
  // After a cold start the zone is known before the time is.
  if (!sys::utcOffsetKnown() && today_.hasUtcOffset) sys::setUtcOffset(today_.utcOffsetSeconds);
}

bool Controller::background() {
  Due what = takeDue();
  const bool dayTurned = today_.valid && sys::clockValid() && sys::utcOffsetKnown() && sys::localDays() != today_.day.days;
  const bool wantSync = linked() && net::wifiConfigured() &&
                        (what.reminder >= 0 || dayTurned || (prefs_.syncHours > 0 && stale(prefs_.syncHours * 3600UL - 300)));
  if (wantSync) syncBlocking();

  if (what.silence) {
    pending_ = Notice::SilenceDone;
    return true;
  }
  if (what.reminder >= 0) {
    // Fresh from the API when there was a network: an office already prayed
    // is not announced.
    const model::Today view = currentView();
    if (view.officesKnown && view.offices[what.reminder].done) return false;
    pending_ = Notice::Reminder;
    pendingValue_ = what.reminder;
    return true;
  }
  if (what.bellHour >= 0) {
    pending_ = Notice::Bell;
    pendingValue_ = what.bellHour;
    return true;
  }
  return false;
}

void Controller::startScreen() {
  if (!board::displayInit()) ORDO_LOG("[ordo] display did not answer\n");
  if (!board::touchInit()) ORDO_LOG("[ordo] touch did not answer\n");
  ui::init(prefs_, actions());
  ui::setBeadsCount(sys::rtc().beads);
  screenUp_ = true;
  bootIgnore_ = board::bootButtonDown();
  pushStatus();
}

void Controller::showInitial() {
  // A battery that is all but flat: say so and switch off before the cell
  // is drained further.
  if (batteryFlat(battery_)) {
    ui::showLowBattery();
    lv_timer_handler();
    board::displaySetBrightness(cfg::kBrightness[0]);
    delay(2500);
    board::displayOff();
    board::powerOff();
  }

  if (today_.valid) {
    refreshView();
  } else {
    ui::showSplash();
  }

  if (!net::wifiConfigured()) {
    enterWifiSetup();
  } else if (!linked()) {
    enterLinking();
  } else {
    flow_ = Flow::Day;
    if (sys::rtc().blocked) showBlocked();
  }

  if (pending_ != Notice::None) {
    Due what;
    if (pending_ == Notice::Reminder) what.reminder = pendingValue_;
    if (pending_ == Notice::Bell) what.bellHour = pendingValue_;
    if (pending_ == Notice::SilenceDone) what.silence = true;
    pending_ = Notice::None;
    lv_timer_handler();
    board::displaySetBrightness(targetBrightness(0));
    fire(what);
    return;
  }

  // Everything LVGL has to draw is drawn before the panel lights up.
  lv_timer_handler();
  board::displaySetBrightness(targetBrightness(0));
  setCpuFrequencyMhz(cfg::kCpuScreenMhz);
}

// =================================================================== loop

void Controller::loop() {
  net::wifiLoop();
  lv_timer_handler();
  handleInput();

  switch (flow_) {
    case Flow::WifiSetup: stepWifiSetup(); break;
    case Flow::Linking: stepLinking(); break;
    case Flow::Day: stepDay(); break;
  }
  stepSync();
  if (millis() - lastSecondMs_ >= 1000) {
    lastSecondMs_ = millis();
    everySecond();
  }
  applyPower();
}

// ============================================================ Wi-Fi setup

void Controller::enterWifiSetup() {
  flow_ = Flow::WifiSetup;
  syncing_ = false;
  setCpuFrequencyMhz(cfg::kCpuRadioMhz);
  net::portalStart();
  portalDrawn_ = false;
  portalDoneMs_ = 0;
}

void Controller::stepWifiSetup() {
  const net::PortalState state = net::portalState();
  if (state == net::PortalState::Off) {
    // Closed by its own timeout, with nobody having come.
    afterWifi();
    return;
  }
  if (!portalDrawn_ || state != portalShown_) {
    portalDrawn_ = true;
    portalShown_ = state;
    ui::showWifiPortal(net::portalSsid(), stepOf(state), net::portalNetwork());
    lv_display_trigger_activity(nullptr);
    if (state == net::PortalState::Connected) portalDoneMs_ = millis();
  }
  // Leave "Conectado" on screen for a moment, then close the open network.
  if (state == net::PortalState::Connected && millis() - portalDoneMs_ > 3500) {
    net::portalStop();
    afterWifi();
  }
}

void Controller::afterWifi() {
  if (!net::wifiConfigured()) {
    flow_ = Flow::Day;
    if (today_.valid) ui::goHome();
    else ui::showMessage("Sem Wi-Fi", "abra Ajustes › Wi-Fi para escolher uma rede", false);
    setCpuFrequencyMhz(cfg::kCpuScreenMhz);
    return;
  }
  if (!linked()) {
    enterLinking();
    return;
  }
  flow_ = Flow::Day;
  if (today_.valid) ui::goHome();
  else ui::showMessage("Wi-Fi pronto", "buscando o dia de hoje…", true);
  requestSync(true);
}

// ================================================================ linking

void Controller::enterLinking() {
  flow_ = Flow::Linking;
  link_ = net::LinkStart();
  linkStartedAt_ = 0;
  nextPollAt_ = millis();
  lastWifiTryMs_ = millis();
  setCpuFrequencyMhz(cfg::kCpuRadioMhz);
  ui::showMessage("Vincule ao Ordo", "conectando ao Wi-Fi…", true);
  net::wifiConnect();
}

void Controller::stepLinking() {
  const uint32_t now = millis();
  if (!net::wifiConnected()) {
    if (net::wifiState() != net::WifiState::Connecting && now - lastWifiTryMs_ > 12000) {
      lastWifiTryMs_ = now;
      ui::showMessage("Sem Wi-Fi", "procurando uma rede conhecida…", true);
      net::wifiConnect();
    }
    return;
  }

  if (link_.deviceCode.isEmpty()) {
    if (!due(nextPollAt_)) return;
    if (api_.startLink(link_)) {
      linkStartedAt_ = now;
      pollIntervalMs_ = link_.interval * 1000UL;
      nextPollAt_ = now + pollIntervalMs_;
      ui::showLink(link_.userCode, link_.verificationUriComplete);
      lv_display_trigger_activity(nullptr);
      ORDO_LOG("[ordo] link code %s\n", link_.userCode.c_str());
    } else {
      ui::showMessage("Sem resposta do Ordo", "tentando de novo em instantes", true);
      nextPollAt_ = now + 30 * 1000UL;
    }
    return;
  }

  // Countdown on screen, once a second.
  if (now - lastCountdownAt_ >= 1000) {
    lastCountdownAt_ = now;
    const uint32_t elapsed = (now - linkStartedAt_) / 1000;
    ui::setLinkCountdown(elapsed >= link_.expiresIn ? 0 : link_.expiresIn - elapsed);
  }
  if (!due(nextPollAt_)) return;
  nextPollAt_ = now + pollIntervalMs_;

  const net::TokenResult result = api_.pollToken(link_.deviceCode);
  switch (result.status) {
    case net::TokenPoll::Success:
      settings_.saveLink(result.deviceToken, result.deviceName);
      sys::rtc().blocked = 0;
      ORDO_LOG("[ordo] linked\n");
      ui::showMessage("Vinculado", "buscando o dia de hoje…", true);
      lv_timer_handler();
      flow_ = Flow::Day;
      fetchToday();
      finishSync();
      break;
    case net::TokenPoll::Pending:
      break;
    case net::TokenPoll::SlowDown:
      pollIntervalMs_ += 5000;
      break;
    case net::TokenPoll::Expired:
    case net::TokenPoll::Invalid:
      // Ask for a fresh code; the old one is gone either way.
      link_ = net::LinkStart();
      nextPollAt_ = now + 1000;
      break;
    case net::TokenPoll::Denied:
      ui::showMessage("Não foi possível vincular", "confira no app se a conta tem Ordo+ e menos de 5 aparelhos", false);
      link_ = net::LinkStart();
      nextPollAt_ = now + 30 * 1000UL;
      break;
    case net::TokenPoll::NetworkError:
      nextPollAt_ = now + 30 * 1000UL;
      break;
  }
}

// ================================================================ the day

model::Today Controller::currentView() const {
  if (!today_.valid || !sys::clockValid() || !sys::utcOffsetKnown()) return today_;
  return model::viewFor(today_, sys::localDays());
}

void Controller::refreshView() {
  if (!today_.valid) return;
  const model::Today view = currentView();
  shownDays_ = view.day.days;
  ui::setToday(view);
}

bool Controller::stale(uint32_t afterSeconds) const {
  const sys::RtcState& r = sys::rtc();
  if (r.lastAnswerRaw == 0) return true;
  return sys::rawSeconds() - r.lastAnswerRaw >= static_cast<int64_t>(afterSeconds);
}

void Controller::stepDay() {
  if (syncing_ || !linked() || !net::wifiConfigured()) return;
  if (!stale(deskMode() ? cfg::kDeskRefreshSeconds : cfg::kStaleAfterSeconds)) return;
  requestSync(false);
}

void Controller::requestSync(bool force) {
  if (syncing_ || !linked() || !net::wifiConfigured()) return;
  if (!force && sys::rawSeconds() < sys::rtc().nextTryRaw) return;
  syncing_ = true;
  syncStartedMs_ = millis();
  setCpuFrequencyMhz(cfg::kCpuRadioMhz);
  net::wifiConnect();
  pushStatus();
}

void Controller::stepSync() {
  if (!syncing_) return;
  switch (net::wifiState()) {
    case net::WifiState::Connected:
      fetchToday();
      finishSync();
      break;
    case net::WifiState::Connecting:
      if (millis() - syncStartedMs_ > 30000) {
        sys::rtc().nextTryRaw = sys::rawSeconds() + cfg::kRetrySeconds;
        finishSync();
      }
      break;
    default:
      // No known network in range: nothing to do until the next look.
      sys::rtc().nextTryRaw = sys::rawSeconds() + cfg::kRetrySeconds;
      finishSync();
      break;
  }
}

void Controller::finishSync() {
  syncing_ = false;
  // On mains power the radio stays up; on battery it is off again at once.
  if (!deskMode() && flow_ == Flow::Day) net::wifiOff();
  if (flow_ == Flow::Day) setCpuFrequencyMhz(cfg::kCpuScreenMhz);
  if (screenUp_) pushStatus();
}

bool Controller::syncBlocking() {
  setCpuFrequencyMhz(cfg::kCpuRadioMhz);
  net::wifiConnect();
  const uint32_t start = millis();
  while (net::wifiState() == net::WifiState::Connecting && millis() - start < cfg::kBackgroundSyncMs) {
    net::wifiLoop();
    delay(15);
  }
  const bool connected = net::wifiConnected();
  if (connected) fetchToday();
  else sys::rtc().nextTryRaw = sys::rawSeconds() + cfg::kRetrySeconds;
  net::wifiOff();
  return connected;
}

void Controller::fetchToday() {
  const net::TodayResult result = api_.fetchToday(settings_.deviceToken(), settings_.etag());
  sys::RtcState& r = sys::rtc();
  const int64_t raw = sys::rawSeconds();
  // Every answer, whatever it says, carries the time.
  const bool answered = result.status != net::TodayStatus::NetworkError;
  if (answered && api_.serverTime() > 0) sys::clockSet(api_.serverTime());

  switch (result.status) {
    case net::TodayStatus::Updated: {
      model::Today parsed;
      if (!model::parseToday(result.body.c_str(), parsed)) {
        ORDO_LOG("[ordo] could not parse today\n");
        r.nextTryRaw = raw + cfg::kRetrySeconds;
        break;
      }
      today_ = parsed;
      settings_.saveToday(result.etag, result.body);
      if (parsed.hasUtcOffset) sys::setUtcOffset(parsed.utcOffsetSeconds);
      else if (parsed.dayEndsInSeconds > 0 && sys::clockValid()) {
        sys::setUtcOffset(offsetFromDayEnd(sys::nowUtc(), parsed.dayEndsInSeconds));
      }
      r.lastAnswerRaw = raw;
      r.nextTryRaw = 0;
      r.blocked = 0;
      r.syncsToday++;
      if (screenUp_) {
        const bool fromNotice = ui::showingNotice() && !ui::showingDay();
        refreshView();
        if (fromNotice) ui::goHome();
      }
      ORDO_LOG("[ordo] today %s\n", today_.day.date.c_str());
      break;
    }
    case net::TodayStatus::NotModified:
      r.lastAnswerRaw = raw;
      r.nextTryRaw = 0;
      r.blocked = 0;
      r.syncsToday++;
      // Unchanged since the last look, but the day may have been waiting for
      // the clock: show it for the date it now knows.
      if (screenUp_ && today_.valid) {
        const bool fromNotice = ui::showingNotice() && !ui::showingDay();
        refreshView();
        if (fromNotice) ui::goHome();
      }
      break;
    case net::TodayStatus::Unauthorized:
      ORDO_LOG("[ordo] token refused; linking again\n");
      settings_.clearLink();
      today_ = model::Today();
      r.lastAnswerRaw = 0;
      r.blocked = 0;
      if (screenUp_) {
        ui::clearDay();
        enterLinking();
      }
      break;
    case net::TodayStatus::PremiumRequired:
      r.blocked = 1;
      r.lastAnswerRaw = raw;
      if (screenUp_) showBlocked();
      break;
    case net::TodayStatus::ClientDisabled:
      r.blocked = 2;
      r.lastAnswerRaw = raw;
      r.nextTryRaw = raw + cfg::kRateLimitedSeconds;
      if (screenUp_) showBlocked();
      break;
    case net::TodayStatus::RateLimited:
      r.nextTryRaw = raw + cfg::kRateLimitedSeconds;
      break;
    case net::TodayStatus::NetworkError:
    case net::TodayStatus::ServerError:
      r.nextTryRaw = raw + cfg::kRetrySeconds;
      break;
  }
}

void Controller::showBlocked() {
  if (sys::rtc().blocked == 2) {
    ui::showMessage("Aparelho desativado", "este tipo de aparelho foi desativado no Ordo", false);
  } else {
    ui::showMessage("Assinatura Ordo+ vencida", "renove no app e a bússola volta sozinha", false);
  }
}

// ============================================================ time-driven

Controller::Due Controller::takeDue() {
  Due what;
  sys::RtcState& r = sys::rtc();
  const int64_t raw = sys::rawSeconds();
  if (r.silenceEndsRaw != 0 && raw >= r.silenceEndsRaw) {
    r.silenceEndsRaw = 0;
    what.silence = true;
  }
  if (!sys::clockValid() || !sys::utcOffsetKnown()) return what;

  const int32_t day = static_cast<int32_t>(sys::localDays());
  if (r.firedDay != day) {
    r.firedDay = day;
    r.remindersFired = 0;
    r.bellsFired = 0;
  }
  const int minute = sys::minuteOfDay();
  for (int i = 0; i < 4; i++) {
    const int at = prefs_.reminder[i];
    if (at < 0 || (r.remindersFired & (1 << i)) || minute < at) continue;
    r.remindersFired |= static_cast<uint8_t>(1 << i);
    // More than twenty minutes late (it was switched off): let it pass.
    if (minute - at <= 20 && what.reminder < 0) what.reminder = i;
  }
  if (prefs_.hourBell) {
    static const int hours[3] = {6, 12, 18};
    for (int i = 0; i < 3; i++) {
      const int at = hours[i] * 60;
      if ((r.bellsFired & (1 << i)) || minute < at) continue;
      r.bellsFired |= static_cast<uint8_t>(1 << i);
      if (minute - at <= 10 && what.bellHour < 0) what.bellHour = hours[i];
    }
  }
  return what;
}

void Controller::everySecond() {
  // A time of silence counting down on its screen.
  sys::RtcState& r = sys::rtc();
  if (r.silenceEndsRaw != 0) {
    const int64_t left = r.silenceEndsRaw - sys::rawSeconds();
    ui::setSilence(left > 0, left > 0 ? static_cast<uint32_t>(left) : 0);
  }

  Due what = takeDue();
  if (what.silence || what.reminder >= 0 || what.bellHour >= 0) {
    if (what.reminder >= 0) {
      const model::Today view = currentView();
      if (view.officesKnown && view.offices[what.reminder].done) what.reminder = -1;
    }
    fire(what);
  }

  if (millis() - lastBatteryMs_ >= 20000 || lastBatteryMs_ == 0) {
    lastBatteryMs_ = millis();
    battery_ = board::batteryState();
    if (batteryFlat(battery_)) {
      ui::showLowBattery();
      lv_timer_handler();
      delay(2500);
      board::displayOff();
      board::powerOff();
    }
    pushStatus();
  }

  const int minute = sys::clockValid() ? sys::minuteOfDay() : -1;
  if (minute != lastMinute_) {
    lastMinute_ = minute;
    // The day turned while the screen was on.
    if (today_.valid && sys::clockValid() && sys::utcOffsetKnown() && sys::localDays() != shownDays_) {
      refreshView();
      requestSync(true);
    }
    pushStatus();
  }
}

void Controller::fire(const Due& what) {
  if (what.silence) {
    ui::setSilence(false, 0);
    ui::showSilenceDone();
    lv_display_trigger_activity(nullptr);
    lv_timer_handler();
    ringBell(3);
  } else if (what.reminder >= 0) {
    ui::showReminder(what.reminder);
    lv_display_trigger_activity(nullptr);
    lv_timer_handler();
    ringBell(2);
  } else if (what.bellHour >= 0) {
    ui::showHourBell(what.bellHour);
    lv_display_trigger_activity(nullptr);
    lv_timer_handler();
    ringBell(3);
  }
}

void Controller::ringBell(int strikes) {
  if (!prefs_.sound || !board::audioAvailable()) return;
  // A battery near its end keeps what it has for the screen.
  if (battery_.present && !battery_.onUsb && battery_.percent >= 0 && battery_.percent <= cfg::kBatteryQuietPercent) return;
  board::audioBell(strikes, prefs_.volume);
  lv_display_trigger_activity(nullptr);
}

uint64_t Controller::secondsToNextWake() const {
  const sys::RtcState& r = sys::rtc();
  const int64_t raw = sys::rawSeconds();
  int64_t best = INT64_MAX;
  auto consider = [&](int64_t seconds) {
    if (seconds < 5) seconds = 5;
    if (seconds < best) best = seconds;
  };

  if (r.silenceEndsRaw != 0) consider(r.silenceEndsRaw - raw);

  const bool canSync = linked() && net::wifiConfigured();
  if (sys::clockValid() && sys::utcOffsetKnown()) {
    const int64_t now = sys::nowUtc();
    const int minute = sys::minuteOfDay();
    for (int i = 0; i < 4; i++) {
      const int at = prefs_.reminder[i];
      if (at < 0) continue;
      // Still due today and not rung yet: nextLocalTime() answers tomorrow
      // once the minute has come, so ask for "in a moment" instead.
      if (minute >= at && !(r.remindersFired & (1 << i)) && r.firedDay == static_cast<int32_t>(sys::localDays())) consider(5);
      else consider(sys::nextLocalTime(at) - now);
    }
    if (prefs_.hourBell) {
      for (int hour : {6, 12, 18}) consider(sys::nextLocalTime(hour * 60) - now);
    }
    // A fresh day right after the owner's midnight.
    if (canSync) consider(sys::nextLocalTime(0) - now + cfg::kAfterMidnightSeconds);
  }
  if (canSync && prefs_.syncHours > 0) {
    int64_t at = (r.lastAnswerRaw != 0 ? r.lastAnswerRaw : raw) + static_cast<int64_t>(prefs_.syncHours) * 3600;
    if (at < r.nextTryRaw) at = r.nextTryRaw;
    // Overdue and failing (away from every known network): look again in a
    // while, not at once.
    if (at <= raw) at = raw + 30 * 60;
    consider(at - raw);
  }
  if (best == INT64_MAX) return 0;
  return sys::sleepSecondsFor(best);
}

// ========================================================= input and power

void Controller::handleInput() {
  if (board::powerButtonPressedEdge()) {
    // PWR is the off switch of the screen: straight to sleep.
    goToSleep();
  }

  // BOOT: a tap moves on (or counts a bead); held, it resets a counter or
  // goes home; held for ten seconds, it wipes the device.
  const bool down = board::bootButtonDown();
  if (bootIgnore_) {
    // The press that woke us is not also a command.
    if (!down) bootIgnore_ = false;
  } else if (down) {
    if (bootDownSince_ == 0) {
      bootDownSince_ = millis();
      bootLongSent_ = false;
      lv_display_trigger_activity(nullptr);
    } else if (millis() - bootDownSince_ >= cfg::kFactoryResetHoldMs) {
      factoryReset();
    } else if (!bootLongSent_ && millis() - bootDownSince_ >= cfg::kBootLongPressMs) {
      bootLongSent_ = true;
      ui::onBootLongPress();
    }
  } else if (bootDownSince_ != 0) {
    if (!bootLongSent_ && millis() - bootDownSince_ >= 30) ui::onBootPress();
    bootDownSince_ = 0;
    lv_display_trigger_activity(nullptr);
  }

  board::TouchSwipe swipe;
  if (board::touchTakeSwipe(swipe)) {
    if (ui::onSwipe(swipeOf(swipe))) board::touchCancelPress();
  }
}

bool Controller::deskMode() const { return battery_.onUsb && prefs_.deskAlwaysOn; }

uint8_t Controller::targetBrightness(uint32_t idleMs) const {
  const ui::PowerHint hint = ui::powerHint();
  if (hint.brightness >= 0) {
    // The torch is the one thing a tired battery refuses.
    const bool tired = battery_.present && !battery_.onUsb && battery_.percent >= 0 && battery_.percent <= cfg::kBatteryQuietPercent;
    return static_cast<uint8_t>(tired && hint.brightness > cfg::kBrightness[1] ? cfg::kBrightness[1] : hint.brightness);
  }
  uint8_t level = cfg::kBrightness[prefs_.brightness > 2 ? 1 : prefs_.brightness];
  if (prefs_.nightDim && sys::clockValid() && sys::utcOffsetKnown()) {
    const int minute = sys::minuteOfDay();
    if ((minute >= 22 * 60 || minute < 6 * 60) && level > cfg::kBrightnessNight) level = cfg::kBrightnessNight;
  }
  // On mains nothing switches the screen off, so it rests dim instead.
  if (deskMode() && idleMs > cfg::kDeskDimAfterMs && level > cfg::kBrightnessDesk) level = cfg::kBrightnessDesk;
  return level;
}

void Controller::applyPower() {
  const uint32_t idle = lv_display_get_inactive_time(nullptr);
  board::displaySetBrightness(targetBrightness(idle));
  if (deskMode()) return;

  const ui::PowerHint hint = ui::powerHint();
  uint32_t timeoutMs = (hint.screenSeconds ? hint.screenSeconds : prefs_.screenSeconds) * 1000UL;
  // Someone is reading a Wi-Fi name or a link code off the screen.
  if (flow_ != Flow::Day) timeoutMs = cfg::kSetupAwakeMs;
  if (idle < timeoutMs || millis() - startedMs_ < cfg::kWakeMinimumMs) return;
  // Let a sync that is nearly done finish; it has its own time limit.
  if (syncing_ && idle < timeoutMs + 20000) return;
  goToSleep();
}

void Controller::pushStatus() {
  if (!screenUp_) return;
  ui::Status status;
  status.clockValid = sys::clockValid() && sys::utcOffsetKnown();
  status.minuteOfDay = status.clockValid ? sys::minuteOfDay() : 0;
  status.batteryPercent = battery_.present ? battery_.percent : -1;
  status.charging = battery_.charging;
  status.onUsb = battery_.onUsb;
  status.syncing = syncing_;
  status.linked = linked();
  const sys::RtcState& r = sys::rtc();
  status.syncedMinutesAgo = r.lastAnswerRaw == 0 ? UINT32_MAX : static_cast<uint32_t>((sys::rawSeconds() - r.lastAnswerRaw) / 60);
  ui::setStatus(status);
}

void Controller::goToSleep() {
  sys::RtcState& r = sys::rtc();
  if (screenUp_) r.beads = ui::beadsCount();
  net::portalStop();
  net::wifiOff();
  if (screenUp_) {
    board::displayOff();
    if (!prefs_.touchWakes) board::touchSleep();
  }
  // The panel and the touch controller keep their reset lines high while
  // the processor is gone, so they stay asleep rather than restart.
  board::displayHoldForSleep();
  board::touchHoldForSleep();
  const uint64_t seconds = secondsToNextWake();
  ORDO_LOG("[ordo] sleep for %llu s\n", static_cast<unsigned long long>(seconds));
  board::deepSleep(seconds, prefs_.touchWakes);
}

void Controller::factoryReset() {
  ORDO_LOG("[ordo] factory reset\n");
  ui::showMessage("Apagando", "conta, Wi-Fi e ajustes removidos; reiniciando", true);
  lv_timer_handler();
  settings_.factoryReset();
  net::wifiForgetAll();
  sys::rtc().magic = 0;
  delay(1500);
  esp_restart();
}

ui::Actions Controller::actions() {
  ui::Actions a;
  a.prefsChanged = [this](const ui::Prefs& prefs) {
    prefs_ = prefs;
    settings_.savePrefs(prefs);
  };
  a.sleepNow = [this]() { goToSleep(); };
  a.powerOff = [this]() {
    ui::showMessage("Desligando", "aperte PWR para ligar de novo", false);
    lv_timer_handler();
    delay(1200);
    sys::rtc().beads = ui::beadsCount();
    board::displayOff();
    board::powerOff();
  };
  a.syncNow = [this]() { requestSync(true); };
  a.openWifiPortal = [this]() { enterWifiSetup(); };
  a.forgetNetwork = [this](int index) {
    settings_.removeNetwork(index);
    sys::rtc().channel = 0;
  };
  a.unlink = [this]() {
    settings_.clearLink();
    today_ = model::Today();
    sys::rtc().lastAnswerRaw = 0;
    sys::rtc().blocked = 0;
    ui::clearDay();
    if (net::wifiConfigured()) enterLinking();
    else enterWifiSetup();
  };
  a.factoryReset = [this]() { factoryReset(); };
  a.bell = [this](int strikes) {
    lv_timer_handler();
    ringBell(strikes);
  };
  a.silence = [this](int minutes) {
    sys::RtcState& r = sys::rtc();
    if (minutes > 0) {
      r.silenceEndsRaw = sys::rawSeconds() + static_cast<int64_t>(minutes) * 60;
      ui::setSilence(true, static_cast<uint32_t>(minutes) * 60);
      lv_timer_handler();
      ringBell(1);
    } else {
      r.silenceEndsRaw = 0;
      ui::setSilence(false, 0);
    }
  };
  a.diagnostics = [this]() {
    ui::Diagnostics d;
    d.firmware = ORDO_FIRMWARE_VERSION;
    d.board = ORDO_BOARD_NAME;
    d.deviceName = settings_.deviceName();
    d.batteryPercent = battery_.present ? battery_.percent : -1;
    d.batteryMillivolts = battery_.millivolts;
    d.wakesToday = sys::rtc().wakesToday;
    d.syncsToday = sys::rtc().syncsToday;
    d.clockDriftPpm = sys::clockDriftPpm();
    for (const auto& network : settings_.networks()) d.networks.push_back(network.ssid);
    return d;
  };
  return a;
}

}  // namespace app
