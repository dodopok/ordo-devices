#include "controller.h"

#include <Wire.h>
#include <esp_system.h>

#include <esp_heap_caps.h>

#include "../app_config.h"
#include "../board/audio.h"
#include "../content/art.h"
#include "../board/display.h"
#include "../board/touch.h"
#include "../sys/clock.h"
#include "../sys/rtc_state.h"
#include "../sys/wake_trail.h"
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

ui::Swipe swipeOf(board::TouchSwipe swipe) {
  switch (swipe) {
    case board::TouchSwipe::Up: return ui::Swipe::Up;
    case board::TouchSwipe::Down: return ui::Swipe::Down;
    case board::TouchSwipe::Left: return ui::Swipe::Left;
    default: return ui::Swipe::Right;
  }
}

sys::WakeCause causeOf(board::Wake wake) {
  switch (wake) {
    case board::Wake::Timer: return sys::WakeCause::Timer;
    case board::Wake::Touch: return sys::WakeCause::Touch;
    case board::Wake::PowerButton:
    case board::Wake::BootButton: return sys::WakeCause::Button;
    default: return sys::WakeCause::PowerOn;
  }
}

// The processor restarted by itself: something went wrong before this boot.
bool restartedByItself() {
  switch (esp_reset_reason()) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_BROWNOUT:
      return true;
    default:
      return false;
  }
}

uint8_t dueBits(const Due& what) {
  uint8_t bits = 0;
  if (what.reminder >= 0 && what.reminder < 4) bits |= static_cast<uint8_t>(sys::kDueMorning << what.reminder);
  if (what.streak) bits |= sys::kDueStreak;
  if (what.bellHour >= 0) bits |= sys::kDueBell;
  if (what.silence) bits |= sys::kDueSilence;
  if (what.test) bits |= sys::kDueTest;
  return bits;
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
  // A line for the "Sobre" screen about this wake, written as it goes: if
  // it stops half way, the line says where.
  sys::trailBegin();
  sys::trailOpen(sys::clockValid() && sys::utcOffsetKnown() ? static_cast<int16_t>(sys::minuteOfDay()) : static_cast<int16_t>(-1),
                 causeOf(wake_), restartedByItself());

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

  if (pending_ != Notice::None) note(sys::WakeStep::Lighting);
  startScreen();
  // After an update this is the new firmware's first boot: it got as far as
  // its screen, so the bootloader may stop keeping the old one in reserve.
  net::otaConfirmRunning();
  // What the site offered at an earlier wake is still on offer.
  if (model::versionNewer(r.updateVersion, ORDO_FIRMWARE_VERSION)) {
    update_ = ui::UpdateState::Available;
    offer_.version = String(r.updateVersion);
  } else {
    r.updateVersion[0] = '\0';
  }
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

// A wake by the timer. True when something has to ring, which the caller
// then does with the screen up.
//
// What rings, rings from what is on record: nothing between the wake and
// the bell waits for a network. The record is kept fresh by the wakes that
// come without anything to ring: every few hours, after midnight, and a few
// minutes before each reminder (app::kPrepMinutes), which also sets the
// clock so the bell falls on its minute.
bool Controller::background() {
  const Due what = takeDue();
  noteDue(what);
  if (what.silence) {
    pending_ = Notice::SilenceDone;
  } else if (what.test) {
    pending_ = Notice::Test;
  } else if (what.reminder >= 0 && !alreadyPrayed(what.reminder)) {
    pending_ = Notice::Reminder;
    pendingValue_ = what.reminder;
  } else if (what.streak && nothingPrayedToday()) {
    // The app's evening nudge, for a day with nothing prayed. An office's
    // own reminder at the same minute (above) says it better.
    pending_ = Notice::Streak;
  } else if (what.bellHour >= 0) {
    pending_ = Notice::Bell;
    pendingValue_ = what.bellHour;
  }
  if (pending_ != Notice::None) return true;
  // A reminder for an office already prayed: said in the trail, not rung.
  skipped_ = what.reminder >= 0 || what.streak;

  const bool dayTurned = today_.valid && sys::clockValid() && sys::utcOffsetKnown() && sys::localDays() != today_.day.days;
  const bool periodic = prefs_.syncHours > 0 && stale(prefs_.syncHours * 3600UL - 300);
  const bool beforeReminder = prefs_.syncHours > 0 && app::reminderWithin(moment(), plan(), app::kPrepMinutes + 2) && stale(5 * 60);
  if (linked() && net::wifiConfigured() && (dayTurned || periodic || beforeReminder)) syncBounded(cfg::kBackgroundSyncMs);
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
    if (pending_ == Notice::Streak) what.streak = true;
    if (pending_ == Notice::Bell) what.bellHour = pendingValue_;
    if (pending_ == Notice::SilenceDone) what.silence = true;
    if (pending_ == Notice::Test) what.test = true;
    pending_ = Notice::None;
    // The notice is what this wake is for: it is drawn before the panel
    // lights up, and the bell waits until it can be read.
    fire(what);
    return;
  }

  // Everything LVGL has to draw is drawn before the panel lights up. With
  // animations on, the screens bring the light up themselves (ui::transition).
  lv_timer_handler();
  applyBrightness();
  setCpuFrequencyMhz(cfg::kCpuScreenMhz);
  note(sys::WakeStep::Shown);
}

// =================================================================== loop

void Controller::loop() {
  net::wifiLoop();
  lv_timer_handler();
  // A firmware being written is the only thing going on.
  if (stepInstall()) return;
  handleInput();
  if (later_) {
    const std::function<void()> job = later_;
    later_ = nullptr;
    job();
  }

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
  // A request under way is waited for whatever else happens; it has its own
  // time limit (cfg::kHttpTimeoutMs).
  if (fetch_.running) {
    if (!fetch_.done) return;
    fetch_.running = false;
    const net::TodayResult result = fetch_.result;
    fetch_.result = net::TodayResult();
    // Unlinked while it was in the air: the answer is nobody's.
    if (linked()) applyToday(result);
    // The radio is still up: fetch what the day's celebrations lack.
    if (syncing_ && linked() && flow_ == Flow::Day && net::wifiConnected() && startContent()) return;
    if (syncing_) finishSync();
    return;
  }
  if (content_.running) {
    drainContent();
    if (!content_.done) return;
    drainContent();
    content_.running = false;
    content_.jobs.clear();
    content_.results.clear();
    pruneContent();
    if (syncing_) finishSync();
    return;
  }
  if (!syncing_) return;
  switch (net::wifiState()) {
    case net::WifiState::Connected:
      if (startFetch()) break;
      // No memory for the task: ask from here, with the screen settled first.
      if (screenUp_) settleScreen();
      fetchToday();
      finishSync();
      break;
    case net::WifiState::Connecting:
      if (millis() - syncStartedMs_ > 30000) {
        noteNet(sys::WakeNet::NoWifi);
        syncFailed();
        finishSync();
      }
      break;
    default:
      // No known network in range: nothing to do until a later look.
      noteNet(sys::WakeNet::NoWifi);
      syncFailed();
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

// A sync with nobody watching. It goes through the very steps a sync with
// the screen on does (stepSync: the request on a task of its own, then the
// pictures the days ahead still lack), driven from here until it ends or
// `budgetMs` runs out. True when it ended by itself.
bool Controller::syncBounded(uint32_t budgetMs) {
  note(sys::WakeStep::Syncing);
  syncing_ = true;
  syncStartedMs_ = millis();
  setCpuFrequencyMhz(cfg::kCpuRadioMhz);
  net::wifiConnect();
  const uint32_t start = millis();
  while (syncing_ && millis() - start < budgetMs) {
    net::wifiLoop();
    stepSync();
    delay(15);
  }
  if (!syncing_) return true;
  // Out of time. With no answer yet, it counts as a sync that failed, so the
  // next try waits its turn on the ladder instead of coming in a minute.
  // The tasks still talking to the network go when the processor sleeps.
  if (!content_.running) {
    noteNet(sys::WakeNet::TimedOut);
    syncFailed();
  }
  syncing_ = false;
  return false;
}

// ===================================================== pictures and texts

// What today and the days ahead still lack, today first. Art before text:
// a celebration without its text still has a name, one without its picture
// has an empty page.
std::vector<Controller::ContentJob> Controller::wantedContent(int limit) {
  std::vector<ContentJob> jobs;
  if (!today_.valid) return jobs;
  const model::Today view = currentView();
  auto absent = [this](const String& slug, store::ContentKind kind) {
    const String key = slug + (kind == store::ContentKind::Art ? ".jpg" : ".json");
    for (const String& gone : missing_) {
      if (gone == key) return false;
    }
    return !store::contentHas(slug, kind);
  };
  auto consider = [&](const model::DayInfo& day) {
    const model::Celebration& c = day.celebration;
    if (c.slug.isEmpty()) return;
    if (static_cast<int>(jobs.size()) < limit && !c.artPath.isEmpty() && absent(c.slug, store::ContentKind::Art)) {
      ContentJob job;
      job.slug = c.slug;
      job.path = c.artPath;
      job.kind = store::ContentKind::Art;
      jobs.push_back(job);
    }
    if (static_cast<int>(jobs.size()) < limit && !c.textPath.isEmpty() && absent(c.slug, store::ContentKind::Text)) {
      ContentJob job;
      job.slug = c.slug;
      job.path = c.textPath;
      job.kind = store::ContentKind::Text;
      jobs.push_back(job);
    }
  };
  consider(view.day);
  for (const model::DayInfo& day : view.upcoming) consider(day);
  return jobs;
}

bool Controller::startContent() {
  content_.jobs = wantedContent(battery_.onUsb ? cfg::kContentPerSyncMains : cfg::kContentPerSyncBattery);
  // The same connection asks, once a day, whether there is a newer firmware:
  // with the screen on or on mains, not on a wake nobody is watching.
  if (updateCheckDue() && (screenUp_ || battery_.onUsb)) {
    ContentJob job;
    job.manifest = true;
    content_.jobs.push_back(job);
  }
  if (content_.jobs.empty()) return false;
  content_.results.clear();
  content_.results.resize(content_.jobs.size());
  content_.produced = 0;
  content_.consumed = 0;
  content_.done = false;
  if (xTaskCreatePinnedToCore(contentTask, "ordo-content", 16384, this, 1, nullptr, 0) != pdPASS) {
    content_.jobs.clear();
    content_.results.clear();
    return false;
  }
  content_.running = true;
  return true;
}

// Runs beside the main loop. It touches the network and its own slot of
// `results`, nothing else; the main loop takes it from there.
void Controller::contentTask(void* self) {
  Controller* c = static_cast<Controller*>(self);
  {
    net::Site site;
    for (size_t i = 0; i < c->content_.jobs.size(); i++) {
      const ContentJob& job = c->content_.jobs[i];
      ContentResult& result = c->content_.results[i];
      if (job.manifest) {
        result.status = site.get("/firmware/manifest.json", result.body, cfg::kManifestMaxBytes);
        if (result.status < 0) result.tlsError = site.lastTlsError();
      } else {
        result.status = site.get(job.path, result.body, job.kind == store::ContentKind::Art ? cfg::kArtMaxBytes : cfg::kTextMaxBytes);
        if (result.status < 0) result.tlsError = site.lastTlsError();
      }
      c->content_.produced = static_cast<int>(i) + 1;
      // The site did not answer at all: the rest would go the same way.
      if (result.status < 0 && result.status != net::kSiteBodyRefused) break;
    }
    site.close();
  }
  c->content_.done = true;
  vTaskDelete(nullptr);
}

void Controller::drainContent() {
  while (content_.consumed < content_.produced.load()) {
    const size_t i = static_cast<size_t>(content_.consumed++);
    keepContent(content_.jobs[i], content_.results[i]);
    content_.results[i].body = std::vector<uint8_t>();
  }
}

void Controller::keepContent(const ContentJob& job, ContentResult& result) {
  if (job.manifest) {
    model::FirmwareOffer offer;
    net::OtaCheck check = net::OtaCheck::Failed;
    String detail;
    if (result.status != 200 || result.body.empty()) {
      detail = net::siteFailure(result.status, result.tlsError);
    } else if (!model::parseManifest(reinterpret_cast<const char*>(result.body.data()), result.body.size(), ORDO_BOARD_KEY, today_.language,
                                     offer)) {
      detail = "manifesto sem " ORDO_BOARD_KEY;
    } else {
      const bool newer = model::versionNewer(offer.version.c_str(), ORDO_FIRMWARE_VERSION) &&
                         !(net::otaRequiresSignature() && offer.signature.isEmpty());
      check = newer ? net::OtaCheck::Available : net::OtaCheck::UpToDate;
    }
    rememberOffer(check, offer, detail);
    return;
  }
  const bool art = job.kind == store::ContentKind::Art;
  if (result.status == 200 && result.body.size() > 16) {
    // A JPEG begins FF D8; the text is a JSON object.
    const bool looksRight = art ? (result.body[0] == 0xFF && result.body[1] == 0xD8) : result.body[0] == '{';
    if (looksRight && store::contentWrite(job.slug, job.kind, result.body.data(), result.body.size())) {
      if (art && artSlug_ == job.slug) artSlug_ = String();
      if (screenUp_) ui::contentArrived(job.slug);
    }
    siteProblem_ = String();
  } else if (result.status == 404) {
    // No picture in the CMS, or one too small to show: the page is set in
    // type, and the site is not asked again until the next wake.
    missing_.push_back(job.slug + (art ? ".jpg" : ".json"));
  } else {
    // Anything else is worth knowing about: it shows on the "Sobre" screen.
    siteProblem_ = net::siteFailure(result.status, result.tlsError);
  }
}

// The days that went by take their pictures with them.
void Controller::pruneContent() {
  if (!today_.valid) return;
  std::vector<String> keep;
  if (!today_.day.celebration.slug.isEmpty()) keep.push_back(today_.day.celebration.slug);
  for (const model::DayInfo& day : today_.upcoming) {
    if (!day.celebration.slug.isEmpty()) keep.push_back(day.celebration.slug);
  }
  store::contentPrune(keep);
}

ui::Art Controller::artFor(const String& slug) {
  ui::Art art;
  if (slug.isEmpty()) return art;
  if (artSlug_ != slug || !artPixels_) {
    size_t length = 0;
    uint8_t* jpeg = store::contentRead(slug, store::ContentKind::Art, length);
    if (!jpeg) return art;
    if (!artPixels_) {
      artPixels_ = static_cast<uint16_t*>(
          heap_caps_malloc(static_cast<size_t>(content::kArtSide) * content::kArtSide * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    const bool decoded = artPixels_ && content::decodeArt(jpeg, length, artPixels_);
    free(jpeg);
    if (!decoded) {
      // A file that does not decode is of no use: fetched again next sync.
      if (artPixels_) store::contentRemove(slug, store::ContentKind::Art);
      artSlug_ = String();
      return art;
    }
    artSlug_ = slug;
  }
  art.pixels = reinterpret_cast<const uint8_t*>(artPixels_);
  art.side = content::kArtSide;
  return art;
}

model::CelebrationText Controller::textFor(const String& slug) {
  model::CelebrationText text;
  size_t length = 0;
  uint8_t* json = store::contentRead(slug, store::ContentKind::Text, length);
  if (!json) return text;
  if (!model::parseCelebrationText(reinterpret_cast<const char*>(json), length, text)) {
    store::contentRemove(slug, store::ContentKind::Text);
  }
  free(json);
  return text;
}

// ======================================================= firmware updates

bool Controller::updateCheckDue() const {
  const sys::RtcState& r = sys::rtc();
  return r.updateCheckedRaw == 0 || sys::rawSeconds() - r.updateCheckedRaw >= cfg::kUpdateCheckEverySeconds;
}

// True unless the compass knows an office was prayed today. A day it rolled
// to without a network counts as nothing prayed: better a nudge too many.
bool Controller::nothingPrayedToday() const {
  const model::Today view = currentView();
  return !view.officesKnown || view.completedToday == 0;
}

void Controller::rememberOffer(net::OtaCheck result, const model::FirmwareOffer& offer, const String& detail) {
  sys::RtcState& r = sys::rtc();
  if (result != net::OtaCheck::Failed) updateDetail_ = String();
  switch (result) {
    case net::OtaCheck::Available:
      update_ = ui::UpdateState::Available;
      offer_ = offer;
      strlcpy(r.updateVersion, offer.version.c_str(), sizeof(r.updateVersion));
      r.updateCheckedRaw = sys::rawSeconds();
      break;
    case net::OtaCheck::UpToDate:
      update_ = ui::UpdateState::UpToDate;
      offer_ = model::FirmwareOffer();
      r.updateVersion[0] = '\0';
      r.updateCheckedRaw = sys::rawSeconds();
      break;
    case net::OtaCheck::Failed:
      // What was known before stands; only someone who asked is told.
      updateDetail_ = detail;
      if (update_ == ui::UpdateState::Checking) update_ = ui::UpdateState::Failed;
      break;
  }
  if (screenUp_) pushStatus();
}

// Brings the Wi-Fi up for something the owner asked for and is waiting on,
// keeping the screen alive meanwhile.
bool Controller::connectForSite() {
  setCpuFrequencyMhz(cfg::kCpuRadioMhz);
  if (!net::wifiConnected()) net::wifiConnect();
  const uint32_t start = millis();
  while (!net::wifiConnected() && net::wifiState() == net::WifiState::Connecting && millis() - start < 20000) {
    net::wifiLoop();
    lv_timer_handler();
    delay(10);
  }
  return net::wifiConnected();
}

void Controller::checkUpdateNow() {
  // One conversation with the network at a time: wait for a sync to end.
  if (fetch_.running || content_.running) {
    later_ = [this]() { checkUpdateNow(); };
    return;
  }
  update_ = ui::UpdateState::Checking;
  pushStatus();
  settleScreen();
  lv_display_trigger_activity(nullptr);
  model::FirmwareOffer offer;
  net::OtaCheck result = net::OtaCheck::Failed;
  String detail = "sem Wi-Fi";
  if (connectForSite()) {
    net::Site site;
    result = net::otaCheck(site, today_.language, offer, &detail);
    site.close();
  }
  rememberOffer(result, offer, detail);
  if (!syncing_ && !deskMode() && flow_ == Flow::Day) {
    net::wifiOff();
    setCpuFrequencyMhz(cfg::kCpuScreenMhz);
  }
  lv_display_trigger_activity(nullptr);
  pushStatus();
}

void Controller::installUpdateNow() {
  if (fetch_.running || content_.running) {
    later_ = [this]() { installUpdateNow(); };
    return;
  }
  // A battery that gives out half way leaves the old firmware in place, but
  // there is no reason to try.
  const bool enough = battery_.onUsb || !battery_.present || battery_.percent < 0 || battery_.percent >= cfg::kUpdateMinBatteryPercent;
  if (install_.running || !enough || update_ != ui::UpdateState::Available) return;

  install_.version = offer_.version;
  install_.language = today_.language;
  install_.percent = 0;
  install_.shown = -1;
  install_.done = false;
  install_.result = net::OtaResult::Unreachable;
  ui::showUpdating(install_.version, 0);
  lv_timer_handler();
  applyBrightness();

  if (!connectForSite() || xTaskCreatePinnedToCore(installTask, "ordo-update", 24576, this, 1, nullptr, 0) != pdPASS) {
    install_.done = true;
  }
  install_.running = true;
}

void Controller::installTask(void* self) {
  Controller* c = static_cast<Controller*>(self);
  {
    net::Site site;
    // The manifest again, now: the offer kept from an earlier wake has no
    // hash, and the site may have moved on since.
    model::FirmwareOffer offer;
    if (net::otaCheck(site, c->install_.language, offer) == net::OtaCheck::Available) {
      c->install_.result = net::otaInstall(site, offer, [c](int percent) { c->install_.percent = percent; });
    } else {
      c->install_.result = net::OtaResult::BadImage;
    }
    site.close();
  }
  c->install_.done = true;
  vTaskDelete(nullptr);
}

// True while a firmware is being written: the loop then does nothing else.
bool Controller::stepInstall() {
  if (!install_.running) return false;
  lv_display_trigger_activity(nullptr);
  const int percent = install_.percent.load();
  if (percent != install_.shown) {
    install_.shown = percent;
    ui::showUpdating(install_.version, percent);
  }
  applyBrightness();
  if (!install_.done) return true;
  install_.running = false;

  if (install_.result == net::OtaResult::Installed) {
    // The new firmware may lay this memory out differently: it starts from
    // nothing, and gets the time back at its first sync.
    sys::rtc().magic = 0;
    ui::showMessage("Atualizada", "reiniciando na versão nova", true);
    lv_timer_handler();
    delay(1500);
    net::wifiOff();
    esp_restart();
  }

  const char* why = install_.result == net::OtaResult::Unreachable ? "sem rede; tente perto do Wi-Fi"
                    : (install_.result == net::OtaResult::NoRoom ? "sem espaço para gravar" : "o arquivo não conferiu; nada foi mudado");
  ui::showMessage("Não atualizou", why, false);
  lv_timer_handler();
  delay(3500);
  if (!syncing_ && !deskMode()) {
    net::wifiOff();
    setCpuFrequencyMhz(cfg::kCpuScreenMhz);
  }
  lv_display_trigger_activity(nullptr);
  if (today_.valid) ui::goHome();
  return false;
}

bool Controller::startFetch() {
  fetch_.token = settings_.deviceToken();
  fetch_.etag = settings_.etag();
  fetch_.result = net::TodayResult();
  fetch_.done = false;
  // TLS wants a deep stack; the radio's own tasks live on core 0 too.
  if (xTaskCreatePinnedToCore(fetchTask, "ordo-fetch", 16384, this, 1, nullptr, 0) != pdPASS) return false;
  fetch_.running = true;
  return true;
}

void Controller::fetchTask(void* self) {
  Controller* c = static_cast<Controller*>(self);
  c->fetch_.result = c->api_.fetchToday(c->fetch_.token, c->fetch_.etag);
  c->fetch_.done = true;
  vTaskDelete(nullptr);
}

void Controller::fetchToday() { applyToday(api_.fetchToday(settings_.deviceToken(), settings_.etag())); }

void Controller::applyToday(const net::TodayResult& result) {
  sys::RtcState& r = sys::rtc();
  const int64_t raw = sys::rawSeconds();
  // Every answer, whatever it says, carries the time.
  const bool answered = result.status != net::TodayStatus::NetworkError;
  if (answered && api_.serverTime() > 0) sys::clockSet(api_.serverTime());
  if (answered) r.syncFailures = 0;
  noteNet(answered ? sys::WakeNet::Answered : sys::WakeNet::NoAnswer);

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
        sys::setUtcOffset(sys::offsetFromDayEnd(sys::nowUtc(), parsed.dayEndsInSeconds));
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
      syncFailed();
      break;
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

Moment Controller::moment() const {
  Moment now;
  now.raw = sys::rawSeconds();
  now.clockValid = sys::clockValid() && sys::utcOffsetKnown();
  now.utc = sys::nowUtc();
  now.utcOffset = sys::utcOffset();
  return now;
}

Plan Controller::plan() const {
  Plan p;
  // The times set in the app, when the owner follows the app and it has any.
  const bool fromApp = prefs_.remindersFromApp && today_.valid && today_.hasReminders;
  for (int i = 0; i < 4; i++) p.reminder[i] = fromApp ? today_.reminders[i] : prefs_.reminder[i];
  // The streak reminder exists only in the app: followed whenever the owner
  // follows the app, whether or not any office has a time there.
  if (prefs_.remindersFromApp && today_.valid) p.streakReminder = today_.streakRemindAt;
  p.hourBell = prefs_.hourBell;
  p.syncHours = prefs_.syncHours;
  p.canSync = linked() && net::wifiConfigured();
  return p;
}

Due Controller::takeDue() { return app::takeDue(moment(), plan(), sys::rtc()); }

// True when the compass knows that office was prayed today.
bool Controller::alreadyPrayed(int office) const {
  const model::Today view = currentView();
  return office >= 0 && office < 4 && view.officesKnown && view.offices[office].done;
}

// The line of this wake on the "Sobre" screen (sys/wake_trail.h): how far
// it got, what was due, what the network said.
void Controller::note(sys::WakeStep step) {
  sys::WakeNote& line = sys::trailNow();
  line.step = step;
  const uint32_t seconds = (millis() - startedMs_) / 1000;
  line.seconds = static_cast<uint8_t>(seconds > 255 ? 255 : seconds);
  sys::trailSeal();
}

void Controller::noteNet(sys::WakeNet net) {
  sys::trailNow().net = net;
  sys::trailSeal();
}

void Controller::noteDue(const Due& what) {
  const uint8_t bits = dueBits(what);
  if (bits == 0) return;
  sys::trailNow().due |= bits;
  sys::trailSeal();
}

// A sync that got no answer: wait longer each time before the next one.
void Controller::syncFailed() {
  sys::RtcState& r = sys::rtc();
  if (r.syncFailures < 250) r.syncFailures++;
  r.nextTryRaw = sys::rawSeconds() + retryDelay(r.syncFailures);
}

void Controller::everySecond() {
  // A time of silence counting down on its screen.
  sys::RtcState& r = sys::rtc();
  if (r.silenceEndsRaw != 0) {
    const int64_t left = r.silenceEndsRaw - sys::rawSeconds();
    ui::setSilence(left > 0, left > 0 ? static_cast<uint32_t>(left) : 0);
  }

  Due what = takeDue();
  if (what.reminder >= 0 && alreadyPrayed(what.reminder)) what.reminder = -1;
  if (what.streak && !nothingPrayedToday()) what.streak = false;
  if (what.any()) fire(what);

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
  int strikes = 0;
  noteDue(what);
  note(sys::WakeStep::Lighting);
  if (what.silence) {
    ui::setSilence(false, 0);
    ui::showSilenceDone();
    strikes = 3;
  } else if (what.test) {
    ui::showReminderTest();
    strikes = 2;
  } else if (what.reminder >= 0) {
    ui::showReminder(what.reminder);
    strikes = 2;
  } else if (what.streak) {
    ui::showStreakReminder();
    strikes = 2;
  } else if (what.bellHour >= 0) {
    ui::showHourBell(what.bellHour);
    strikes = 3;
  }
  if (strikes == 0) return;
  lv_display_trigger_activity(nullptr);
  // Drawn and lit before the bell, which holds everything else for seconds.
  lv_timer_handler();
  applyBrightness();
  ringBell(strikes);
  note(sys::WakeStep::Rang);
}

void Controller::ringBell(int strikes) {
  if (!prefs_.sound || !board::audioAvailable()) return;
  // A battery near its end keeps what it has for the screen.
  if (battery_.present && !battery_.onUsb && battery_.percent >= 0 && battery_.percent <= cfg::kBatteryQuietPercent) return;
  board::audioBell(strikes, prefs_.volume);
  lv_display_trigger_activity(nullptr);
}

uint64_t Controller::secondsToNextWake() const {
  const int64_t seconds = app::secondsToNextWake(moment(), plan(), sys::rtc());
  return seconds <= 0 ? 0 : sys::sleepSecondsFor(seconds);
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

// The owner's brightness, the hour and the desk decide how bright; the
// screens scale that down to nothing and back while one replaces another.
void Controller::applyBrightness() {
  if (!screenUp_) return;
  const uint32_t level = targetBrightness(lv_display_get_inactive_time(nullptr));
  board::displaySetBrightness(static_cast<uint8_t>(level * ui::brightnessScale() / 255));
}

// Lets a screen change that is under way finish, and gives a short effect
// `atLeastMs` to play, before something that blocks: the bell holds the
// processor for seconds, and the glass would stay frozen half lit.
void Controller::settleScreen(uint32_t atLeastMs) {
  const uint32_t started = millis();
  for (;;) {
    lv_timer_handler();
    const uint32_t spent = millis() - started;
    if (spent >= 900 || (spent >= atLeastMs && !ui::changingScreen())) break;
    delay(5);
  }
  applyBrightness();
}

void Controller::applyPower() {
  const uint32_t idle = lv_display_get_inactive_time(nullptr);
  applyBrightness();
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
  // The radio is off nearly all the time, so the mark tells of the last
  // time it was on: how strong the network was, or that none answered.
  if (!net::wifiConfigured()) status.wifiBars = -1;
  else if (r.syncFailures > 0 || r.rssi == 0) status.wifiBars = 0;
  else status.wifiBars = r.rssi >= -62 ? 3 : (r.rssi >= -75 ? 2 : 1);
  status.update = update_;
  status.updateVersion = offer_.version;
  status.updateNotes = offer_.notes;
  status.updateDetail = updateDetail_;
  ui::setStatus(status);
}

void Controller::goToSleep() {
  sys::RtcState& r = sys::rtc();
  // A wake that never lit the screen ends here, and says so.
  if (!screenUp_) note(skipped_ ? sys::WakeStep::Skipped : sys::WakeStep::Slept);
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
  store::contentWipe();
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
  // The ones below block or draw a last message, so they run from the main
  // loop (see `later_`), not from the tap that asked.
  a.powerOff = [this]() {
    later_ = [this]() {
      ui::showMessage("Desligando", "aperte PWR para ligar de novo", false);
      lv_timer_handler();
      delay(1200);
      sys::rtc().beads = ui::beadsCount();
      board::displayOff();
      board::powerOff();
    };
  };
  a.syncNow = [this]() { requestSync(true); };
  a.openWifiPortal = [this]() { enterWifiSetup(); };
  a.forgetNetwork = [this](int index) {
    settings_.removeNetwork(index);
    sys::rtc().channel = 0;
  };
  a.art = [this](const String& slug) { return artFor(slug); };
  a.celebrationText = [this](const String& slug) { return textFor(slug); };
  a.checkUpdate = [this]() { later_ = [this]() { checkUpdateNow(); }; };
  a.installUpdate = [this]() { later_ = [this]() { installUpdateNow(); }; };
  a.unlink = [this]() {
    settings_.clearLink();
    store::contentWipe();
    today_ = model::Today();
    sys::rtc().lastAnswerRaw = 0;
    sys::rtc().blocked = 0;
    ui::clearDay();
    if (net::wifiConfigured()) enterLinking();
    else enterWifiSetup();
  };
  a.factoryReset = [this]() { later_ = [this]() { factoryReset(); }; };
  a.bell = [this](int strikes) {
    later_ = [this, strikes]() {
      // The last bead lights up and its ring opens before the bell holds things.
      settleScreen(prefs_.animations ? 450 : 0);
      ringBell(strikes);
    };
  };
  a.brightnessMoved = [this]() { applyBrightness(); };
  a.stageBegin = []() { return board::displayStageBegin(); };
  a.stageReveal = [](ui::Wipe wipe) { board::displayStageReveal(wipe); };
  // A reminder in a minute, with the compass asleep meanwhile: the way to
  // see, without waiting for the evening, that it wakes by itself and rings.
  a.testReminder = [this]() {
    later_ = [this]() {
      sys::rtc().testAtRaw = sys::rawSeconds() + cfg::kTestReminderSeconds;
      ui::showMessage("Teste do aviso", "dormindo agora; em um minuto ela acorda sozinha e toca", false);
      lv_timer_handler();
      applyBrightness();
      delay(2500);
      goToSleep();
    };
  };
  a.silence = [this](int minutes) {
    sys::RtcState& r = sys::rtc();
    if (minutes > 0) {
      r.silenceEndsRaw = sys::rawSeconds() + static_cast<int64_t>(minutes) * 60;
      ui::setSilence(true, static_cast<uint32_t>(minutes) * 60);
      // The candle is on the glass before the first bell.
      later_ = [this]() {
        settleScreen();
        ringBell(1);
      };
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
    d.siteProblem = siteProblem_;
    d.freeMemoryKb = static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
    d.lowestMemoryKb = static_cast<uint32_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024);
    // When it would wake by itself if put to sleep now.
    const int64_t ahead = app::secondsToNextWake(moment(), plan(), sys::rtc());
    if (ahead > 0 && sys::clockValid() && sys::utcOffsetKnown()) {
      const int64_t minutes = sys::minuteOfDay() + (ahead + 30) / 60;
      char at[24];
      if (minutes >= 2 * 1440) snprintf(at, sizeof(at), "em %lld dias", static_cast<long long>(minutes / 1440));
      else snprintf(at, sizeof(at), "%02d:%02d%s", static_cast<int>((minutes / 60) % 24), static_cast<int>(minutes % 60), minutes >= 1440 ? " de amanhã" : "");
      d.nextWake = String(at);
    }
    for (const auto& network : settings_.networks()) d.networks.push_back(network.ssid);
    for (int i = 0; i < sys::trailCount(); i++) d.wakes.push_back(sys::trailAt(i));
    return d;
  };
  return a;
}

}  // namespace app
