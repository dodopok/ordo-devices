#include "ui.h"

#include <math.h>

#include "../app_config.h"
#include "board_pins.h"
#include "theme.h"

namespace ui {
namespace {

using theme::kCenter;
using theme::kScreen;

model::Today currentDay;
bool currentOffline = false;
int batteryPercent = -1;
bool batteryCharging = false;

lv_obj_t* screenCompass = nullptr;
lv_obj_t* screenReadings = nullptr;
lv_obj_t* screenOffices = nullptr;
lv_obj_t* screenSettings = nullptr;
lv_obj_t* screenNotice = nullptr;
SettingsHandlers handlers;
uint8_t standbyMinutes = cfg::kStandbyDefaultMinutes;
lv_obj_t* noticeTitle = nullptr;
lv_obj_t* noticeText = nullptr;
lv_obj_t* noticeSpinner = nullptr;
lv_obj_t* noticeQr = nullptr;
lv_obj_t* noticeCode = nullptr;
lv_obj_t* noticeFooter = nullptr;
lv_obj_t* noticeCountdown = nullptr;
Screen current = Screen::Compass;
bool dayShown = false;

// ---------------------------------------------------------------- helpers

lv_obj_t* blankScreen() {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, theme::bg(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

lv_obj_t* text(lv_obj_t* parent, const char* str, const lv_font_t* font, lv_color_t color, int32_t y,
               int32_t letterSpacing = 0) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, str);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_letter_space(label, letterSpacing, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(label, kScreen - 80);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
  return label;
}

lv_obj_t* dot(lv_obj_t* parent, int32_t size, lv_color_t fill, lv_color_t border, int32_t borderWidth) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_set_size(obj, size, size);
  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(obj, fill, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, border, 0);
  lv_obj_set_style_border_width(obj, borderWidth, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  return obj;
}

// Angle 0 at the top, clockwise, like a clock; LVGL's arc has 0 at 3 o'clock.
void placePolar(lv_obj_t* obj, float radius, float fraction) {
  const float angle = fraction * 2.0f * static_cast<float>(M_PI) - static_cast<float>(M_PI) / 2.0f;
  const int32_t x = kCenter + static_cast<int32_t>(lroundf(radius * cosf(angle)));
  const int32_t y = kCenter + static_cast<int32_t>(lroundf(radius * sinf(angle)));
  const int32_t half = lv_obj_get_style_width(obj, 0) / 2;
  lv_obj_set_pos(obj, x - half, y - half);
}

// A ring piece drawn with the arc's background, so it needs no knob and no
// indicator. `from`/`to` are fractions of the year starting at the top.
lv_obj_t* ringArc(lv_obj_t* parent, int32_t diameter, int32_t width, float from, float to, lv_color_t color,
                  lv_opa_t opa = LV_OPA_COVER) {
  lv_obj_t* arc = lv_arc_create(parent);
  lv_obj_set_size(arc, diameter, diameter);
  lv_obj_center(arc);
  lv_arc_set_rotation(arc, 270);
  lv_arc_set_bg_angles(arc, from * 360.0f, to * 360.0f);
  lv_arc_set_angles(arc, 0, 0);
  lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(arc, opa, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  return arc;
}

const char* monthShort(int month, const String& language) {
  static const char* pt[] = {"jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez"};
  static const char* en[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  static const char* es[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};
  if (month < 1 || month > 12) return "";
  if (language.startsWith("en")) return en[month - 1];
  if (language.startsWith("es")) return es[month - 1];
  return pt[month - 1];
}

String dateLine(const model::Today& day) {
  int y = 0, m = 0, d = 0;
  if (sscanf(day.date.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return day.date;
  char buf[32];
  if (day.language.startsWith("en")) {
    snprintf(buf, sizeof(buf), "%s %d", monthShort(m, day.language), d);
  } else {
    snprintf(buf, sizeof(buf), "%d de %s", d, monthShort(m, day.language));
  }
  return String(buf);
}

String upper(const String& s) {
  String out = s;
  out.toUpperCase();
  return out;
}

const char* tr(const char* pt, const char* en, const char* es) {
  if (currentDay.language.startsWith("en")) return en;
  if (currentDay.language.startsWith("es")) return es;
  return pt;
}

const char* officeShort(int i) {
  static const char* pt[] = {"M", "MD", "T", "C"};
  static const char* en[] = {"M", "MD", "E", "C"};
  if (currentDay.language.startsWith("en")) return en[i];
  return pt[i];
}

const char* officeName(int i) {
  static const char* pt[] = {"Manhã", "Meio-dia", "Tarde", "Completas"};
  static const char* en[] = {"Morning", "Midday", "Evening", "Compline"};
  static const char* es[] = {"Mañana", "Mediodía", "Tarde", "Completas"};
  if (currentDay.language.startsWith("en")) return en[i];
  if (currentDay.language.startsWith("es")) return es[i];
  return pt[i];
}

const char* slotName(const String& slot) {
  if (slot == "psalm") return tr("Salmo", "Psalm", "Salmo");
  if (slot == "psalm_alternative") return tr("Salmo alt.", "Psalm alt.", "Salmo alt.");
  if (slot == "first_reading") return tr("1ª leitura", "1st reading", "1ª lectura");
  if (slot == "second_reading") return tr("2ª leitura", "2nd reading", "2ª lectura");
  if (slot == "gospel") return tr("Evangelho", "Gospel", "Evangelio");
  return slot.c_str();
}

void clearChildren(lv_obj_t* scr) { lv_obj_clean(scr); }

void switchTo(Screen target, lv_screen_load_anim_t anim);
void buildSettings();

void gestureCb(lv_event_t* e) {
  const lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
  // Compass -> Readings -> Offices -> Settings -> Compass, and back.
  static const Screen order[] = {Screen::Compass, Screen::Readings, Screen::Offices, Screen::Settings};
  int index = 0;
  for (int i = 0; i < 4; i++) if (order[i] == current) index = i;
  if (dir == LV_DIR_TOP || dir == LV_DIR_LEFT) {
    switchTo(order[(index + 1) % 4], LV_SCREEN_LOAD_ANIM_MOVE_TOP);
  } else if (dir == LV_DIR_BOTTOM || dir == LV_DIR_RIGHT) {
    switchTo(order[(index + 3) % 4], LV_SCREEN_LOAD_ANIM_MOVE_BOTTOM);
  }
}

void switchTo(Screen target, lv_screen_load_anim_t anim) {
  current = target;
  lv_obj_t* scr = screenCompass;
  switch (target) {
    case Screen::Compass: scr = screenCompass; break;
    case Screen::Readings: scr = screenReadings; break;
    case Screen::Offices: scr = screenOffices; break;
    case Screen::Settings: scr = screenSettings; break;
  }
  lv_screen_load_anim(scr, anim, 220, 0, false);
}

// --------------------------------------------------------- status markers

void statusMarkers(lv_obj_t* scr) {
  // Battery, small and dim, at the top of the face.
  if (batteryPercent >= 0) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%s%d%%", batteryCharging ? LV_SYMBOL_CHARGE " " : "", batteryPercent);
    lv_obj_t* label = text(scr, buf, theme::tiny(), theme::dim(), 54, 1);
    lv_obj_set_width(label, LV_SIZE_CONTENT);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 54);
  }
  // A grey dot when the day shown is the last one fetched, not a live one.
  if (currentOffline) {
    lv_obj_t* d = dot(scr, 8, theme::dim(), theme::dim(), 0);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 40);
  }
}

// ---------------------------------------------------------------- compass

void buildCompass() {
  clearChildren(screenCompass);
  const model::Today& day = currentDay;
  const bool allDone = day.allDone();
  const lv_color_t accent = allDone ? theme::gold() : (day.hasColor ? day.color : theme::dim());

  // Track, then the seasons on top of it.
  ringArc(screenCompass, 412, 20, 0.0f, 1.0f, theme::track());
  for (const auto& seg : day.segments) {
    const bool active = day.progress >= seg.from && day.progress < seg.to;
    const float gap = 0.004f;
    ringArc(screenCompass, 412, 18, seg.from + gap, seg.to - gap, allDone ? theme::gold() : seg.color,
            active ? LV_OPA_COVER : LV_OPA_40);
  }
  for (const auto& feast : day.feasts) {
    lv_obj_t* d = dot(screenCompass, 7, feast.color, feast.color, 0);
    placePolar(d, 218.0f, feast.at);
  }

  // Today's marker on the ring, with a short tick towards the centre.
  lv_obj_t* marker = dot(screenCompass, 30, theme::bg(), accent, 3);
  placePolar(marker, 196.0f, day.progress);
  lv_obj_t* markerCore = dot(screenCompass, 12, accent, accent, 0);
  placePolar(markerCore, 196.0f, day.progress);

  // Centre
  if (allDone) {
    text(screenCompass, "LAUS DEO", theme::tiny(), theme::gold(), 142, 3);
  } else if (day.streakCurrent > 0) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%d %s", day.streakCurrent, tr("DIAS SEGUIDOS", "DAY STREAK", "DÍAS SEGUIDOS"));
    text(screenCompass, buf, theme::tiny(), theme::gold(), 142, 2);
  }
  text(screenCompass, upper(day.weekday).c_str(), theme::small(), theme::dim(), 176, 3);
  text(screenCompass, dateLine(day).c_str(), theme::display(), theme::fg(), 200);
  lv_obj_t* title = text(screenCompass, day.title.c_str(), theme::title(), accent, 262);
  lv_obj_set_width(title, 300);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 262);
  if (!day.season.isEmpty() && day.season != day.title) {
    text(screenCompass, day.season.c_str(), theme::tiny(), theme::dim(), 312, 1);
  }

  // The four offices as dots, the next one breathing.
  const int next = day.nextOfficeIndex();
  for (int i = 0; i < 4; i++) {
    const bool done = day.offices[i].done;
    const int32_t x = kCenter + (i - 2) * 40 + 20;
    lv_obj_t* d = dot(screenCompass, 18, done ? theme::gold() : theme::bg(), done ? theme::gold() : theme::dim(), 2);
    lv_obj_set_pos(d, x - 9, 342);
    if (!done && i == next) {
      lv_obj_set_style_border_color(d, theme::gold(), 0);
      lv_anim_t a;
      lv_anim_init(&a);
      lv_anim_set_var(&a, d);
      lv_anim_set_values(&a, LV_OPA_40, LV_OPA_COVER);
      lv_anim_set_duration(&a, 1600);
      lv_anim_set_playback_duration(&a, 1600);
      lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
        lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
      });
      lv_anim_start(&a);
    }
    lv_obj_t* letter = text(screenCompass, officeShort(i), theme::tiny(), done ? theme::fg() : theme::dim(), 366, 1);
    lv_obj_set_width(letter, 40);
    lv_obj_set_pos(letter, x - 20, 366);
  }

  statusMarkers(screenCompass);
}

// --------------------------------------------------------------- readings

void readingRow(lv_obj_t* parent, const char* label, const String& reference, int32_t y) {
  text(parent, upper(label).c_str(), theme::tiny(), theme::dim(), y, 2);
  text(parent, reference.c_str(), theme::reading(), theme::fg(), y + 18);
}

void buildReadings() {
  clearChildren(screenReadings);
  const model::Today& day = currentDay;
  const lv_color_t accent = day.hasColor ? day.color : theme::dim();

  ringArc(screenReadings, 444, 4, 0.0f, 1.0f, accent, LV_OPA_90);
  int y = 0, m = 0, d = 0;
  sscanf(day.date.c_str(), "%d-%d-%d", &y, &m, &d);
  char header[48];
  snprintf(header, sizeof(header), "%s · %d %s", tr("LEITURAS", "READINGS", "LECTURAS"), d,
           upper(monthShort(m, day.language)).c_str());
  text(screenReadings, header, theme::small(), accent, 84, 3);

  struct Row { const char* label; String ref; };
  std::vector<Row> rows;
  auto first = [](const std::vector<model::Reading>& list, const char* slot) -> String {
    for (const auto& r : list) if (r.slot == slot) return r.reference;
    return String();
  };
  const String morningPsalm = first(day.morning, "psalm");
  const String eveningPsalm = first(day.evening, "psalm");
  if (!morningPsalm.isEmpty()) rows.push_back({tr("Manhã", "Morning", "Mañana"), morningPsalm});
  if (!eveningPsalm.isEmpty()) rows.push_back({tr("Tarde", "Evening", "Tarde"), eveningPsalm});
  for (const auto& r : day.daily) {
    if (r.slot == "psalm" && (!morningPsalm.isEmpty() || !eveningPsalm.isEmpty())) continue;
    if (rows.size() >= 5) break;
    rows.push_back({slotName(r.slot), r.reference});
  }
  if (rows.empty()) {
    text(screenReadings, tr("Sem leituras para hoje", "No readings today", "Sin lecturas hoy"), theme::body(), theme::dim(), 220);
  }
  int32_t yPos = 128;
  for (const auto& row : rows) {
    readingRow(screenReadings, row.label, row.ref, yPos);
    yPos += 58;
  }
  statusMarkers(screenReadings);
}

// ---------------------------------------------------------------- offices

void buildOffices() {
  clearChildren(screenOffices);
  const model::Today& day = currentDay;
  const int next = day.nextOfficeIndex();
  const bool allDone = day.allDone();

  for (int i = 0; i < 4; i++) {
    const float gap = 0.02f;
    const bool done = day.offices[i].done;
    lv_obj_t* arc = ringArc(screenOffices, 400, 22, i * 0.25f + gap, (i + 1) * 0.25f - gap,
                            done ? (allDone ? theme::gold() : theme::green()) : lv_color_hex(0x2A2A2A));
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    if (!done && i == next) {
      lv_obj_set_style_arc_color(arc, theme::green(), LV_PART_MAIN);
      lv_anim_t a;
      lv_anim_init(&a);
      lv_anim_set_var(&a, arc);
      lv_anim_set_values(&a, LV_OPA_30, LV_OPA_COVER);
      lv_anim_set_duration(&a, 1600);
      lv_anim_set_playback_duration(&a, 1600);
      lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
        lv_obj_set_style_arc_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), LV_PART_MAIN);
      });
      lv_anim_start(&a);
    }
    lv_obj_t* name = text(screenOffices, upper(officeName(i)).c_str(), theme::tiny(), done ? theme::fg() : theme::dim(), 0, 2);
    lv_obj_set_width(name, 110);
    placePolar(name, 142.0f, i * 0.25f + 0.125f);
  }

  char streak[16];
  snprintf(streak, sizeof(streak), "%d", day.streakCurrent);
  text(screenOffices, streak, theme::display(), theme::fg(), 176);
  text(screenOffices, tr("DIAS SEGUIDOS", "DAY STREAK", "DÍAS SEGUIDOS"), theme::tiny(), theme::gold(), 236, 3);
  if (allDone) {
    text(screenOffices, tr("dia completo", "day complete", "día completo"), theme::label(), theme::dim(), 266);
  } else if (next >= 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %s", tr("próximo:", "next:", "siguiente:"), officeName(next));
    text(screenOffices, buf, theme::label(), theme::dim(), 266);
  }
  statusMarkers(screenOffices);
}

// --------------------------------------------------------------- settings

void standbyChoiceCb(lv_event_t* e) {
  const uint8_t minutes = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
  standbyMinutes = minutes;
  if (handlers.onStandbyMinutes) handlers.onStandbyMinutes(minutes);
  buildSettings();
}

void sleepNowCb(lv_event_t*) {
  if (handlers.onSleepNow) handlers.onSleepNow();
}

lv_obj_t* pill(lv_obj_t* parent, const char* label, bool selected, lv_event_cb_t cb, void* user) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, LV_SIZE_CONTENT, 40);
  lv_obj_set_style_radius(btn, 20, 0);
  lv_obj_set_style_pad_hor(btn, 16, 0);
  lv_obj_set_style_bg_color(btn, selected ? theme::gold() : theme::bg(), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(btn, selected ? theme::gold() : theme::dim(), 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_t* l = lv_label_create(btn);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, theme::small(), 0);
  lv_obj_set_style_text_color(l, selected ? theme::bg() : theme::fg(), 0);
  lv_obj_center(l);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user);
  return btn;
}

void buildSettings() {
  clearChildren(screenSettings);
  text(screenSettings, tr("AJUSTES", "SETTINGS", "AJUSTES"), theme::small(), theme::dim(), 84, 3);
  text(screenSettings, tr("Tela desliga após", "Screen off after", "Pantalla se apaga tras"), theme::body(), theme::fg(), 130);

  lv_obj_t* row = lv_obj_create(screenSettings);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, 400, 100);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 164);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_set_style_pad_row(row, 10, 0);
  for (uint8_t minutes : cfg::kStandbyChoices) {
    char label[12];
    if (minutes == 0) snprintf(label, sizeof(label), "%s", tr("Nunca", "Never", "Nunca"));
    else snprintf(label, sizeof(label), "%u min", minutes);
    pill(row, label, minutes == standbyMinutes, standbyChoiceCb, reinterpret_cast<void*>(static_cast<uintptr_t>(minutes)));
  }

  lv_obj_t* sleepBtn = pill(screenSettings, tr(LV_SYMBOL_POWER "  Desligar tela agora", LV_SYMBOL_POWER "  Screen off now",
                                                 LV_SYMBOL_POWER "  Apagar pantalla ahora"),
                            false, sleepNowCb, nullptr);
  lv_obj_align(sleepBtn, LV_ALIGN_TOP_MID, 0, 290);
  text(screenSettings, tr("Toque ou PWR acordam a tela", "Touch or PWR wakes the screen", "Toque o PWR despierta la pantalla"),
       theme::tiny(), theme::dim(), 344);

  char footer[64];
  snprintf(footer, sizeof(footer), "v%s · %s", ORDO_FIRMWARE_VERSION, ORDO_BOARD_NAME);
  text(screenSettings, footer, theme::tiny(), theme::dim(), 392);
  statusMarkers(screenSettings);
}

}  // namespace

// ----------------------------------------------------------------- public

void init() {
  screenCompass = blankScreen();
  screenReadings = blankScreen();
  screenOffices = blankScreen();
  screenSettings = blankScreen();
  screenNotice = blankScreen();
  for (lv_obj_t* scr : {screenCompass, screenReadings, screenOffices, screenSettings}) {
    lv_obj_add_event_cb(scr, gestureCb, LV_EVENT_GESTURE, nullptr);
  }
  buildSettings();
}

void showSplash() {
  dayShown = false;
  clearChildren(screenNotice);
  text(screenNotice, "Ordo", theme::display(), theme::fg(), 196);
  text(screenNotice, "bússola litúrgica", theme::small(), theme::dim(), 256, 1);
  lv_screen_load(screenNotice);
}

void showMessage(const String& title, const String& body, bool spinner) {
  dayShown = false;
  clearChildren(screenNotice);
  text(screenNotice, title.c_str(), theme::title(), theme::fg(), spinner ? 150 : 180);
  text(screenNotice, body.c_str(), theme::body(), theme::dim(), spinner ? 196 : 226);
  if (spinner) {
    lv_obj_t* s = lv_spinner_create(screenNotice);
    lv_spinner_set_anim_params(s, 1200, 200);
    lv_obj_set_size(s, 40, 40);
    lv_obj_set_style_arc_color(s, theme::track(), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s, theme::gold(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s, 4, LV_PART_INDICATOR);
    lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 290);
  }
  lv_screen_load(screenNotice);
}

void showWifiPortal(const String& ssid) {
  dayShown = false;
  clearChildren(screenNotice);
  text(screenNotice, "WI-FI", theme::small(), theme::dim(), 96, 3);
  text(screenNotice, "Conecte seu celular à rede", theme::body(), theme::fg(), 150);
  text(screenNotice, ssid.c_str(), theme::large(), theme::gold(), 190);
  text(screenNotice, "e escolha o Wi-Fi da bússola na página que abrir", theme::small(), theme::dim(), 250);
  // Phones join the network straight from this QR.
  String wifiQr = "WIFI:T:nopass;S:" + ssid + ";;";
  lv_obj_t* qr = lv_qrcode_create(screenNotice);
  lv_qrcode_set_size(qr, 110);
  lv_qrcode_set_dark_color(qr, theme::bg());
  lv_qrcode_set_light_color(qr, theme::paper());
  lv_qrcode_update(qr, wifiQr.c_str(), wifiQr.length());
  lv_obj_set_style_border_color(qr, theme::paper(), 0);
  lv_obj_set_style_border_width(qr, 6, 0);
  lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 300);
  lv_screen_load(screenNotice);
}

void showLink(const String& userCode, const String& verificationUri) {
  dayShown = false;
  clearChildren(screenNotice);
  text(screenNotice, "VINCULE AO ORDO", theme::small(), theme::dim(), 88, 3);

  noticeQr = lv_qrcode_create(screenNotice);
  lv_qrcode_set_size(noticeQr, 150);
  lv_qrcode_set_dark_color(noticeQr, theme::bg());
  lv_qrcode_set_light_color(noticeQr, theme::paper());
  lv_qrcode_update(noticeQr, verificationUri.c_str(), verificationUri.length());
  lv_obj_set_style_border_color(noticeQr, theme::paper(), 0);
  lv_obj_set_style_border_width(noticeQr, 8, 0);
  lv_obj_set_style_radius(noticeQr, 6, 0);
  lv_obj_align(noticeQr, LV_ALIGN_TOP_MID, 0, 116);

  noticeCode = text(screenNotice, userCode.c_str(), theme::code(), theme::fg(), 300, 4);
  noticeFooter = text(screenNotice, "Ordo › Configurações › Aparelhos", theme::small(), theme::dim(), 352);
  noticeCountdown = text(screenNotice, "", theme::tiny(), theme::dim(), 378, 1);
  lv_screen_load(screenNotice);
}

void setLinkCountdown(uint32_t secondsLeft) {
  if (!noticeCountdown || lv_screen_active() != screenNotice) return;
  char buf[32];
  snprintf(buf, sizeof(buf), "expira em %lu:%02lu", static_cast<unsigned long>(secondsLeft / 60),
           static_cast<unsigned long>(secondsLeft % 60));
  lv_label_set_text(noticeCountdown, buf);
}

void setToday(const model::Today& today, bool offline) {
  currentDay = today;
  currentOffline = offline;
  buildCompass();
  buildReadings();
  buildOffices();
  buildSettings();
  if (!dayShown) {
    dayShown = true;
    current = Screen::Compass;
    lv_screen_load_anim(screenCompass, LV_SCREEN_LOAD_ANIM_FADE_IN, 400, 0, false);
  }
}

void setBattery(int percent, bool charging) {
  if (percent == batteryPercent && charging == batteryCharging) return;
  batteryPercent = percent;
  batteryCharging = charging;
  if (dayShown) {
    buildCompass();
    buildReadings();
    buildOffices();
  }
}

void goHome() {
  if (dayShown && current != Screen::Compass) {
    switchTo(Screen::Compass, LV_SCREEN_LOAD_ANIM_FADE_IN);
  }
}

bool showingDay() { return dayShown; }

void setSettingsHandlers(SettingsHandlers h) { handlers = h; }

void setStandbyMinutes(uint8_t minutes) {
  standbyMinutes = minutes;
  buildSettings();
}

}  // namespace ui
