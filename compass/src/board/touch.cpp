#include "touch.h"

#include <Arduino.h>
#include <TouchDrvCSTXXX.hpp>
#include <Wire.h>

#include "board_pins.h"

// The panel is mounted so that both axes read mirrored relative to the
// display's rotation 0; Waveshare's own examples flip both. Override at build
// time if a revision differs.
#ifndef TOUCH_MIRROR_X
#define TOUCH_MIRROR_X true
#endif
#ifndef TOUCH_MIRROR_Y
#define TOUCH_MIRROR_Y true
#endif

namespace board {
namespace {

TouchDrvCST92xx touch;
lv_indev_t* indev = nullptr;

void readCb(lv_indev_t*, lv_indev_data_t* data) {
  int16_t x[1] = {0};
  int16_t y[1] = {0};
  // The INT line sits low while a finger is down; skipping the I2C read
  // otherwise keeps the bus quiet for the PMU and the IMU.
  if (digitalRead(PIN_TP_INT) == HIGH) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  if (touch.getPoint(x, y, 1) > 0) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x[0];
    data->point.y = y[0];
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

}  // namespace

bool touchInit() {
  pinMode(PIN_TP_INT, INPUT_PULLUP);
  pinMode(PIN_TP_RST, OUTPUT);
  digitalWrite(PIN_TP_RST, LOW);
  delay(30);
  digitalWrite(PIN_TP_RST, HIGH);
  delay(50);

  touch.setPins(PIN_TP_RST, PIN_TP_INT);
  if (!touch.begin(Wire, I2C_ADDR_TOUCH, PIN_I2C_SDA, PIN_I2C_SCL)) {
    return false;
  }
  touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  touch.setMirrorXY(TOUCH_MIRROR_X, TOUCH_MIRROR_Y);

  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, readCb);
  return true;
}

lv_indev_t* touchIndev() { return indev; }

}  // namespace board
