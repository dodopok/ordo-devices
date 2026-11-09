#include "touch.h"

#include <Arduino.h>
#include <TouchDrvCSTXXX.hpp>
#include <Wire.h>
#include <driver/gpio.h>

#include "../app_config.h"
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
bool touchOk = false;

// The CST9217 pulses its interrupt line once per report; it does not hold it
// low while a finger is down. Reading the line's level (as the first
// firmware did) catches one sample in many and chops every drag into taps.
// The pulse is latched here instead, and the controller is polled for as
// long as a finger is, or has just been, on the glass.
volatile bool irqSeen = false;
volatile bool activity = false;
uint32_t lastIrqMs = 0;
uint32_t lastPointMs = 0;
bool pressed = false;
int16_t startX = 0, startY = 0, lastX = 0, lastY = 0;
bool swipeSent = false;
bool swipePending = false;
TouchSwipe swipe = TouchSwipe::Up;

void IRAM_ATTR onIrq() {
  irqSeen = true;
  activity = true;
}

void trackSwipe(int16_t x, int16_t y) {
  if (swipeSent) return;
  const int32_t dx = x - startX;
  const int32_t dy = y - startY;
  const int32_t ax = dx < 0 ? -dx : dx;
  const int32_t ay = dy < 0 ? -dy : dy;
  if (ax < cfg::kSwipePixels && ay < cfg::kSwipePixels) return;
  swipeSent = true;
  swipePending = true;
  if (ay >= ax) swipe = dy < 0 ? TouchSwipe::Up : TouchSwipe::Down;
  else swipe = dx < 0 ? TouchSwipe::Left : TouchSwipe::Right;
}

void readCb(lv_indev_t*, lv_indev_data_t* data) {
  const uint32_t now = millis();
  if (irqSeen) {
    irqSeen = false;
    lastIrqMs = now;
  }
  // Nothing on the glass and no pulse lately: leave the I2C bus alone.
  if (!touchOk || (!pressed && now - lastIrqMs > 150)) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  int16_t x[1] = {0};
  int16_t y[1] = {0};
  if (touch.getPoint(x, y, 1) > 0) {
    lastPointMs = now;
    if (!pressed) {
      pressed = true;
      startX = x[0];
      startY = y[0];
      swipeSent = false;
    }
    lastX = x[0];
    lastY = y[0];
    trackSwipe(lastX, lastY);
  } else if (pressed && now - lastPointMs > 60) {
    // An empty report can slip in between two real ones; the finger is gone
    // only when there has been nothing for a few frames.
    pressed = false;
  }

  data->point.x = lastX;
  data->point.y = lastY;
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

}  // namespace

bool touchInit() {
  gpio_hold_dis(static_cast<gpio_num_t>(PIN_TP_RST));
  pinMode(PIN_TP_INT, INPUT_PULLUP);
  pinMode(PIN_TP_RST, OUTPUT);
  digitalWrite(PIN_TP_RST, LOW);
  delay(12);
  digitalWrite(PIN_TP_RST, HIGH);
  delay(40);

  touch.setPins(PIN_TP_RST, PIN_TP_INT);
  touchOk = touch.begin(Wire, I2C_ADDR_TOUCH, PIN_I2C_SDA, PIN_I2C_SCL);
  if (touchOk) {
    touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
    touch.setMirrorXY(TOUCH_MIRROR_X, TOUCH_MIRROR_Y);
    attachInterrupt(digitalPinToInterrupt(PIN_TP_INT), onIrq, FALLING);
  }

  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, readCb);
  return touchOk;
}

lv_indev_t* touchIndev() { return indev; }

bool touchTakeSwipe(TouchSwipe& out) {
  if (!swipePending) return false;
  swipePending = false;
  out = swipe;
  return true;
}

void touchCancelPress() {
  if (indev) lv_indev_wait_release(indev);
}

bool touchTakeActivity() {
  if (!activity) return false;
  activity = false;
  return true;
}

void touchSleep() {
  if (!touchOk) return;
  detachInterrupt(digitalPinToInterrupt(PIN_TP_INT));
  touch.sleep();
}

void touchHoldForSleep() {
  digitalWrite(PIN_TP_RST, HIGH);
  gpio_hold_en(static_cast<gpio_num_t>(PIN_TP_RST));
}

}  // namespace board
