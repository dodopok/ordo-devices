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

// The stage of a screen change: one whole frame, and room for one upright
// strip gathered from it. 450 KB of the 8 MB of external memory, taken the
// first time a screen changes and kept.
uint16_t* stage = nullptr;
uint16_t* upright = nullptr;
bool staging = false;
// LVGL's clock: the processor's, less the time spent changing screens.
uint32_t clockHeldMs = 0;
uint32_t clockStoppedAt = 0;

void flushCb(lv_display_t* d, const lv_area_t* area, uint8_t* px_map) {
  const int32_t w = lv_area_get_width(area);
  const int32_t h = lv_area_get_height(area);
  if (staging) {
    const uint16_t* from = reinterpret_cast<const uint16_t*>(px_map);
    for (int32_t y = area->y1; y <= area->y2; y++, from += w) {
      memcpy(stage + y * LCD_WIDTH + area->x1, from, static_cast<size_t>(w) * sizeof(uint16_t));
    }
  } else {
    gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t*>(px_map), w, h);
  }
  lv_display_flush_ready(d);
}

// One strip of the stage onto the glass.
void layStrip(const ui::WipeStrip& strip) {
  if (strip.w == LCD_WIDTH) {
    gfx->draw16bitRGBBitmap(0, strip.y, stage + strip.y * LCD_WIDTH, LCD_WIDTH, strip.h);
    return;
  }
  // An upright strip is not one stretch of the frame: gathered row by row.
  for (int32_t y = 0; y < LCD_HEIGHT; y++) {
    memcpy(upright + y * strip.w, stage + y * LCD_WIDTH + strip.x, static_cast<size_t>(strip.w) * sizeof(uint16_t));
  }
  gfx->draw16bitRGBBitmap(strip.x, 0, upright, strip.w, LCD_HEIGHT);
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

// Never backwards: while a screen changes it stands at the moment the change
// began, and goes on from there.
uint32_t tickCb() { return staging ? clockStoppedAt : millis() - clockHeldMs; }

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

bool displayStageBegin() {
  if (!gfx || staging) return false;
  if (!stage) {
    stage = static_cast<uint16_t*>(heap_caps_malloc(static_cast<size_t>(LCD_WIDTH) * LCD_HEIGHT * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    upright = static_cast<uint16_t*>(heap_caps_malloc(static_cast<size_t>(ui::kWipeLines) * LCD_HEIGHT * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!stage || !upright) {
      free(stage);
      free(upright);
      stage = nullptr;
      upright = nullptr;
      return false;
    }
  }
  clockStoppedAt = millis() - clockHeldMs;
  staging = true;
  return true;
}

void displayStageReveal(ui::Wipe wipe) {
  if (!staging) return;
  const int steps = ui::wipeSteps(wipe, LCD_WIDTH);
  const uint32_t started = millis();
  int laid = 0;
  while (laid < steps) {
    const int due = ui::wipeStepsDue(steps, millis() - started);
    if (laid >= due) {
      delay(1);
      continue;
    }
    for (; laid < due; laid++) {
      ui::WipeStrip strips[2];
      const int count = ui::wipeStrips(wipe, LCD_WIDTH, laid, strips);
      for (int i = 0; i < count; i++) layStrip(strips[i]);
    }
  }
  // The clock goes on from where it stopped.
  clockHeldMs = millis() - clockStoppedAt;
  staging = false;
}

}  // namespace board
