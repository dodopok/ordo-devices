#pragma once

// Shared by the ui_*.cpp files; nothing outside src/ui includes this.

#include <math.h>

#include "theme.h"
#include "ui.h"

namespace ui::detail {

using theme::kCenter;
using theme::kScreen;

// What is on the glass right now.
enum class Mode { Notice, Pages, Beads, Silence, Torch, Settings, SettingsSub };

struct State {
  model::Today day;
  Status status;
  Prefs prefs;
  Actions actions;

  Mode mode = Mode::Notice;
  Page page = Page::Now;
  bool dayShown = false;
  // A notice that sits on top of the day (a reminder, a bell) rather than
  // replacing it (Wi-Fi setup, the link code).
  bool noticeOverDay = false;
  uint32_t noticeSeconds = 0;

  lv_obj_t* pages[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  bool dirty[6] = {true, true, true, true, true, true};
  lv_obj_t* tool = nullptr;      // beads, silence, torch
  lv_obj_t* settings = nullptr;  // the settings list
  lv_obj_t* sub = nullptr;       // a settings sub-screen
  lv_obj_t* notice = nullptr;

  uint16_t beads = 0;
  bool silenceRunning = false;
  uint32_t silenceRemaining = 0;
};

extern State S;

// ---- helpers (ui_core.cpp)
lv_obj_t* blankScreen();
void inert(lv_obj_t* obj);
void clear(lv_obj_t* scr);
lv_obj_t* text(lv_obj_t* parent, const char* str, const lv_font_t* font, lv_color_t color, int32_t y,
               int32_t letterSpacing = 0, int32_t width = 386);
lv_obj_t* dot(lv_obj_t* parent, int32_t size, lv_color_t fill, lv_color_t border, int32_t borderWidth);
void placePolar(lv_obj_t* obj, float radius, float fraction);
lv_obj_t* ringArc(lv_obj_t* parent, int32_t diameter, int32_t width, float from, float to, lv_color_t color,
                  lv_opa_t opa = LV_OPA_COVER);
lv_obj_t* pill(lv_obj_t* parent, const char* label, bool selected, lv_event_cb_t cb, void* user,
               lv_color_t accent = theme::gold());
// Breathes an object's opacity between two values, for "this is next".
void breathe(lv_obj_t* obj, lv_opa_t low, lv_anim_exec_xcb_t exec);
void statusMarkers(lv_obj_t* scr);
void pageDots(lv_obj_t* scr, Page page);
// A small "‹ voltar" at the top of tools and settings.
void backChip(lv_obj_t* scr);
void load(lv_obj_t* scr);

const char* tr(const char* pt, const char* en, const char* es);
const char* officeName(int i);
const char* officeShort(int i);
const char* slotName(const String& slot);
const char* monthShort(int month);
const char* weekdayShort(int weekday);
String upper(const String& s);
String dateLine(const model::DayInfo& day);
String clockText(int minuteOfDay);
lv_color_t accentOf(const model::Today& day);

// ---- navigation (ui_core.cpp)
void showPage(Page page);
void goBack();
void rebuildVisible();

// ---- builders
void buildNow(lv_obj_t* scr);       // ui_faces.cpp
void buildCompass(lv_obj_t* scr);
void buildReadings(lv_obj_t* scr);  // ui_day.cpp
void buildOffices(lv_obj_t* scr);
void buildWeek(lv_obj_t* scr);
void buildMenu(lv_obj_t* scr);      // ui_tools.cpp
void openBeads();
void openSilence();
void openTorch();
void beadsCountOne();
void beadsReset();
void openSettings();                // ui_settings.cpp
void refreshSettings();

}  // namespace ui::detail
