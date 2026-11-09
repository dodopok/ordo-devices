#include "power.h"

#include <Arduino.h>
#include <Wire.h>

#include "board_pins.h"

#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

namespace board {
namespace {

XPowersPMU pmu;
bool pmuOk = false;
bool lastPwrLevel = false;
uint32_t lastPwrChangeMs = 0;

bool readPwrLevel() {
#if BOARD_HAS_IO_EXPANDER
  // TCA9554 input port register is 0x00; P4 carries the conditioned PWR
  // level, high while pressed.
  Wire.beginTransmission(I2C_ADDR_IO_EXPANDER);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(static_cast<int>(I2C_ADDR_IO_EXPANDER), 1) != 1) return false;
  const uint8_t port = Wire.read();
  return (port >> IO_EXPANDER_PWR_BIT) & 0x01;
#else
  return digitalRead(PIN_BTN_PWR) == HIGH;
#endif
}

}  // namespace

bool powerInit() {
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
#if !BOARD_HAS_IO_EXPANDER
  pinMode(PIN_BTN_PWR, INPUT_PULLDOWN);
#endif

  pmuOk = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  if (!pmuOk) return false;

  // The compass never acts on PMU interrupts; the button is polled.
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  pmu.clearIrqStatus();
  pmu.enableBattDetection();
  pmu.enableBattVoltageMeasure();
  pmu.enableVbusVoltageMeasure();
  pmu.enableSystemVoltageMeasure();
  return true;
}

BatteryState batteryState() {
  BatteryState state;
  if (!pmuOk) return state;
  state.present = pmu.isBatteryConnect();
  state.charging = pmu.isCharging();
  state.onUsb = pmu.isVbusIn();
  state.percent = state.present ? pmu.getBatteryPercent() : -1;
  return state;
}

bool powerButtonPressedEdge() {
  const bool level = readPwrLevel();
  const uint32_t now = millis();
  if (level != lastPwrLevel && now - lastPwrChangeMs > 40) {
    lastPwrLevel = level;
    lastPwrChangeMs = now;
    return level;  // rising edge = press
  }
  return false;
}

bool bootButtonHeld() { return digitalRead(PIN_BTN_BOOT) == LOW; }

}  // namespace board
