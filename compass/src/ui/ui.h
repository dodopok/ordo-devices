#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include <functional>
#include <vector>

#include "../model/content.h"
#include "../model/today.h"
#include "../sys/wake_trail.h"
#include "wipe.h"

// The screens. Nothing here touches the board or the network: the controller
// hands in the day, the clock and the battery, and gets back what the owner
// asked for through `Actions`. That is also what lets sim/ draw every screen
// on a computer.
namespace ui {

// The carousel, in order. It wraps: after Menu comes Now again. Celebration
// is there only on a day that commemorates someone or something.
enum class Page { Now, Celebration, Compass, Readings, Offices, Week, Menu };

// What the owner can change on the settings screens. The controller keeps it
// in NVS; the screens edit a copy and report every change.
struct Prefs {
  uint8_t screenSeconds = 15;  // 10, 15, 30, 60
  uint8_t brightness = 1;      // 0 low, 1 medium, 2 high
  bool nightDim = true;        // dimmer between 22h and 6h
  uint8_t homeFace = 0;        // the screen a wake opens on: 0 Agora, 1 Bússola, 2 Leituras, 3 Ofícios, 4 Semana
  bool touchWakes = false;     // off: only the buttons wake the screen
  bool deskAlwaysOn = true;    // on USB power the screen stays on
  bool sound = true;
  uint8_t volume = 1;          // 0 low, 1 medium, 2 high
  bool hourBell = false;       // a bell at 6h, 12h and 18h
  int16_t reminder[4] = {-1, -1, -1, -1};  // minute of the day per office, -1: off
  // Ring at the times set in the app instead, whenever the account has any.
  bool remindersFromApp = true;
  uint8_t syncHours = 4;       // 0: only when woken; else every 2, 4, 6 or 12 hours
  uint8_t beadsMode = 0;       // 0 Anglican rosary (33), 1 rope of 33, 2 of 50, 3 of 100
  bool animations = true;      // a new screen sweeps over the old one; faces play an entrance
};

// Where the search for a newer firmware stands.
enum class UpdateState { Unknown, Checking, UpToDate, Available, Failed };

// What changes by itself: the clock, the battery, the radio.
struct Status {
  bool clockValid = false;
  int minuteOfDay = 0;       // local time
  int batteryPercent = -1;   // -1: no battery fitted
  bool charging = false;
  bool onUsb = false;
  bool syncing = false;
  bool linked = false;
  // The Wi-Fi mark: -1 no network set up; 0 the last try found none; 1 to 3
  // the signal of the last one joined.
  int wifiBars = -1;
  // Minutes since the day on screen was confirmed by the API; UINT32_MAX: never.
  uint32_t syncedMinutesAgo = UINT32_MAX;
  UpdateState update = UpdateState::Unknown;
  String updateVersion;  // the firmware on offer, when `update` is Available
  String updateNotes;    // what it brings, one or two sentences
  // Why the search failed, in a few technical words ("sem Wi-Fi", "HTTP 404",
  // "conexão -1, TLS -0x2700"): what to read out when asking for help.
  String updateDetail;
};

// A celebration's picture as the screens take it: square, RGB565, already
// decoded. `pixels` stays valid until a picture is asked for another slug.
struct Art {
  const uint8_t* pixels = nullptr;
  uint16_t side = 0;
};

// And its text: a summary that fits the glass, and the post in headings and
// paragraphs for whoever wants to read on.
using model::CelebrationText;
using model::TextBlock;

// The "Sobre" screen, asked for when it opens.
struct Diagnostics {
  String firmware;
  String board;
  String deviceName;
  int batteryPercent = -1;
  int batteryMillivolts = 0;
  uint32_t wakesToday = 0;
  uint32_t syncsToday = 0;
  int32_t clockDriftPpm = 0;
  // Internal memory (the kind Wi-Fi and TLS need), free now and at its
  // lowest since the wake, in KB; 0: not known.
  uint32_t freeMemoryKb = 0;
  uint32_t lowestMemoryKb = 0;
  // The local time of the next wake the compass scheduled for itself
  // ("22:00"); empty: only a button will wake it.
  String nextWake;
  // The last failure fetching a picture or a text from the site, named
  // ("HTTP 403", "conexão -1, TLS -0x2700"); empty: none since the wake.
  String siteProblem;
  std::vector<String> networks;
  // The last times it woke and how far each wake got, newest first: what to
  // read out when a reminder did not ring.
  std::vector<sys::WakeNote> wakes;
};

struct Actions {
  std::function<void(const Prefs&)> prefsChanged;
  std::function<void()> sleepNow;
  std::function<void()> powerOff;
  std::function<void()> syncNow;
  std::function<void()> openWifiPortal;
  std::function<void(int index)> forgetNetwork;
  std::function<void()> unlink;
  std::function<void()> factoryReset;
  std::function<void(int strikes)> bell;
  // Starts a time of silence of that many minutes; 0 ends the one running.
  std::function<void(int minutes)> silence;
  std::function<Diagnostics()> diagnostics;
  // The celebration's picture and text, by slug, from what the compass has
  // stored. Empty answers when it has not fetched them (yet).
  std::function<Art(const String& slug)> art;
  std::function<CelebrationText(const String& slug)> celebrationText;
  // Looks for a newer firmware; installs the one on offer.
  std::function<void()> checkUpdate;
  std::function<void()> installUpdate;
  // The light of a wake moved (see brightnessScale): set the panel now, not
  // at the controller's next pass.
  std::function<void()> brightnessMoved;
  // A change of screen (wipe.h). `stageBegin` sends what LVGL draws next to
  // a frame kept out of sight instead of the glass, and answers false when
  // there is no memory for one; `stageReveal` lays that frame over the glass
  // and gives the glass back to LVGL. Left unset, screens are simply
  // redrawn in place.
  std::function<bool()> stageBegin;
  std::function<void(Wipe)> stageReveal;
  // Sleeps now and rings in a minute: a reminder to try the compass with.
  std::function<void()> testReminder;
};

// Creates the screens. Call once after the display is up.
void init(const Prefs& prefs, Actions actions);

// ------------------------------------------------------------------ notices
// Full-screen messages, used while there is nothing liturgical to show or
// when something asks for attention.
void showSplash();
void showMessage(const String& title, const String& text, bool spinner);

enum class PortalStep { WaitingPhone, PhoneConnected, Connecting, Connected, Failed };
// The Wi-Fi setup screen: a QR that joins the phone to the compass' own
// network, and what is happening on the page the phone opened.
void showWifiPortal(const String& ssid, PortalStep step, const String& network);

// The link screen: code, QR of the verification URL and a countdown.
void showLink(const String& userCode, const String& verificationUri);
void setLinkCountdown(uint32_t secondsLeft);

// A reminder for an office (0 morning .. 3 compline), the bell of the hours,
// the end of a time of silence, and the last screen before a flat battery
// switches the board off.
void showReminder(int office);
// The app's evening nudge: nothing prayed yet today.
void showStreakReminder();
// The reminder asked for from the settings, to see it ring.
void showReminderTest();
void showHourBell(int hour);
void showSilenceDone();
void showLowBattery();
// A firmware being written: how far it is, 0..100.
void showUpdating(const String& version, int percent);
// Leaves a reminder or a bell and goes back to the day.
void dismissNotice();

// -------------------------------------------------------------------- state
// The day to show: already the view for the local date (model::viewFor).
void setToday(const model::Today& today);
// Forgets the day (after an unlink): notices are all there is to show.
void clearDay();
// A celebration's picture or text has just been stored: the screens that
// show it draw themselves again.
void contentArrived(const String& slug);
void setStatus(const Status& status);
// A time of silence under way, and how much of it is left.
void setSilence(bool running, uint32_t remainingSeconds);

// Opens the home face the owner chose.
void goHome();
bool showingDay();
// True while a notice is on screen and the day is not.
bool showingNotice();

// --------------------------------------------------------------------- input
// Touch swipes and the BOOT button reach the screens through the controller.
enum class Swipe { Up, Down, Left, Right };
// True when the swipe changed the screen (the caller then cancels the rest
// of that press); false when the screen keeps it, as a scrolling list does.
bool onSwipe(Swipe dir);
void onBootPress();
void onBootLongPress();

// The first screen after a wake is drawn in the dark and the panel's own
// brightness then rises on it. This is the factor, 0..255, the controller
// multiplies its brightness by; it is 255 the rest of the time.
uint8_t brightnessScale();
// True while that rise, or a change of screen, is under way. Anything that
// blocks for a while (the bell) waits for it to end, or the glass would
// freeze half lit.
bool changingScreen();

// What the screen on show asks of the power policy.
struct PowerHint {
  uint32_t screenSeconds = 0;  // 0: the owner's setting
  int brightness = -1;         // -1: the owner's setting; else 0..255
};
PowerHint powerHint();

// The bead counter survives sleep: the controller keeps it in RTC memory.
uint16_t beadsCount();
void setBeadsCount(uint16_t count);

}  // namespace ui
