#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include <functional>
#include <vector>

#include "../model/today.h"

// The screens. Nothing here touches the board or the network: the controller
// hands in the day, the clock and the battery, and gets back what the owner
// asked for through `Actions`. That is also what lets sim/ draw every screen
// on a computer.
namespace ui {

// The carousel, in order. It wraps: after Menu comes Now again.
enum class Page { Now, Compass, Readings, Offices, Week, Menu };

// What the owner can change on the settings screens. The controller keeps it
// in NVS; the screens edit a copy and report every change.
struct Prefs {
  uint8_t screenSeconds = 15;  // 10, 15, 30, 60
  uint8_t brightness = 1;      // 0 low, 1 medium, 2 high
  bool nightDim = true;        // dimmer between 22h and 6h
  uint8_t homeFace = 0;        // 0 Agora, 1 Bússola
  bool touchWakes = false;     // off: only the buttons wake the screen
  bool deskAlwaysOn = true;    // on USB power the screen stays on
  bool sound = true;
  uint8_t volume = 1;          // 0 low, 1 medium, 2 high
  bool hourBell = false;       // a bell at 6h, 12h and 18h
  int16_t reminder[4] = {-1, -1, -1, -1};  // minute of the day per office, -1: off
  uint8_t syncHours = 4;       // 0: only when woken; else every 2, 4, 6 or 12 hours
  uint8_t beadsMode = 0;       // 0 Anglican rosary (33), 1 rope of 33, 2 of 50, 3 of 100
};

// What changes by itself: the clock, the battery, the radio.
struct Status {
  bool clockValid = false;
  int minuteOfDay = 0;       // local time
  int batteryPercent = -1;   // -1: no battery fitted
  bool charging = false;
  bool onUsb = false;
  bool syncing = false;
  bool linked = false;
  // Minutes since the day on screen was confirmed by the API; UINT32_MAX: never.
  uint32_t syncedMinutesAgo = UINT32_MAX;
};

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
  std::vector<String> networks;
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
void showHourBell(int hour);
void showSilenceDone();
void showLowBattery();
// Leaves a reminder or a bell and goes back to the day.
void dismissNotice();

// -------------------------------------------------------------------- state
// The day to show: already the view for the local date (model::viewFor).
void setToday(const model::Today& today);
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
void onSwipe(Swipe dir);
void onBootPress();
void onBootLongPress();

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
