// Navigation, shared drawing helpers and the public entry points that are
// not a screen of their own.

#include "ui_internal.h"

namespace ui::detail {

State S;

namespace {
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
void polarPoint(float radius, float fraction, int32_t& x, int32_t& y) {
  const float angle = fraction * 2.0f * static_cast<float>(M_PI) - static_cast<float>(M_PI) / 2.0f;
  x = kCenter + static_cast<int32_t>(lroundf(radius * cosf(angle)));
  y = kCenter + static_cast<int32_t>(lroundf(radius * sinf(angle)));
}

void placePolar(lv_obj_t* obj, float radius, float fraction) {
  int32_t x = 0, y = 0;
  polarPoint(radius, fraction, x, y);
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

// ---------------------------------------------------------------- entrances

namespace {
// How long after its screen is built an entrance begins. After a wake the
// light is still rising: an entrance that began in the dark would be over
// before anyone saw it. After a change of screen it follows the wipe at
// once (LVGL's clock stands still while a wipe plays).
uint32_t entranceDelay() { return S.shownOnce ? 40 : 150; }

void setOpa(void* obj, int32_t v) { lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0); }
void setShift(void* obj, int32_t v) { lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(obj), v, 0); }
void setFigure(void* obj, int32_t v) { lv_label_set_text_fmt(static_cast<lv_obj_t*>(obj), "%d", static_cast<int>(v)); }
}  // namespace

bool entrance() { return S.entering && S.prefs.animations; }

void fadeIn(lv_obj_t* obj, uint32_t delayMs, uint32_t ms) {
  if (!entrance()) return;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_delay(&a, entranceDelay() + delayMs);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, setOpa);
  lv_anim_start(&a);
}

void riseIn(lv_obj_t* obj, uint32_t delayMs, int32_t from) {
  if (!entrance()) return;
  fadeIn(obj, delayMs, 260);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, from, 0);
  lv_anim_set_duration(&a, 260);
  lv_anim_set_delay(&a, entranceDelay() + delayMs);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, setShift);
  lv_anim_start(&a);
}

namespace {
struct Sweep {
  float from;
  float to;
};
}  // namespace

void sweepIn(lv_obj_t* arc, float from, float to, uint32_t delayMs, uint32_t ms) {
  if (!entrance()) return;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, arc);
  lv_anim_set_values(&a, 0, 1000);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_delay(&a, entranceDelay() + delayMs);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_user_data(&a, new Sweep{from, to});
  lv_anim_set_custom_exec_cb(&a, [](lv_anim_t* anim, int32_t v) {
    const Sweep* sweep = static_cast<const Sweep*>(lv_anim_get_user_data(anim));
    lv_arc_set_bg_end_angle(static_cast<lv_obj_t*>(anim->var),
                            (sweep->from + (sweep->to - sweep->from) * static_cast<float>(v) / 1000.0f) * 360.0f);
  });
  lv_anim_set_deleted_cb(&a, [](lv_anim_t* anim) { delete static_cast<Sweep*>(lv_anim_get_user_data(anim)); });
  lv_anim_start(&a);
}

void countUp(lv_obj_t* label, int value, uint32_t delayMs) {
  if (!entrance() || value < 2) return;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, label);
  lv_anim_set_values(&a, 0, value);
  lv_anim_set_duration(&a, value > 20 ? 700 : 450);
  lv_anim_set_delay(&a, entranceDelay() + delayMs);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, setFigure);
  lv_anim_start(&a);
}

void pulse(lv_obj_t* obj) {
  if (!S.prefs.animations) return;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, LV_OPA_20, LV_OPA_COVER);
  lv_anim_set_duration(&a, 260);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, setOpa);
  lv_anim_start(&a);
}

namespace {

// The Wi-Fi mark: a point and three arcs opening upwards, lit up to the
// signal of the last network joined. Its foot is at (cx, cy). When the day on
// screen has gone more than twelve hours without the API confirming it, the
// arcs go out and the point turns red.
void wifiMark(lv_obj_t* scr, int32_t cx, int32_t cy) {
  const Status& st = S.status;
  const bool stale = st.linked && st.syncedMinutesAgo > 12 * 60 && !st.syncing;
  const int bars = stale ? 0 : st.wifiBars;
  const lv_color_t lit = st.syncing ? theme::gold() : theme::dim();
  lv_obj_t* point = dot(scr, 5, stale ? theme::red() : (bars > 0 || st.syncing ? lit : theme::faint()), theme::bg(), 0);
  lv_obj_align(point, LV_ALIGN_TOP_LEFT, cx - 2, cy - 4);
  for (int i = 0; i < 3; i++) {
    const int32_t radius = 7 + i * 5;
    lv_obj_t* arc = lv_arc_create(scr);
    lv_obj_set_size(arc, radius * 2, radius * 2);
    lv_obj_align(arc, LV_ALIGN_TOP_LEFT, cx - radius, cy - radius - 1);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 318, 42);
    lv_arc_set_angles(arc, 0, 0);
    lv_obj_set_style_arc_width(arc, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, i < bars || st.syncing ? lit : theme::border(), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
    inert(arc);
    if (st.syncing) {
      // Looking for the network: the arcs light up one after the other.
      lv_anim_t a;
      lv_anim_init(&a);
      lv_anim_set_var(&a, arc);
      lv_anim_set_values(&a, LV_OPA_20, LV_OPA_COVER);
      lv_anim_set_duration(&a, 500);
      lv_anim_set_reverse_duration(&a, 500);
      lv_anim_set_delay(&a, static_cast<uint32_t>(i) * 180);
      lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
        lv_obj_set_style_arc_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), LV_PART_MAIN);
      });
      lv_anim_start(&a);
    }
  }
}

// The battery: an outline that fills with the charge. Gold and breathing
// while it charges, red when it is nearly out. Top-left corner at (x, y).
void batteryMark(lv_obj_t* scr, int32_t x, int32_t y) {
  const Status& st = S.status;
  const int percent = st.batteryPercent < 0 ? 0 : (st.batteryPercent > 100 ? 100 : st.batteryPercent);
  const lv_color_t ink = percent <= 10 && !st.charging ? theme::red() : (st.charging ? theme::gold() : theme::dim());
  lv_obj_t* body = dot(scr, 10, theme::bg(), ink, 2);
  lv_obj_set_size(body, 27, 14);
  lv_obj_set_style_radius(body, 4, 0);
  lv_obj_align(body, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_t* nub = dot(scr, 4, ink, ink, 0);
  lv_obj_set_size(nub, 3, 6);
  lv_obj_set_style_radius(nub, 1, 0);
  lv_obj_align(nub, LV_ALIGN_TOP_LEFT, x + 28, y + 4);
  const int32_t width = 2 + (19 * percent) / 100;
  lv_obj_t* fill = dot(scr, 4, ink, ink, 0);
  lv_obj_set_size(fill, width, 6);
  lv_obj_set_style_radius(fill, 1, 0);
  lv_obj_align(fill, LV_ALIGN_TOP_LEFT, x + 4, y + 4);
  if (st.charging) {
    breathe(fill, LV_OPA_30, [](void* obj, int32_t v) {
      lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
    });
  }
}

}  // namespace

// The top of every face: Wi-Fi, battery and its figure, small and quiet.
// `y` is where the row sits: under the rim, or inside the ring on the faces
// whose ring is thick enough to be in the way.
void statusMarkers(lv_obj_t* scr, int32_t y) {
  const Status& st = S.status;
  const bool battery = st.batteryPercent >= 0;
  char figure[12] = "";
  if (battery) snprintf(figure, sizeof(figure), "%d%%", st.batteryPercent);
  // Laid out around the middle: [wifi] [battery] [82%].
  int32_t figureWidth = 0;
  if (battery) {
    lv_point_t size;
    lv_text_get_size(&size, figure, theme::tiny(), 1, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    figureWidth = size.x;
  }
  const int32_t total = 34 + (battery ? 12 + 31 + 6 + figureWidth : 0);
  int32_t x = kCenter - total / 2;
  wifiMark(scr, x + 17, y + 15);
  x += 34;
  if (battery) {
    x += 12;
    batteryMark(scr, x, y + 1);
    x += 31 + 6;
    lv_obj_t* label = lv_label_create(scr);
    lv_label_set_text(label, figure);
    lv_obj_set_style_text_font(label, theme::tiny(), 0);
    lv_obj_set_style_text_letter_space(label, 1, 0);
    lv_obj_set_style_text_color(label, st.batteryPercent <= 10 && !st.charging ? theme::red() : theme::dim(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y - 1);
  }
}

bool pageAvailable(Page page) {
  // The celebration's page exists on the days that have one.
  return page != Page::Celebration || S.day.day.celebration.present();
}

// The carousel as it turns today. The screen the owner chose to wake on
// comes first; the others follow in their usual order, so choosing another
// home only moves "Agora" one place along, it does not hide it. Pages with
// nothing to show today are left out. Returns how many there are.
int carousel(Page out[kPageCount]) {
  static const Page usual[kPageCount] = {Page::Now, Page::Celebration, Page::Compass, Page::Readings,
                                         Page::Offices, Page::Week, Page::Menu};
  const Page home = homePage();
  int count = 0;
  out[count++] = home;
  for (Page page : usual) {
    if (page == home || !pageAvailable(page)) continue;
    out[count++] = page;
  }
  return count;
}

Page neighbour(Page page, bool forward) {
  Page order[kPageCount];
  const int count = carousel(order);
  int at = 0;
  for (int i = 0; i < count; i++) {
    if (order[i] == page) at = i;
  }
  return order[(at + (forward ? 1 : count - 1)) % count];
}

void pageDots(lv_obj_t* scr, Page page) {
  // Where you are in the carousel: a point per page there is today, on the
  // rim at three o'clock, outside every ring the faces draw.
  Page order[kPageCount];
  const int count = carousel(order);
  int here = 0;
  for (int i = 0; i < count; i++) {
    if (order[i] == page) here = i;
  }
  for (int i = 0; i < count; i++) {
    lv_obj_t* d = dot(scr, i == here ? 9 : 5, i == here ? theme::fg() : theme::faint(), theme::bg(), 0);
    const float span = 0.016f;
    placePolar(d, 227.0f, 0.25f + (i - (count - 1) / 2.0f) * span);
  }
}

lv_obj_t* scrollColumn(lv_obj_t* scr) {
  lv_obj_t* col = lv_obj_create(scr);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, kScreen, kScreen);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(col, 34, 0);
  lv_obj_set_style_pad_bottom(col, 170, 0);
  lv_obj_set_style_pad_row(col, 8, 0);
  lv_obj_set_scroll_dir(col, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(col, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scrollable(col, true);
  return col;
}

int16_t reminderFor(int office) {
  if (office < 0 || office > 3) return -1;
  if (S.prefs.remindersFromApp && S.day.hasReminders) return S.day.reminders[office];
  return S.prefs.reminder[office];
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
    case Page::Celebration: buildCelebration(scr); break;
    case Page::Compass: buildCompass(scr); break;
    case Page::Readings: buildReadings(scr); break;
    case Page::Offices: buildOffices(scr); break;
    case Page::Week: buildWeek(scr); break;
    case Page::Menu: buildMenu(scr); break;
  }
  // Drawn last, so they sit above whatever the page drew: the picture of a
  // celebration included.
  statusMarkers(scr, page == Page::Compass || page == Page::Offices ? 82 : 40);
  pageDots(scr, page);
  S.dirty[i] = false;
}

}  // namespace

// ---- screen changes
//
// A new screen is drawn whole out of sight while the old one stays lit, and
// is then laid over it strip by strip, in the direction of the gesture
// (wipe.h). Nothing on the glass goes dark on the way.
//
// Only the first screen after a wake is different: there is nothing lit to
// cover, so it is painted in the dark and the panel's light rises on it.

namespace {

void setFade(uint8_t value) {
  if (S.fade == value) return;
  S.fade = value;
  if (S.actions.brightnessMoved) S.actions.brightnessMoved();
}

void fadeTo(uint8_t target, uint32_t ms) {
  lv_anim_delete(&S.fade, nullptr);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, &S.fade);
  lv_anim_set_values(&a, S.fade, target);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, [](void*, int32_t v) { setFade(static_cast<uint8_t>(v)); });
  lv_anim_start(&a);
}

// After a wake: the screen is painted while the panel is still dark, and
// only then does the light come up. This runs on its own, a moment after
// the screen was built, outside whatever LVGL callback asked for it.
void riseAfterPaint(void*) {
  S.risePending = false;
  lv_refr_now(nullptr);
  fadeTo(255, 180);
}

// The change of screen itself. It runs a moment after the gesture, on its
// own: it draws a whole screen, which cannot be done from inside the LVGL
// callback (a tap, a swipe) that asked for it.
void swapScreens(void*) {
  S.swapPending = false;
  const std::function<void()> change = S.pendingChange;
  S.pendingChange = nullptr;
  if (!change) return;
  // While the light of a wake is still rising there is nothing lit to sweep
  // over: the new screen is simply drawn.
  const bool staged = S.fade == 255 && !S.risePending && S.actions.stageBegin && S.actions.stageReveal && S.actions.stageBegin();
  S.entering = true;
  change();
  S.entering = false;
  if (!staged) return;
  // Whatever the change drew on, the stage must hold the whole screen.
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(nullptr);
  S.actions.stageReveal(S.pendingWipe);
}

}  // namespace

void transition(std::function<void()> change, Wipe wipe) {
  if (!S.prefs.animations) {
    haltTransition();
    change();
    S.shownOnce = true;
    revealNow();
    return;
  }
  // Nothing on the glass yet (a wake): drawn in the dark, then the light.
  if (!S.shownOnce) {
    lv_anim_delete(&S.fade, nullptr);
    setFade(0);
    S.entering = true;
    change();
    S.entering = false;
    S.shownOnce = true;
    if (!S.risePending) {
      S.risePending = true;
      // No memory even for that call: the light comes up at once rather
      // than never.
      if (lv_async_call(riseAfterPaint, nullptr) != LV_RESULT_OK) {
        S.risePending = false;
        setFade(255);
      }
    }
    return;
  }
  S.pendingChange = change;
  S.pendingWipe = wipe;
  if (S.swapPending) return;
  S.swapPending = true;
  // The same, for the change itself: made here and now rather than never.
  if (lv_async_call(swapScreens, nullptr) != LV_RESULT_OK) swapScreens(nullptr);
}

void haltTransition() {
  lv_anim_delete(&S.fade, nullptr);
  if (S.risePending) lv_async_call_cancel(riseAfterPaint, nullptr);
  if (S.swapPending) lv_async_call_cancel(swapScreens, nullptr);
  S.risePending = false;
  S.swapPending = false;
  S.pendingChange = nullptr;
}

void revealNow() {
  if (S.fade == 255) return;
  lv_refr_now(nullptr);
  setFade(255);
}

Page homePage() {
  static const Page faces[] = {Page::Now, Page::Compass, Page::Readings, Page::Offices, Page::Week};
  return faces[S.prefs.homeFace < 5 ? S.prefs.homeFace : 0];
}

void showPage(Page page, Wipe wipe) {
  // A page with nothing to show today gives way to the first one.
  if (!pageAvailable(page)) page = Page::Now;
  // Leaving the celebration closes it: next time it opens on the picture.
  if (page != Page::Celebration) S.celebrationOpen = false;
  // What is on show is settled at once, so a second swipe before the swap
  // starts from the page being opened; the glass follows a moment later.
  S.mode = Mode::Pages;
  S.page = page;
  S.noticeOverDay = false;
  transition(
      [page]() {
        if (S.mode != Mode::Pages || S.page != page) return;
        // An entrance is played by building the page afresh.
        if (S.dirty[indexOf(page)] || S.entering) buildPage(page);
        load(S.pages[indexOf(page)]);
      },
      wipe);
}

void rebuildVisible() {
  for (bool& d : S.dirty) d = true;
  // A page change in flight builds the page itself when it lands.
  if (S.mode == Mode::Pages && !S.swapPending && lv_screen_active() == S.pages[indexOf(S.page)]) {
    buildPage(S.page);
  }
}

void goBack(Wipe wipe) {
  switch (S.mode) {
    case Mode::Beads:
    case Mode::Silence:
    case Mode::Torch:
    case Mode::Settings:
      showPage(Page::Menu, wipe);
      break;
    case Mode::SettingsSub:
      openSettings(wipe);
      break;
    case Mode::Reading:
      showPage(Page::Celebration, wipe);
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
  // Called again (the simulator does), it only takes the new settings.
  if (S.tool) return;
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
  } else if (S.mode == Mode::Pages && !pageAvailable(S.page)) {
    // The day turned under the celebration's page and the new one has none.
    showPage(Page::Now);
  } else if (S.mode == Mode::Pages) {
    rebuildVisible();
  }
}

void contentArrived(const String& slug) {
  if (slug != S.day.day.celebration.slug) return;
  S.dirty[static_cast<int>(Page::Celebration)] = true;
  celebrationContentArrived();
}

void setStatus(const Status& status) {
  const Status& old = S.status;
  const bool changed = old.clockValid != status.clockValid || old.minuteOfDay != status.minuteOfDay ||
                       old.batteryPercent != status.batteryPercent || old.charging != status.charging ||
                       old.syncing != status.syncing || old.linked != status.linked || old.wifiBars != status.wifiBars ||
                       (old.syncedMinutesAgo > 12 * 60) != (status.syncedMinutesAgo > 12 * 60);
  S.status = status;
  if (changed && S.dayShown) rebuildVisible();
  refreshSettings();
}

void goHome() {
  if (!S.dayShown) return;
  showPage(homePage(), Wipe::Close);
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
  // The new screen follows the finger.
  const Wipe wipe = dir == Swipe::Up ? Wipe::Up : (dir == Swipe::Down ? Wipe::Down : (dir == Swipe::Left ? Wipe::Left : Wipe::Right));
  switch (S.mode) {
    case Mode::Pages:
      showPage(neighbour(S.page, forward), wipe);
      return true;
    case Mode::Reading:
      // The text scrolls up and down; a swipe to the right closes it.
      if (dir != Swipe::Right) return false;
      goBack(wipe);
      return true;
    case Mode::Settings:
      // The list scrolls up and down; only a sideways swipe leaves it.
      if (dir != Swipe::Right) return false;
      goBack(wipe);
      return true;
    case Mode::SettingsSub:
      // Some of these scroll too (Wi-Fi, about).
      if (dir != Swipe::Right) return false;
      goBack(wipe);
      return true;
    case Mode::Beads:
    case Mode::Silence:
    case Mode::Torch:
      if (forward) return false;
      goBack(wipe);
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
      // Counting goes on by feel: the screen waits a minute per bead.
      hint.screenSeconds = 60;
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
    case Mode::Reading:
      // Someone is reading: the screen waits for them.
      hint.screenSeconds = 90;
      break;
    case Mode::Notice:
      if (S.noticeOverDay) hint.screenSeconds = S.noticeSeconds;
      break;
    case Mode::Pages:
      break;
  }
  return hint;
}

uint8_t brightnessScale() { return S.fade; }

bool changingScreen() { return S.swapPending || S.risePending || S.fade != 255; }

uint16_t beadsCount() { return S.beads; }

void setBeadsCount(uint16_t count) { S.beads = count; }

void setSilence(bool running, uint32_t remainingSeconds) {
  const bool changed = running != S.silenceRunning || remainingSeconds != S.silenceRemaining;
  const bool flipped = running != S.silenceRunning;
  S.silenceRunning = running;
  S.silenceRemaining = remainingSeconds;
  if (!changed) return;
  if (S.mode != Mode::Silence) {
    // The menu tile says how long is left, to the minute.
    if (flipped || remainingSeconds % 60 == 0) {
      S.dirty[static_cast<int>(Page::Menu)] = true;
      if (S.mode == Mode::Pages && S.page == Page::Menu) rebuildVisible();
    }
    return;
  }
  // Starting or ending changes the whole screen; a second going by only
  // changes the figure.
  if (flipped) openSilence();
  else refreshSilence();
}

}  // namespace ui
