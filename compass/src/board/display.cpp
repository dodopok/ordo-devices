#include "display.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>

#include "board_pins.h"

namespace board {
namespace {

// One sixth of the screen: 466 x 78 x 2 bytes = 72 KB, small enough to leave
// internal RAM for Wi-Fi and TLS, large enough that a full redraw takes six
// flushes rather than dozens. One buffer, not two: the flush below returns
// only when the pixels are on the panel, so LVGL never needs a second one to
// draw into meanwhile, and TLS (which this SDK keeps in internal RAM) has
// those 72 KB instead.
constexpr int32_t kBufferRows = LCD_HEIGHT / 6 + 1;
constexpr size_t kBufferBytes = LCD_WIDTH * kBufferRows * sizeof(uint16_t);

Arduino_DataBus* bus = nullptr;
Arduino_CO5300* gfx = nullptr;
lv_display_t* disp = nullptr;
uint8_t brightness = 0;

void flushCb(lv_display_t* d, const lv_area_t* area, uint8_t* px_map) {
  const int32_t w = lv_area_get_width(area);
  const int32_t h = lv_area_get_height(area);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t*>(px_map), w, h);
  lv_display_flush_ready(d);
}

// The CO5300 addresses pixels in pairs: a window must start on an even
// column and row and end on an odd one, or the panel shifts the image by a
// pixel. LVGL asks before it draws, so the area is widened here.
void roundAreaCb(lv_event_t* e) {
  auto* area = static_cast<lv_area_t*>(lv_event_get_param(e));
  area->x1 &= ~1;
  area->y1 &= ~1;
  area->x2 |= 1;
  area->y2 |= 1;
  if (area->x2 >= LCD_WIDTH) area->x2 = LCD_WIDTH - 1;
  if (area->y2 >= LCD_HEIGHT) area->y2 = LCD_HEIGHT - 1;
}

uint32_t tickCb() { return millis(); }

}  // namespace

bool displayInit() {
  // The reset line was held through deep sleep; let go of it before the
  // driver pulses it.
  gpio_hold_dis(static_cast<gpio_num_t>(PIN_LCD_RST));
  bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_SDIO0, PIN_LCD_SDIO1, PIN_LCD_SDIO2,
                              PIN_LCD_SDIO3);
  gfx = new Arduino_CO5300(bus, PIN_LCD_RST, 0 /* rotation */, LCD_WIDTH, LCD_HEIGHT, LCD_COL_OFFSET, 0,
                           0, 0);
  if (!gfx->begin()) {
    return false;
  }
  gfx->fillScreen(RGB565_BLACK);
  gfx->setBrightness(0);

  lv_init();
  lv_tick_set_cb(tickCb);

  void* buffer = heap_caps_malloc(kBufferBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!buffer) {
    return false;
  }

  disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_set_buffers(disp, buffer, nullptr, kBufferBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_add_event_cb(disp, roundAreaCb, LV_EVENT_INVALIDATE_AREA, nullptr);
  return true;
}

void displaySetBrightness(uint8_t level) {
  if (!gfx || level == brightness) return;
  brightness = level;
  gfx->setBrightness(level);
}

uint8_t displayBrightness() { return brightness; }

void displayOff() {
  if (!gfx) return;
  gfx->setBrightness(0);
  brightness = 0;
  gfx->displayOff();
}

void displayOn(uint8_t level) {
  if (!gfx) return;
  gfx->displayOn();
  brightness = level;
  gfx->setBrightness(level);
}

void displayHoldForSleep() {
  pinMode(PIN_LCD_RST, OUTPUT);
  digitalWrite(PIN_LCD_RST, HIGH);
  gpio_hold_en(static_cast<gpio_num_t>(PIN_LCD_RST));
}

lv_display_t* display() { return disp; }

}  // namespace board
