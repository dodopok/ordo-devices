#pragma once

#include <Arduino.h>

namespace board {

struct BatteryState {
  bool present = false;
  bool charging = false;
  bool onUsb = false;
  int percent = -1;
  int millivolts = 0;
};

// Why the processor is running.
enum class Wake {
  ColdBoot,     // power applied, reset, or a firmware upload
  PowerButton,  // PWR
  BootButton,   // BOOT
  Touch,        // a finger on the glass, when the owner allowed that
  Timer,        // a wake the firmware scheduled for itself
};

// Reads why we woke, brings up the AXP2101 and the buttons, and makes sure
// the speaker amplifier is off. Returns false when the PMU does not answer;
// the compass still runs, without battery telemetry.
bool powerInit();
Wake wakeCause();
BatteryState batteryState();

// PWR: true once per press. BOOT: the level, low-active, debounced by the
// caller's timing.
bool powerButtonPressedEdge();
bool bootButtonDown();

// Deep sleep: only the low-power timer and the wake pins stay alive. Never
// returns; the next thing to run is setup(). `timerSeconds` 0 means no
// scheduled wake.
[[noreturn]] void deepSleep(uint64_t timerSeconds, bool touchWakes);

// Switches the board off at the PMU. Only the PWR button brings it back.
[[noreturn]] void powerOff();

}  // namespace board
