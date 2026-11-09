#include "power.h"

#include <Wire.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "../app_config.h"
#include "board_pins.h"

#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

namespace board {
namespace {

XPowersPMU pmu;
bool pmuOk = false;
bool lastPwrLevel = false;
uint32_t lastPwrChangeMs = 0;
Wake wake = Wake::ColdBoot;

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

Wake readWakeCause() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0:
      return Wake::PowerButton;
    case ESP_SLEEP_WAKEUP_EXT1: {
      const uint64_t pins = esp_sleep_get_ext1_wakeup_status();
      if (pins & (1ULL << PIN_TP_INT)) return Wake::Touch;
      return Wake::BootButton;
    }
    case ESP_SLEEP_WAKEUP_TIMER:
      return Wake::Timer;
    default:
      return Wake::ColdBoot;
  }
}

// Waits for a button to be let go, so the press that asked for sleep is not
// also the press that ends it.
void waitReleased() {
  const uint32_t start = millis();
  while (millis() - start < 3000) {
    if (!readPwrLevel() && digitalRead(PIN_BTN_BOOT) == HIGH) break;
    delay(10);
  }
  delay(60);
}

}  // namespace

bool powerInit() {
  wake = readWakeCause();

  // Pins used as wake sources were handed to the RTC domain before sleeping.
  rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_BTN_BOOT));
  rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_TP_INT));
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
#if !BOARD_HAS_IO_EXPANDER
  rtc_gpio_deinit(static_cast<gpio_num_t>(PIN_BTN_PWR));
  pinMode(PIN_BTN_PWR, INPUT_PULLDOWN);
#endif

  // The amplifier stays off until a bell rings.
  pinMode(PIN_PA_CTRL, OUTPUT);
  digitalWrite(PIN_PA_CTRL, LOW);

  // A press that woke us is still down: it must not also count as "sleep now".
  lastPwrLevel = readPwrLevel();
  lastPwrChangeMs = millis();

  pmuOk = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
  if (!pmuOk) return false;

  // The compass never acts on PMU interrupts; the button is polled.
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  pmu.clearIrqStatus();
  pmu.enableBattDetection();
  pmu.enableBattVoltageMeasure();
  pmu.enableVbusVoltageMeasure();
  pmu.enableSystemVoltageMeasure();
  // The charge LED would be the brightest thing on a sleeping keychain.
  pmu.setChargingLedMode(XPOWERS_CHG_LED_OFF);
  // Holding PWR for six seconds cuts the board's power at the PMU, whatever
  // the firmware is doing: the way out of a hang, and the off switch for a
  // drawer. A short press afterwards switches it back on.
  pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_6S);
  pmu.setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);
  pmu.setLongPressPowerOFF();
  pmu.enableLongPressShutdown();
  return true;
}

Wake wakeCause() { return wake; }

BatteryState batteryState() {
  BatteryState state;
  if (!pmuOk) return state;
  state.present = pmu.isBatteryConnect();
  state.charging = pmu.isCharging();
  state.onUsb = pmu.isVbusIn();
  state.percent = state.present ? pmu.getBatteryPercent() : -1;
  state.millivolts = state.present ? pmu.getBattVoltage() : 0;
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

bool bootButtonDown() { return digitalRead(PIN_BTN_BOOT) == LOW; }

void deepSleep(uint64_t timerSeconds, bool touchWakes) {
  waitReleased();

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (timerSeconds > 0) esp_sleep_enable_timer_wakeup(timerSeconds * 1000000ULL);

  // BOOT reads low while pressed, and so does the touch interrupt: both go
  // on the "any low" wake. The pull-ups need the RTC peripherals powered.
  uint64_t lowPins = 1ULL << PIN_BTN_BOOT;
  rtc_gpio_init(static_cast<gpio_num_t>(PIN_BTN_BOOT));
  rtc_gpio_set_direction(static_cast<gpio_num_t>(PIN_BTN_BOOT), RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(static_cast<gpio_num_t>(PIN_BTN_BOOT));
  rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(PIN_BTN_BOOT));
  if (touchWakes) {
    lowPins |= 1ULL << PIN_TP_INT;
    rtc_gpio_init(static_cast<gpio_num_t>(PIN_TP_INT));
    rtc_gpio_set_direction(static_cast<gpio_num_t>(PIN_TP_INT), RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(static_cast<gpio_num_t>(PIN_TP_INT));
    rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(PIN_TP_INT));
  }
  esp_sleep_enable_ext1_wakeup_io(lowPins, ESP_EXT1_WAKEUP_ANY_LOW);

#if !BOARD_HAS_IO_EXPANDER
  // PWR reads high while pressed: the level wake.
  rtc_gpio_init(static_cast<gpio_num_t>(PIN_BTN_PWR));
  rtc_gpio_set_direction(static_cast<gpio_num_t>(PIN_BTN_PWR), RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_dis(static_cast<gpio_num_t>(PIN_BTN_PWR));
  rtc_gpio_pulldown_en(static_cast<gpio_num_t>(PIN_BTN_PWR));
  esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(PIN_BTN_PWR), 1);
#endif
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

  // Lines that were held (display and touch resets) stay held.
  gpio_deep_sleep_hold_en();
  esp_deep_sleep_start();
  while (true) {
  }
}

void powerOff() {
  waitReleased();
  if (pmuOk) pmu.shutdown();
  // Without a PMU there is nothing to cut: sleep with no timer instead.
  deepSleep(0, false);
}

}  // namespace board
