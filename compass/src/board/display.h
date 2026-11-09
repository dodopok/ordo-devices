#pragma once

#include <lvgl.h>

namespace board {

// Brings up the CO5300 over QSPI and registers it with LVGL. Call lv_init()
// is done here; the display starts black with the panel's brightness at 0.
bool displayInit();
// 0..255; the AMOLED has no backlight, this is the panel's own brightness.
void displaySetBrightness(uint8_t level);
uint8_t displayBrightness();
// Panel off and in its sleep mode (a few tens of microamps). The image is
// lost; displayInit() after a deep sleep, or displayOn() while awake, bring
// it back.
void displayOff();
void displayOn(uint8_t brightness);
// Holds the panel's reset line through deep sleep, so it stays asleep
// instead of floating into a reset.
void displayHoldForSleep();

lv_display_t* display();

}  // namespace board
