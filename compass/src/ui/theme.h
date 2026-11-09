#pragma once

#include <lvgl.h>

// The compass is an AMOLED: black costs nothing, so the background is
// black and the liturgical colour is the only light. Palette shared with
// the Ordo app's dark theme.
namespace ui::theme {

inline lv_color_t bg() { return lv_color_hex(0x000000); }
inline lv_color_t fg() { return lv_color_hex(0xF2F2EE); }
inline lv_color_t dim() { return lv_color_hex(0x8C8F8A); }
inline lv_color_t track() { return lv_color_hex(0x1A1A1A); }
inline lv_color_t gold() { return lv_color_hex(0xE8C968); }
inline lv_color_t green() { return lv_color_hex(0x66BB6A); }
inline lv_color_t paper() { return lv_color_hex(0xF2F2EE); }

inline const lv_font_t* tiny() { return &lv_font_montserrat_12; }
inline const lv_font_t* small() { return &lv_font_montserrat_14; }
inline const lv_font_t* body() { return &lv_font_montserrat_16; }
inline const lv_font_t* label() { return &lv_font_montserrat_18; }
inline const lv_font_t* title() { return &lv_font_montserrat_20; }
inline const lv_font_t* large() { return &lv_font_montserrat_28; }
inline const lv_font_t* reading() { return &lv_font_montserrat_24; }
inline const lv_font_t* display() { return &lv_font_montserrat_44; }
inline const lv_font_t* code() { return &lv_font_montserrat_40; }

constexpr int32_t kScreen = 466;
constexpr int32_t kCenter = 233;

}  // namespace ui::theme
