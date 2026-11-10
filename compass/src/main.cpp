// Ordo compass: the liturgical day on a round AMOLED, linked to one Ordo
// account, built to live on a keychain. See README.md for the flow.

#include <Arduino.h>

#include "app/controller.h"
#include "app_config.h"

// The main loop makes TLS connections of its own (a background sync, a
// search for a firmware the owner asked for), on top of whatever LVGL has on
// the stack: twice the default 8 KB.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

namespace {
app::Controller controller;
}

void setup() {
#ifdef ORDO_DEBUG
  Serial.begin(115200);
  delay(200);
#endif
  controller.begin();
}

void loop() {
  controller.loop();
  delay(3);
}
