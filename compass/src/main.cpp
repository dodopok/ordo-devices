// Ordo compass: the liturgical day on a round AMOLED, linked to one Ordo
// account. See README.md for the flow and docs/ for the design.

#include <Arduino.h>

#include "app/controller.h"

namespace {
app::Controller controller;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  controller.begin();
}

void loop() {
  controller.loop();
  delay(3);
}
