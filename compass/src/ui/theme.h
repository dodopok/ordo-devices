#pragma once

#include <lvgl.h>

#include "fonts/fonts.h"

// The compass is an AMOLED: black costs nothing, so the background is black
// and the liturgical colour is the only light. Text colours and the two
// typefaces are the Ordo app's dark theme: Raleway for the interface,
// Newsreader for the names of days and feasts.
namespace ui::theme {

inline lv_color_t bg() { return lv_color_hex(0x000000); }
inline lv_color_t fg() { return lv_color_hex(0xE8E6E3); }
inline lv_color_t dim() { return lv_color_hex(0xA3A3A0); }
inline lv_color_t faint() { return lv_color_hex(0x6B6F78); }
inline lv_color_t track() { return lv_color_hex(0x1A1C20); }
inline lv_color_t surface() { return lv_color_hex(0x171A20); }
inline lv_color_t border() { return lv_color_hex(0x262B33); }
inline lv_color_t gold() { return lv_color_hex(0xE8C968); }
inline lv_color_t green() { return lv_color_hex(0x66BB6A); }
inline lv_color_t red() { return lv_color_hex(0xEF5350); }
inline lv_color_t paper() { return lv_color_hex(0xF2F2EE); }

inline const lv_font_t* tiny() { return &ordo_sans_14; }     // captions, letter-spaced capitals
inline const lv_font_t* small() { return &ordo_sans_16; }
inline const lv_font_t* body() { return &ordo_sans_18; }
inline const lv_font_t* label() { return &ordo_sans_20; }
inline const lv_font_t* reading() { return &ordo_sans_24; }
inline const lv_font_t* italic() { return &ordo_serif_20i; }
inline const lv_font_t* title() { return &ordo_serif_24; }   // the day's name
inline const lv_font_t* large() { return &ordo_serif_30; }
inline const lv_font_t* display() { return &ordo_serif_44; }
inline const lv_font_t* digits() { return &ordo_digits_104; }  // clock and counters: digits and colon only
inline const lv_font_t* code() { return &ordo_code_44; }       // link code: capitals, digits, hyphen
inline const lv_font_t* icons() { return &lv_font_montserrat_20; }  // LV_SYMBOL_* only
inline const lv_font_t* iconsLarge() { return &lv_font_montserrat_28; }

constexpr int32_t kScreen = 466;
constexpr int32_t kCenter = 233;

}  // namespace ui::theme
