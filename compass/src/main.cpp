// Ordo compass: the liturgical day on a round AMOLED, linked to one Ordo
// account, built to live on a keychain. See README.md for the flow.

#include <Arduino.h>

#include "app/controller.h"
#include "app_config.h"

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
