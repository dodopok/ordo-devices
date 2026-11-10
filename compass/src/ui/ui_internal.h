#pragma once

// Shared by the ui_*.cpp files; nothing outside src/ui includes this.

#include <math.h>

#include "theme.h"
#include "ui.h"

namespace ui::detail {

using theme::kCenter;
using theme::kScreen;

// What is on the glass right now.
enum class Mode { Notice, Pages, Beads, Silence, Torch, Settings, SettingsSub, Reading };

constexpr int kPageCount = 7;

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

  lv_obj_t* pages[kPageCount] = {};
  bool dirty[kPageCount] = {true, true, true, true, true, true, true};
  // On the celebration page: false shows the picture whole, true the picture
  // above and the words below.
  bool celebrationOpen = false;
  lv_obj_t* tool = nullptr;      // beads, silence, torch
  lv_obj_t* settings = nullptr;  // the settings list
  lv_obj_t* sub = nullptr;       // a settings sub-screen
  lv_obj_t* notice = nullptr;

  uint16_t beads = 0;
  bool silenceRunning = false;
  uint32_t silenceRemaining = 0;

  // Screen changes (see ui::brightnessScale).
  uint8_t fade = 255;
  bool dipping = false;
  std::function<void()> pendingChange;
  bool risePending = false;
  bool shownOnce = false;
  // True while the screen being built is one the owner just navigated to:
  // its entrance (the marker finding today, the dots arriving) plays. A
  // rebuild because the minute changed does not replay it.
  bool entering = false;
};

extern State S;

// ---- helpers (ui_core.cpp)
lv_obj_t* blankScreen();
void inert(lv_obj_t* obj);
void clear(lv_obj_t* scr);
lv_obj_t* text(lv_obj_t* parent, const char* str, const lv_font_t* font, lv_color_t color, int32_t y,
               int32_t letterSpacing = 0, int32_t width = 386);
lv_obj_t* dot(lv_obj_t* parent, int32_t size, lv_color_t fill, lv_color_t border, int32_t borderWidth);
void polarPoint(float radius, float fraction, int32_t& x, int32_t& y);
void placePolar(lv_obj_t* obj, float radius, float fraction);
lv_obj_t* ringArc(lv_obj_t* parent, int32_t diameter, int32_t width, float from, float to, lv_color_t color,
                  lv_opa_t opa = LV_OPA_COVER);
lv_obj_t* pill(lv_obj_t* parent, const char* label, bool selected, lv_event_cb_t cb, void* user,
               lv_color_t accent = theme::gold());
// Breathes an object's opacity between two values, for "this is next".
void breathe(lv_obj_t* obj, lv_opa_t low, lv_anim_exec_xcb_t exec);
// Entrances. They play only on a screen the owner just navigated to, and only
// with animations on; otherwise these do nothing and the object is simply
// there. `delayMs` staggers the members of a group.
bool entrance();
void fadeIn(lv_obj_t* obj, uint32_t delayMs, uint32_t ms = 220);
void riseIn(lv_obj_t* obj, uint32_t delayMs, int32_t from = 12);
// A piece of ring (see ringArc) that draws itself from `from` to `to`. Only
// the stretch added at each step is repainted, which a fade of the whole arc
// (as wide as the glass) would not manage.
void sweepIn(lv_obj_t* arc, float from, float to, uint32_t delayMs, uint32_t ms = 320);
// A figure that counts up to its value.
void countUp(lv_obj_t* label, int value, uint32_t delayMs);
// A quick blink of something that just changed under a finger.
void pulse(lv_obj_t* obj);
void statusMarkers(lv_obj_t* scr, int32_t y);
// A column that scrolls under the round glass, padded so that its first and
// last rows can be brought to the middle, where the screen is widest.
lv_obj_t* scrollColumn(lv_obj_t* scr);
// The time an office's reminder rings at: the app's when the owner follows
// the app and it has any, the one set on the compass otherwise. -1: none.
int16_t reminderFor(int office);
// Whether a page of the carousel has anything to show today.
bool pageAvailable(Page page);
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
// Runs `change` (which builds and loads a screen) behind a dip to black, or
// at once when animations are off. The newest change wins if two arrive
// during the same dip.
void transition(std::function<void()> change);
// For what interrupts (a notice): `haltTransition` stops a change where it
// stands, light included; `revealNow` then paints whatever was loaded while
// the glass is still dim and brings the light back at once.
void haltTransition();
void revealNow();
void showPage(Page page);
Page homePage();
void goBack();
void rebuildVisible();

// ---- builders
void buildNow(lv_obj_t* scr);       // ui_faces.cpp
void buildCelebration(lv_obj_t* scr);  // ui_celebration.cpp
void celebrationContentArrived();
void openReading();
void openUpdate();                  // ui_settings.cpp
void buildCompass(lv_obj_t* scr);
void buildReadings(lv_obj_t* scr);  // ui_day.cpp
void buildOffices(lv_obj_t* scr);
void buildWeek(lv_obj_t* scr);
void buildMenu(lv_obj_t* scr);      // ui_tools.cpp
void openBeads();
void openSilence();
void openTorch();
void refreshSilence();
void beadsCountOne();
void beadsReset();
void openSettings();                // ui_settings.cpp
void refreshSettings();

}  // namespace ui::detail
