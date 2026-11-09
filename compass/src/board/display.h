#pragma once

#include <lvgl.h>
#include <stdint.h>

namespace board {

// Brings up the CO5300 over QSPI and registers it as the LVGL display with
// two partial draw buffers in internal DMA memory. Returns false if the
// panel did not answer.
bool displayInit();

// 0 (off) to 255, written as the panel's own brightness command. The AMOLED
// has no backlight; black pixels cost nothing, dim text costs little.
void displaySetBrightness(uint8_t level);

// Panel sleep: the AMOLED stops driving pixels and the controller idles.
// Far cheaper than brightness 0, which keeps the panel scanning black.
void displayOff();
void displayOn(uint8_t brightness);

lv_display_t* display();

}  // namespace board
