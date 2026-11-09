#pragma once

// Pin map for the Waveshare ESP32-S3-Touch-AMOLED-1.75 family.
//
// Source: the board repositories' bundled `pin_config.h` and the 1.75
// HARDWARE_REFERENCE.md (waveshareteam/ESP32-S3-Touch-AMOLED-1.75 and
// waveshareteam/ESP32-S3-Touch-AMOLED-1.75C). The display bus, I2C bus and
// touch interrupt are the same on both; the resets and the power button are
// not, and only the standard board has the TCA9554 expander and a card slot.

#if !defined(ORDO_BOARD_AMOLED_175C) && !defined(ORDO_BOARD_AMOLED_175)
#error "Pick a board: build with -DORDO_BOARD_AMOLED_175C or -DORDO_BOARD_AMOLED_175"
#endif

// CO5300 AMOLED over QSPI, 466 x 466, brightness by panel command 0x51.
#define PIN_LCD_SDIO0 4
#define PIN_LCD_SDIO1 5
#define PIN_LCD_SDIO2 6
#define PIN_LCD_SDIO3 7
#define PIN_LCD_SCLK 38
#define PIN_LCD_CS 12
#define PIN_LCD_TE 13
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
// The panel's first visible column is offset by 6 in the controller's memory.
#define LCD_COL_OFFSET 6

// Shared I2C bus: CST9217 (0x5A), AXP2101 (0x34), QMI8658 (0x6B),
// and on the standard board PCF85063 (0x51) and TCA9554 (0x20).
#define PIN_I2C_SDA 15
#define PIN_I2C_SCL 14
#define PIN_TP_INT 11
#define I2C_ADDR_TOUCH 0x5A

// BOOT is a strap pin with an external pull-up; it reads LOW while pressed.
#define PIN_BTN_BOOT 0

// ES8311 codec over I2S and the NS4150B amplifier behind it. The amplifier's
// control line has a pull-down on the board: floating means silent.
#define I2C_ADDR_CODEC 0x18
#define PIN_I2S_BCLK 9
#define PIN_I2S_LRCK 45
#define PIN_I2S_DOUT 8
#define PIN_PA_CTRL 46

#if defined(ORDO_BOARD_AMOLED_175C)
#define ORDO_BOARD_NAME "ESP32-S3-Touch-AMOLED-1.75C"
#define PIN_LCD_RST 1
#define PIN_TP_RST 2
// On the C board the conditioned PWR-button level is on a GPIO.
#define PIN_BTN_PWR 3
#define BOARD_HAS_IO_EXPANDER 0
// MCLK on GPIO16 and the codec's address strap fixed on the board (the 1.75C
// schematic and Waveshare's 07_ES8311 example): the speaker needs nothing
// but I2C and I2S.
#define PIN_I2S_MCLK 16
#define BOARD_HAS_SPEAKER 1
// QMI8658 INT1 reaches the processor directly on the C board.
#define PIN_IMU_INT 21
#else
#define ORDO_BOARD_NAME "ESP32-S3-Touch-AMOLED-1.75"
#define PIN_LCD_RST 39
#define PIN_TP_RST 40
// PWR is read through TCA9554 P4 (EXIO4) on the standard board, not a GPIO.
#define PIN_BTN_PWR -1
#define BOARD_HAS_IO_EXPANDER 1
#define I2C_ADDR_IO_EXPANDER 0x20
#define IO_EXPANDER_PWR_BIT 4
// MCLK is GPIO42 here and the codec's enable goes through the expander
// (HARDWARE_REFERENCE.md). The bell is not wired up for this board yet.
#define PIN_I2S_MCLK 42
#define BOARD_HAS_SPEAKER 0
// QMI8658 INT2; INT1 goes through the expander.
#define PIN_IMU_INT 21
#endif
