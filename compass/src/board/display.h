#pragma once

#include <lvgl.h>

#include "../ui/wipe.h"

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

// A change of screen (ui/wipe.h). Between the two calls LVGL draws into a
// frame kept in the external memory instead of onto the glass, which keeps
// showing the old screen; `displayStageReveal` then lays that frame over the
// glass strip by strip and hands the glass back. LVGL's clock stands still
// meanwhile, so what the new screen animates begins when it is in place.
// `displayStageBegin` answers false when there is no memory for the frame:
// nothing was changed, and LVGL goes on drawing straight onto the glass.
bool displayStageBegin();
void displayStageReveal(ui::Wipe wipe);

}  // namespace board
