#pragma once

#include <lvgl.h>

namespace board {

enum class TouchSwipe { Up, Down, Left, Right };

// Brings up the CST9217 and registers it as LVGL's pointer.
bool touchInit();
lv_indev_t* touchIndev();

// A swipe the finger has just made (a drag of cfg::kSwipePixels, mostly
// along one axis), reported once per press. The caller decides what it
// means; if it changes the screen, touchCancelPress() keeps the rest of the
// press from also landing as a tap.
bool touchTakeSwipe(TouchSwipe& out);
void touchCancelPress();

// True once after any contact with the glass since the last call.
bool touchTakeActivity();

// Puts the controller to sleep before a deep sleep; touchInit() wakes it.
void touchSleep();
// Holds its reset line through deep sleep.
void touchHoldForSleep();

}  // namespace board
