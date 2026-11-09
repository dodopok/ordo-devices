// Navigation, shared drawing helpers and the public entry points that are
// not a screen of their own.

#include "ui_internal.h"

namespace ui::detail {

State S;

namespace {
constexpr int kPageCount = 6;
int indexOf(Page page) { return static_cast<int>(page); }
}  // namespace

// ------------------------------------------------------------------ helpers

// Drawn, never touched: not clickable, not scrollable.
void inert(lv_obj_t* obj) {
  lv_obj_set_clickable(obj, false);
  lv_obj_set_scrollable(obj, false);
}

lv_obj_t* blankScreen() {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(scr, theme::bg(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_set_scrollable(scr, false);
  return scr;
}

void clear(lv_obj_t* scr) {
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, theme::bg(), 0);
  lv_obj_set_scrollable(scr, false);
  lv_obj_remove_event_cb(scr, nullptr);
}

lv_obj_t* text(lv_obj_t* parent, const char* str, const lv_font_t* font, lv_color_t color, int32_t y,
               int32_t letterSpacing, int32_t width) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, str);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_letter_space(label, letterSpacing, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(label, width);
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_clickable(label, false);
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
  inert(obj);
  return obj;
}

// Angle 0 at the top, clockwise, like a clock; LVGL's arc has 0 at 3 o'clock.
void placePolar(lv_obj_t* obj, float radius, float fraction) {
  const float angle = fraction * 2.0f * static_cast<float>(M_PI) - static_cast<float>(M_PI) / 2.0f;
  const int32_t x = kCenter + static_cast<int32_t>(lroundf(radius * cosf(angle)));
  const int32_t y = kCenter + static_cast<int32_t>(lroundf(radius * sinf(angle)));
  lv_obj_update_layout(obj);
  lv_obj_align(obj, LV_ALIGN_TOP_LEFT, x - lv_obj_get_width(obj) / 2, y - lv_obj_get_height(obj) / 2);
}

// A ring piece drawn with the arc's background, so it needs no knob and no
// indicator. `from`/`to` are fractions of the year starting at the top.
lv_obj_t* ringArc(lv_obj_t* parent, int32_t diameter, int32_t width, float from, float to, lv_color_t color,
                  lv_opa_t opa) {
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
  inert(arc);
  return arc;
}

lv_obj_t* pill(lv_obj_t* parent, const char* label, bool selected, lv_event_cb_t cb, void* user, lv_color_t accent) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, LV_SIZE_CONTENT, 48);
  lv_obj_set_style_radius(btn, 24, 0);
  lv_obj_set_style_pad_hor(btn, 22, 0);
  lv_obj_set_style_bg_color(btn, selected ? accent : theme::surface(), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(btn, selected ? accent : theme::border(), 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_t* l = lv_label_create(btn);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, theme::body(), 0);
  lv_obj_set_style_text_color(l, selected ? theme::bg() : theme::fg(), 0);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user);
  return btn;
}

void breathe(lv_obj_t* obj, lv_opa_t low, lv_anim_exec_xcb_t exec) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, low, LV_OPA_COVER);
  lv_anim_set_duration(&a, 1600);
  lv_anim_set_reverse_duration(&a, 1600);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_exec_cb(&a, exec);
  lv_anim_start(&a);
}

void statusMarkers(lv_obj_t* scr) {
  // Battery, small and dim, at the top of the face: a figure only when it is
  // worth a look (charging, or under a third).
  const Status& st = S.status;
  if (st.batteryPercent >= 0 && (st.charging || st.batteryPercent <= 30)) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%d%%", st.batteryPercent);
    lv_obj_t* label = text(scr, buf, theme::tiny(), st.batteryPercent <= 10 ? theme::red() : theme::dim(), 50, 1, 80);
    if (st.charging) {
      lv_obj_t* bolt = lv_label_create(scr);
      lv_label_set_text(bolt, LV_SYMBOL_CHARGE);
      lv_obj_set_style_text_font(bolt, theme::icons(), 0);
      lv_obj_set_style_text_color(bolt, theme::gold(), 0);
      lv_obj_align_to(bolt, label, LV_ALIGN_OUT_LEFT_MID, 22, 0);
    }
  }
  // A grey dot when the day on show has not been confirmed for a while.
  if (st.syncing) {
    lv_obj_t* d = dot(scr, 8, theme::gold(), theme::gold(), 0);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 34);
    breathe(d, LV_OPA_30, [](void* obj, int32_t v) {
      lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
    });
  } else if (st.syncedMinutesAgo > 12 * 60) {
    lv_obj_t* d = dot(scr, 8, theme::faint(), theme::faint(), 0);
    lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 34);
  }
}

void pageDots(lv_obj_t* scr, Page page) {
  // Where you are in the carousel: six points on the rim, at three o'clock,
  // outside every ring the faces draw.
  const int count = kPageCount;
  for (int i = 0; i < count; i++) {
    const bool here = i == indexOf(page);
    lv_obj_t* d = dot(scr, here ? 9 : 5, here ? theme::fg() : theme::faint(), theme::bg(), 0);
    const float span = 0.016f;
    placePolar(d, 227.0f, 0.25f + (i - (count - 1) / 2.0f) * span);
  }
}

void load(lv_obj_t* scr) {
  if (lv_screen_active() != scr) lv_screen_load(scr);
}

void backChip(lv_obj_t* scr) {
  lv_obj_t* btn = lv_button_create(scr);
  lv_obj_set_size(btn, 132, 44);
  lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 26);
  lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_t* l = lv_label_create(btn);
  char buf[32];
  snprintf(buf, sizeof(buf), "‹  %s", tr("voltar", "back", "volver"));
  lv_label_set_text(l, buf);
  lv_obj_set_style_text_font(l, theme::small(), 0);
  lv_obj_set_style_text_color(l, theme::dim(), 0);
  lv_obj_center(l);
  lv_obj_add_event_cb(btn, [](lv_event_t*) { goBack(); }, LV_EVENT_CLICKED, nullptr);
}

// --------------------------------------------------------------------- words

const char* tr(const char* pt, const char* en, const char* es) {
  if (S.day.language.startsWith("en")) return en;
  if (S.day.language.startsWith("es")) return es;
  return pt;
}

const char* officeName(int i) {
  static const char* pt[] = {"Manhã", "Meio-dia", "Tarde", "Completas"};
  static const char* en[] = {"Morning", "Midday", "Evening", "Compline"};
  static const char* es[] = {"Mañana", "Mediodía", "Tarde", "Completas"};
  if (i < 0 || i > 3) return "";
  return tr(pt[i], en[i], es[i]);
}

const char* officeShort(int i) {
  static const char* pt[] = {"M", "MD", "T", "C"};
  static const char* en[] = {"M", "MD", "E", "C"};
  if (i < 0 || i > 3) return "";
  return S.day.language.startsWith("en") ? en[i] : pt[i];
}

const char* slotName(const String& slot) {
  if (slot == "psalm") return tr("Salmo", "Psalm", "Salmo");
  if (slot == "psalm_alternative") return tr("Salmo alt.", "Psalm alt.", "Salmo alt.");
  if (slot == "first_reading") return tr("1ª leitura", "1st reading", "1ª lectura");
  if (slot == "second_reading") return tr("2ª leitura", "2nd reading", "2ª lectura");
  if (slot == "gospel") return tr("Evangelho", "Gospel", "Evangelio");
  return slot.c_str();
}

const char* monthShort(int month) {
  static const char* pt[] = {"jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez"};
  static const char* en[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  static const char* es[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};
  if (month < 1 || month > 12) return "";
  return tr(pt[month - 1], en[month - 1], es[month - 1]);
}

const char* weekdayShort(int weekday) {
  static const char* pt[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SÁB"};
  static const char* en[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  static const char* es[] = {"DOM", "LUN", "MAR", "MIÉ", "JUE", "VIE", "SÁB"};
  if (weekday < 0 || weekday > 6) return "";
  return tr(pt[weekday], en[weekday], es[weekday]);
}

// Capitals for the letter-spaced captions. Arduino's toUpperCase() only knows
// ASCII; the accented vowels of Portuguese and Spanish are two-byte UTF-8
// sequences starting with 0xC3, and their capital is 0x20 below.
String upper(const String& s) {
  String out;
  const char* p = s.c_str();
  while (*p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c == 0xC3 && p[1]) {
      unsigned char d = static_cast<unsigned char>(p[1]);
      if (d >= 0xA0 && d <= 0xBE && d != 0xB7) d -= 0x20;
      out += static_cast<char>(c);
      out += static_cast<char>(d);
      p += 2;
      continue;
    }
    out += static_cast<char>((c >= 'a' && c <= 'z') ? c - 32 : c);
    p++;
  }
  return out;
}

String dateLine(const model::DayInfo& day) {
  int y = 0, m = 0, d = 0;
  if (day.days < 0) return day.date;
  model::civilFromDays(day.days, y, m, d);
  char buf[32];
  if (S.day.language.startsWith("en")) snprintf(buf, sizeof(buf), "%s %d", monthShort(m), d);
  else snprintf(buf, sizeof(buf), "%d de %s", d, monthShort(m));
  return String(buf);
}

String clockText(int minuteOfDay) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", (minuteOfDay / 60) % 24, minuteOfDay % 60);
  return String(buf);
}

lv_color_t accentOf(const model::Today& day) {
  if (day.allDone()) return theme::gold();
  return day.day.hasColor ? day.day.color : theme::dim();
}

// --------------------------------------------------------------- navigation

namespace {

void buildPage(Page page) {
  const int i = indexOf(page);
  lv_obj_t* scr = S.pages[i];
  clear(scr);
  switch (page) {
    case Page::Now: buildNow(scr); break;
    case Page::Compass: buildCompass(scr); break;
    case Page::Readings: buildReadings(scr); break;
    case Page::Offices: buildOffices(scr); break;
    case Page::Week: buildWeek(scr); break;
    case Page::Menu: buildMenu(scr); break;
  }
  if (page != Page::Menu) statusMarkers(scr);
  pageDots(scr, page);
  S.dirty[i] = false;
}

}  // namespace

void showPage(Page page) {
  S.mode = Mode::Pages;
  S.page = page;
  S.noticeOverDay = false;
  if (S.dirty[indexOf(page)]) buildPage(page);
  load(S.pages[indexOf(page)]);
}

void rebuildVisible() {
  for (bool& d : S.dirty) d = true;
  if (S.mode == Mode::Pages) {
    buildPage(S.page);
  }
}

void goBack() {
  switch (S.mode) {
    case Mode::Beads:
    case Mode::Silence:
    case Mode::Torch:
    case Mode::Settings:
      showPage(Page::Menu);
      break;
    case Mode::SettingsSub:
      openSettings();
      break;
    case Mode::Notice:
      if (S.noticeOverDay) dismissNotice();
      break;
    case Mode::Pages:
      break;
  }
}

}  // namespace ui::detail

// ============================================================ public (ui::)

namespace ui {

using namespace detail;

void init(const Prefs& prefs, Actions actions) {
  S.prefs = prefs;
  S.actions = actions;
  for (auto& scr : S.pages) scr = blankScreen();
  S.tool = blankScreen();
  S.settings = blankScreen();
  S.sub = blankScreen();
  S.notice = blankScreen();
}

void setToday(const model::Today& today) {
  S.day = today;
  const bool first = !S.dayShown;
  S.dayShown = true;
  for (bool& d : S.dirty) d = true;
  if (first && (S.mode == Mode::Notice && !S.noticeOverDay)) {
    goHome();
  } else if (S.mode == Mode::Pages) {
    rebuildVisible();
  }
}

void setStatus(const Status& status) {
  const Status& old = S.status;
  const bool changed = old.clockValid != status.clockValid || old.minuteOfDay != status.minuteOfDay ||
                       old.batteryPercent != status.batteryPercent || old.charging != status.charging ||
                       old.syncing != status.syncing || old.linked != status.linked ||
                       (old.syncedMinutesAgo > 12 * 60) != (status.syncedMinutesAgo > 12 * 60);
  S.status = status;
  if (changed && S.dayShown) rebuildVisible();
}

void goHome() {
  if (!S.dayShown) return;
  showPage(S.prefs.homeFace == 1 ? Page::Compass : Page::Now);
}

void clearDay() {
  S.day = model::Today();
  S.dayShown = false;
  for (bool& d : S.dirty) d = true;
}

bool showingDay() { return S.dayShown && (S.mode != Mode::Notice || S.noticeOverDay); }

bool showingNotice() { return S.mode == Mode::Notice; }

bool onSwipe(Swipe dir) {
  const bool forward = dir == Swipe::Up || dir == Swipe::Left;
  switch (S.mode) {
    case Mode::Pages: {
      const int count = 6;
      const int next = (static_cast<int>(S.page) + (forward ? 1 : count - 1)) % count;
      showPage(static_cast<Page>(next));
      return true;
    }
    case Mode::Settings:
      // The list scrolls up and down; only a sideways swipe leaves it.
      if (dir != Swipe::Right) return false;
      goBack();
      return true;
    case Mode::SettingsSub:
      // Some of these scroll too (Wi-Fi, about).
      if (dir != Swipe::Right) return false;
      goBack();
      return true;
    case Mode::Beads:
    case Mode::Silence:
    case Mode::Torch:
      if (forward) return false;
      goBack();
      return true;
    case Mode::Notice:
      if (!S.noticeOverDay) return false;
      dismissNotice();
      return true;
  }
  return false;
}

void onBootPress() {
  switch (S.mode) {
    case Mode::Pages:
      onSwipe(Swipe::Up);
      break;
    case Mode::Beads:
      beadsCountOne();
      break;
    default:
      goBack();
      break;
  }
}

void onBootLongPress() {
  if (S.mode == Mode::Beads) beadsReset();
  else if (S.mode == Mode::Pages) goHome();
}

PowerHint powerHint() {
  PowerHint hint;
  switch (S.mode) {
    case Mode::Beads:
      // Counting goes on by feel: a dim screen that waits a minute per bead.
      hint.screenSeconds = 60;
      hint.brightness = 40;
      break;
    case Mode::Torch:
      hint.screenSeconds = 60;
      hint.brightness = 255;
      break;
    case Mode::Silence:
      hint.screenSeconds = 20;
      break;
    case Mode::Settings:
    case Mode::SettingsSub:
      hint.screenSeconds = 45;
      break;
    case Mode::Notice:
      if (S.noticeOverDay) hint.screenSeconds = S.noticeSeconds;
      break;
    case Mode::Pages:
      break;
  }
  return hint;
}

uint16_t beadsCount() { return S.beads; }

void setBeadsCount(uint16_t count) { S.beads = count; }

void setSilence(bool running, uint32_t remainingSeconds) {
  const bool changed = running != S.silenceRunning || remainingSeconds != S.silenceRemaining;
  S.silenceRunning = running;
  S.silenceRemaining = remainingSeconds;
  if (changed && S.mode == Mode::Silence) openSilence();
}

}  // namespace ui
