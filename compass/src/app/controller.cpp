#include "controller.h"

#include <Wire.h>
#include <esp_system.h>

#include "../app_config.h"
#include "../board/display.h"
#include "../board/power.h"
#include "../board/touch.h"
#include "../net/wifi_portal.h"
#include "../ui/ui.h"
#include "board_pins.h"

namespace app {
namespace {

inline bool due(uint32_t at) { return static_cast<int32_t>(millis() - at) >= 0; }

}  // namespace

void Controller::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  settings_.begin();

  if (!board::displayInit()) {
    Serial.println("[ordo] display did not answer");
  }
  if (!board::touchInit()) {
    Serial.println("[ordo] touch did not answer");
  }
  if (!board::powerInit()) {
    Serial.println("[ordo] PMU did not answer; running without battery telemetry");
  }

  ui::init();
  ui::setStandbyMinutes(settings_.standbyMinutes());
  ui::setSettingsHandlers({
      [this](uint8_t minutes) { settings_.setStandbyMinutes(minutes); },
      [this]() { enterStandby(); },
  });
  ui::showSplash();
  lv_timer_handler();
  board::displaySetBrightness(cfg::kBrightnessNormal);

  Serial.printf("[ordo] %s firmware %s\n", ORDO_BOARD_NAME, ORDO_FIRMWARE_VERSION);
  showCachedDay();
  enterWifi();
}

void Controller::loop() {
  net::wifiLoop();
  if (standby_) {
    // Screen off: no rendering, but the network keeps the day fresh so
    // waking shows the right thing at once.
    if (board::touchActive()) wake();
  } else {
    lv_timer_handler();
  }
  handleButtons();
  handleBrightness();
  handleBattery();

  switch (state_) {
    case State::Boot:
    case State::Wifi: stepWifi(); break;
    case State::Linking: stepLinking(); break;
    case State::Linked: stepLinked(); break;
    case State::Blocked:
      // A notice is on screen; the API is asked again on the normal schedule.
      if (net::wifiConnected() && due(nextFetchAt_)) fetchToday();
      break;
  }
}

// ------------------------------------------------------------------- Wi-Fi

void Controller::enterWifi() {
  state_ = State::Wifi;
  wifiNoticeShown_ = false;
  portalNoticeShown_ = false;
  net::wifiBegin();
}

void Controller::stepWifi() {
  if (net::wifiConnected()) {
    Serial.println("[ordo] wifi up");
    if (settings_.deviceToken().isEmpty()) {
      enterLinking();
    } else {
      enterLinked();
    }
    return;
  }
  // A cached day stays on screen while the network comes back; a blank
  // compass gets the notice instead.
  if (ui::showingDay()) return;
  if (net::wifiPortalActive()) {
    if (!portalNoticeShown_) {
      ui::showWifiPortal(net::wifiPortalSsid());
      portalNoticeShown_ = true;
    }
  } else if (!wifiNoticeShown_) {
    ui::showMessage("Conectando ao Wi-Fi", "aguarde um instante", true);
    wifiNoticeShown_ = true;
  }
}

// ----------------------------------------------------------------- linking

void Controller::enterLinking() {
  state_ = State::Linking;
  link_ = net::LinkStart();
  linkStartedAt_ = 0;
  nextPollAt_ = millis();
  ui::showMessage("Vincule ao Ordo", "pedindo um código...", true);
}

void Controller::stepLinking() {
  if (!net::wifiConnected()) {
    enterWifi();
    return;
  }
  const uint32_t now = millis();

  if (link_.deviceCode.isEmpty()) {
    if (!due(nextPollAt_)) return;
    if (api_.startLink(link_)) {
      linkStartedAt_ = now;
      pollIntervalMs_ = link_.interval * 1000UL;
      nextPollAt_ = now + pollIntervalMs_;
      ui::showLink(link_.userCode, link_.verificationUriComplete);
      Serial.printf("[ordo] link code %s\n", link_.userCode.c_str());
    } else {
      ui::showMessage("Sem resposta da API", "tentando de novo em instantes", true);
      nextPollAt_ = now + cfg::kRetryMinSeconds * 1000UL;
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
      Serial.println("[ordo] linked");
      ui::showMessage("Vinculado", "buscando o dia de hoje...", true);
      enterLinked();
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
      nextPollAt_ = now + cfg::kRetryMinSeconds * 1000UL;
      break;
  }
}

// ------------------------------------------------------------------ linked

void Controller::enterLinked() {
  state_ = State::Linked;
  retrySeconds_ = 0;
  nextFetchAt_ = millis();
  dayEndsAt_ = 0;
}

void Controller::stepLinked() {
  if (!net::wifiConnected()) {
    offline_ = true;
    enterWifi();
    return;
  }
  if (due(nextFetchAt_) || (dayEndsAt_ != 0 && due(dayEndsAt_))) {
    fetchToday();
  }
}

void Controller::fetchToday() {
  const net::TodayResult result = api_.fetchToday(settings_.deviceToken(), settings_.etag());
  const uint32_t now = millis();
  dayEndsAt_ = 0;

  switch (result.status) {
    case net::TodayStatus::Updated: {
      model::Today parsed;
      if (model::parseToday(result.body, parsed)) {
        today_ = parsed;
        settings_.saveToday(result.etag, result.body);
        offline_ = false;
        retrySeconds_ = 0;
        state_ = State::Linked;
        ui::setToday(today_, false);
        nextFetchAt_ = now + today_.refreshIntervalSeconds * 1000UL;
        if (today_.dayEndsInSeconds > 0) dayEndsAt_ = now + (today_.dayEndsInSeconds + 2) * 1000UL;
        Serial.printf("[ordo] today %s, next in %lus\n", today_.date.c_str(),
                      static_cast<unsigned long>(today_.refreshIntervalSeconds));
      } else {
        Serial.println("[ordo] could not parse today");
        scheduleRetry();
      }
      break;
    }
    case net::TodayStatus::NotModified:
      retrySeconds_ = 0;
      state_ = State::Linked;
      if (offline_ && today_.valid) {
        offline_ = false;
        ui::setToday(today_, false);
      }
      nextFetchAt_ = now + (today_.valid ? today_.refreshIntervalSeconds : cfg::kDefaultRefreshSeconds) * 1000UL;
      if (today_.valid && today_.dayEndsInSeconds > 0) {
        // The payload's countdown was measured when it was fetched; the
        // ETag match means the day has not turned, so refresh at the interval.
      }
      break;
    case net::TodayStatus::Unauthorized:
      Serial.println("[ordo] token refused; linking again");
      settings_.clearLink();
      today_ = model::Today();
      enterLinking();
      break;
    case net::TodayStatus::PremiumRequired:
      state_ = State::Blocked;
      ui::showMessage("Assinatura Ordo+ vencida", "renove no app e a bússola volta sozinha", false);
      nextFetchAt_ = now + cfg::kDefaultRefreshSeconds * 1000UL;
      break;
    case net::TodayStatus::ClientDisabled:
      state_ = State::Blocked;
      ui::showMessage("Aparelho desativado", "este tipo de aparelho foi desativado pelo fabricante", false);
      nextFetchAt_ = now + cfg::kRateLimitedSeconds * 1000UL;
      break;
    case net::TodayStatus::RateLimited:
      nextFetchAt_ = now + cfg::kRateLimitedSeconds * 1000UL;
      break;
    case net::TodayStatus::NetworkError:
    case net::TodayStatus::ServerError:
      offline_ = true;
      if (today_.valid) ui::setToday(today_, true);
      else if (!ui::showingDay()) ui::showMessage("Sem resposta da API", "mostrando quando conseguir", true);
      scheduleRetry();
      break;
  }
}

void Controller::scheduleRetry() {
  retrySeconds_ = retrySeconds_ == 0 ? cfg::kRetryMinSeconds : min(retrySeconds_ * 2, cfg::kRetryMaxSeconds);
  nextFetchAt_ = millis() + retrySeconds_ * 1000UL;
}

// ------------------------------------------------------------ housekeeping

void Controller::showCachedDay() {
  const String cached = settings_.lastPayload();
  if (cached.isEmpty()) return;
  if (model::parseToday(cached, today_)) {
    offline_ = true;
    ui::setToday(today_, true);
  }
}

void Controller::handleButtons() {
  if (board::powerButtonPressedEdge()) {
    // PWR is the screen switch: off when on, on when off.
    if (standby_) wake();
    else enterStandby();
  }
  if (board::bootButtonHeld()) {
    if (bootHeldSince_ == 0) {
      bootHeldSince_ = millis();
      if (standby_) wake();
    } else if (millis() - bootHeldSince_ >= cfg::kFactoryResetHoldMs) {
      factoryReset();
    }
  } else {
    bootHeldSince_ = 0;
  }
}

void Controller::handleBrightness() {
  if (standby_) return;
  const uint32_t idleMs = lv_display_get_inactive_time(nullptr);
  const uint8_t standbyMinutes = settings_.standbyMinutes();

  // The link code and the Wi-Fi notice stay lit: someone is reading them.
  if (standbyMinutes > 0 && ui::showingDay() && idleMs > standbyMinutes * 60UL * 1000UL) {
    enterStandby();
    return;
  }
  const bool idle = idleMs > cfg::kDimAfterMs;
  if (idle && !dimmed_) {
    board::displaySetBrightness(cfg::kBrightnessDim);
    dimmed_ = true;
  } else if (!idle && dimmed_) {
    board::displaySetBrightness(cfg::kBrightnessNormal);
    dimmed_ = false;
  }
}

void Controller::enterStandby() {
  if (standby_) return;
  standby_ = true;
  dimmed_ = false;
  board::displayOff();
  setCpuFrequencyMhz(cfg::kStandbyCpuMhz);
  Serial.println("[ordo] standby");
}

void Controller::wake() {
  if (!standby_) return;
  standby_ = false;
  setCpuFrequencyMhz(cfg::kActiveCpuMhz);
  lv_display_trigger_activity(nullptr);
  // Everything LVGL skipped while the panel slept is drawn before it lights up.
  lv_obj_invalidate(lv_screen_active());
  lv_timer_handler();
  board::displayOn(cfg::kBrightnessNormal);
  Serial.println("[ordo] wake");
}

void Controller::handleBattery() {
  if (millis() - lastBatteryAt_ < 30 * 1000UL) return;
  lastBatteryAt_ = millis();
  const board::BatteryState battery = board::batteryState();
  ui::setBattery(battery.present ? battery.percent : -1, battery.charging);
}

void Controller::factoryReset() {
  Serial.println("[ordo] factory reset");
  ui::showMessage("Apagando", "Wi-Fi e vínculo removidos; reiniciando", true);
  lv_timer_handler();
  settings_.factoryReset();
  net::wifiForget();
  delay(1500);
  esp_restart();
}

}  // namespace app
