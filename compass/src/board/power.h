#pragma once

#include <stdint.h>

namespace board {

struct BatteryState {
  bool present = false;
  bool charging = false;
  bool onUsb = false;
  int percent = -1;  // -1 when unknown
};

// Opens the AXP2101 on the shared bus and switches on battery telemetry.
// Returns false if the PMU did not answer; the compass still works on USB.
bool powerInit();

BatteryState batteryState();

// The side PWR button. On the 1.75C it is a GPIO; on the standard board it
// is read through the TCA9554 expander. Debounced edge: true once per press.
bool powerButtonPressedEdge();

// The BOOT button (GPIO0), level: true while held.
bool bootButtonHeld();

}  // namespace board
