#pragma once

#include <lvgl.h>

namespace board {

// Resets the CST9217, opens it on the shared I2C bus and registers it as
// the LVGL pointer device. Returns false if the controller did not answer.
bool touchInit();

lv_indev_t* touchIndev();

}  // namespace board
