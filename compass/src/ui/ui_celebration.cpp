// The celebration of the day: its picture filling the glass, then, at a
// touch, the picture above and the words below, and from there the whole
// text for whoever wants to read on.
//
// The picture and the text are not in the day's payload. The controller
// fetches them from the content host when it synchronises and keeps them on
// the device; here they are asked for by slug, and a celebration whose
// picture has not arrived (or does not exist) is set in type alone.

#include "ui_internal.h"

namespace ui::detail {
namespace {

// The picture as LVGL takes it: a header over the controller's pixels.
lv_image_dsc_t artImage;
bool hasArt = false;

// The celebration's text, kept while the day lasts.
CelebrationText words;
String wordsSlug;

// What moves when the page opens.
lv_obj_t* curtain = nullptr;  // a fade to black and the black under it
lv_obj_t* texts = nullptr;    // caption, name, byline, summary
lv_obj_t* summary = nullptr;
lv_obj_t* readPill = nullptr;
lv_obj_t* hint = nullptr;

// Closed: the picture fills the glass and fades to black near the bottom,
// where the name sits. Open: the fade rises to the middle and the words come
// up with it. The fade has to be black by the first line of text: a picture
// can be as bright as a white wall.
constexpr int32_t kCurtainClosed = 198;
constexpr int32_t kCurtainOpen = 84;
constexpr int32_t kFade = 150;
constexpr int32_t kTextsOpen = 226;
// The last line of the name and byline ends here when closed.
constexpr int32_t kTextsBottomClosed = 416;
int32_t textsClosed = 300;

const CelebrationText& textFor(const String& slug) {
  if (slug != wordsSlug || !words.found) {
    words = S.actions.celebrationText && !slug.isEmpty() ? S.actions.celebrationText(slug) : CelebrationText();
    wordsSlug = slug;
  }
  return words;
}

const char* typeName(const String& type) {
  if (type == "principal_feast") return tr("FESTA PRINCIPAL", "PRINCIPAL FEAST", "FIESTA PRINCIPAL");
  if (type == "major_holy_day") return tr("DIA SANTO", "HOLY DAY", "DÍA SANTO");
  if (type == "festival") return tr("FESTA", "FESTIVAL", "FIESTA");
  if (type == "lesser_feast") return tr("FESTA MENOR", "LESSER FEAST", "FIESTA MENOR");
  if (type == "commemoration") return tr("COMEMORAÇÃO", "COMMEMORATION", "CONMEMORACIÓN");
  return tr("COMEMORAÇÃO", "COMMEMORATION", "CONMEMORACIÓN");
}

lv_obj_t* fade(lv_obj_t* parent, int32_t y, int32_t height, lv_opa_t top, lv_opa_t bottom, int32_t reachedAt) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_size(obj, kScreen, height);
  lv_obj_set_pos(obj, 0, y);
  lv_obj_set_style_bg_color(obj, theme::bg(), 0);
  lv_obj_set_style_bg_grad_color(obj, theme::bg(), 0);
  lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_main_opa(obj, top, 0);
  lv_obj_set_style_bg_grad_opa(obj, bottom, 0);
  lv_obj_set_style_bg_main_stop(obj, 0, 0);
  lv_obj_set_style_bg_grad_stop(obj, reachedAt, 0);
  inert(obj);
  return obj;
}

// Everything that depends on how far open the page is, 0 (the picture) to
// 1000 (the words).
void place(int32_t v) {
  if (!curtain) return;
  lv_obj_set_y(curtain, kCurtainClosed + (kCurtainOpen - kCurtainClosed) * v / 1000);
  lv_obj_set_y(texts, textsClosed + (kTextsOpen - textsClosed) * v / 1000);
  // The hint leaves first and the summary arrives last, once the words have
  // risen clear of where the button sits: the three never share the glass.
  const int32_t late = v < 550 ? 0 : (v - 550) * 255 / 450;
  const int32_t early = v > 350 ? 0 : (350 - v) * 255 / 350;
  if (summary) lv_obj_set_style_opa(summary, static_cast<lv_opa_t>(late), 0);
  if (readPill) {
    lv_obj_set_style_opa(readPill, static_cast<lv_opa_t>(late), 0);
    lv_obj_set_clickable(readPill, v > 800);
  }
  if (hint) lv_obj_set_style_opa(hint, static_cast<lv_opa_t>(early), 0);
}

void animateTo(bool open) {
  S.celebrationOpen = open;
  if (!curtain) return;
  lv_anim_delete(curtain, nullptr);
  if (!S.prefs.animations) {
    place(open ? 1000 : 0);
    return;
  }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, curtain);
  lv_anim_set_values(&a, open ? 0 : 1000, open ? 1000 : 0);
  lv_anim_set_duration(&a, 340);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, [](void*, int32_t v) { place(v); });
  lv_anim_start(&a);
}

lv_obj_t* line(lv_obj_t* parent, const char* str, const lv_font_t* font, lv_color_t color, int32_t width, int32_t spacing = 0) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, str);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_letter_space(label, spacing, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(label, width);
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_clickable(label, false);
  return label;
}

// At most `lines` lines of a label: what does not fit ends in an ellipsis.
void clamp(lv_obj_t* label, int lines) {
  const lv_font_t* font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
  const int32_t lineHeight = lv_font_get_line_height(font) + lv_obj_get_style_text_line_space(label, LV_PART_MAIN);
  const int32_t padding = lv_obj_get_style_pad_top(label, LV_PART_MAIN) + lv_obj_get_style_pad_bottom(label, LV_PART_MAIN);
  lv_obj_update_layout(label);
  if (lv_obj_get_height(label) <= lineHeight * lines + padding) return;
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_height(label, lineHeight * lines + padding);
}

lv_obj_t* column(lv_obj_t* scr, int32_t y, int32_t width) {
  lv_obj_t* col = lv_obj_create(scr);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, width, LV_SIZE_CONTENT);
  lv_obj_align(col, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, 5, 0);
  inert(col);
  return col;
}

lv_obj_t* readButton(lv_obj_t* scr, int32_t y) {
  lv_obj_t* btn = pill(scr, tr("Ler tudo", "Read on", "Leer todo"), false, [](lv_event_t*) { openReading(); }, nullptr);
  lv_obj_set_height(btn, 40);
  lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
  return btn;
}

String caption(const model::DayInfo& day) {
  int y = 0, m = 0, d = 0;
  model::civilFromDays(day.days, y, m, d);
  char buf[72];
  snprintf(buf, sizeof(buf), "%d %s  ·  %s", d, upper(monthShort(m)).c_str(), typeName(day.celebration.type));
  return String(buf);
}

// No picture: the celebration in type, inside a ring of the day's colour.
void buildInType(lv_obj_t* scr, const model::DayInfo& day, const CelebrationText& text) {
  const lv_color_t accent = day.hasColor ? day.color : theme::gold();
  ringArc(scr, 440, 4, 0.0f, 1.0f, accent, LV_OPA_90);
  lv_obj_t* col = column(scr, 96, 320);
  line(col, caption(day).c_str(), theme::tiny(), accent, 300, 2);
  clamp(line(col, day.celebration.name.c_str(), theme::large(), theme::fg(), 310), 2);
  const String byline = day.celebration.byline();
  if (!byline.isEmpty()) clamp(line(col, byline.c_str(), theme::small(), theme::dim(), 290), 2);
  if (text.found && !text.summary.isEmpty()) {
    lv_obj_t* s = line(col, text.summary.c_str(), theme::small(), theme::fg(), 300);
    lv_obj_set_style_pad_top(s, 8, 0);
    clamp(s, 5);
    readButton(scr, 390);
  } else {
    lv_obj_t* s = line(col, tr("o texto chega na próxima sincronização", "the text arrives with the next sync",
                               "el texto llega con la próxima sincronización"),
                       theme::italic(), theme::dim(), 280);
    lv_obj_set_style_pad_top(s, 10, 0);
  }
  riseIn(col, 40);
}

void buildWithArt(lv_obj_t* scr, const model::DayInfo& day, const CelebrationText& text, const Art& art) {
  const lv_color_t accent = day.hasColor ? day.color : theme::gold();

  artImage.header.magic = LV_IMAGE_HEADER_MAGIC;
  artImage.header.cf = LV_COLOR_FORMAT_RGB565;
  artImage.header.w = art.side;
  artImage.header.h = art.side;
  artImage.header.stride = static_cast<uint32_t>(art.side) * 2;
  artImage.data = art.pixels;
  artImage.data_size = static_cast<uint32_t>(art.side) * art.side * 2;

  lv_obj_t* image = lv_image_create(scr);
  lv_image_set_src(image, &artImage);
  lv_obj_align(image, LV_ALIGN_CENTER, 0, 0);
  inert(image);

  // A shade under the Wi-Fi and the battery, so they read on a bright sky.
  fade(scr, 0, 118, LV_OPA_80, LV_OPA_TRANSP, 255);

  // The curtain: a fade to black, and black from there down. It is taller
  // than the glass so that it still covers the bottom when it rises.
  curtain = lv_obj_create(scr);
  lv_obj_remove_style_all(curtain);
  lv_obj_set_size(curtain, kScreen, 420);
  lv_obj_set_pos(curtain, 0, kCurtainClosed);
  inert(curtain);
  fade(curtain, 0, kFade, LV_OPA_TRANSP, LV_OPA_COVER, 225);
  lv_obj_t* floor = lv_obj_create(curtain);
  lv_obj_remove_style_all(floor);
  lv_obj_set_size(floor, kScreen, 420 - kFade);
  lv_obj_set_pos(floor, 0, kFade);
  lv_obj_set_style_bg_color(floor, theme::bg(), 0);
  lv_obj_set_style_bg_opa(floor, LV_OPA_COVER, 0);
  inert(floor);

  lv_obj_t* rim = ringArc(scr, 452, 3, 0.0f, 1.0f, accent, LV_OPA_90);
  LV_UNUSED(rim);

  texts = column(scr, kTextsOpen, 310);
  line(texts, caption(day).c_str(), theme::tiny(), accent, 300, 2);
  lv_obj_t* name = line(texts, day.celebration.name.c_str(), theme::large(), theme::fg(), 306);
  clamp(name, 2);
  lv_obj_update_layout(name);
  const bool longName = lv_obj_get_height(name) > 44;
  const String byline = day.celebration.byline();
  if (!byline.isEmpty()) clamp(line(texts, byline.c_str(), theme::small(), lv_color_hex(0xC9C7C2), 284), longName ? 1 : 2);
  if (text.found && !text.summary.isEmpty()) {
    summary = line(texts, text.summary.c_str(), theme::small(), theme::fg(), 292);
    lv_obj_set_style_pad_top(summary, 4, 0);
    clamp(summary, longName ? 2 : 3);
    readPill = readButton(scr, 392);
  } else {
    summary = line(texts, tr("o texto chega na próxima sincronização", "the text arrives with the next sync",
                             "el texto llega con la próxima sincronización"),
                   theme::italic(), theme::dim(), 270);
  }
  // Closed, the column sits as low as its name and byline allow; the summary
  // under them is not shown until it opens.
  lv_obj_update_layout(texts);
  textsClosed = kTextsBottomClosed - lv_obj_get_y(summary);
  hint = text.found ? line(scr, tr("toque para ler", "tap to read", "toque para leer"), theme::tiny(), theme::faint(), 200) : nullptr;
  if (hint) lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 428);

  place(S.celebrationOpen ? 1000 : 0);
  riseIn(texts, 80);

  // A touch on the glass opens and closes it; the button has its own.
  lv_obj_set_clickable(scr, true);
  lv_obj_add_event_cb(scr, [](lv_event_t*) { animateTo(!S.celebrationOpen); }, LV_EVENT_CLICKED, nullptr);
}

void buildReading(lv_obj_t* scr) {
  const model::DayInfo& day = S.day.day;
  const lv_color_t accent = day.hasColor ? day.color : theme::gold();
  const CelebrationText& text = textFor(day.celebration.slug);

  lv_obj_t* col = scrollColumn(scr);
  lv_obj_t* back = lv_button_create(col);
  lv_obj_set_size(back, 150, 44);
  lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_width(back, 0, 0);
  lv_obj_set_style_border_width(back, 0, 0);
  lv_obj_t* l = lv_label_create(back);
  char buf[32];
  snprintf(buf, sizeof(buf), "‹  %s", tr("voltar", "back", "volver"));
  lv_label_set_text(l, buf);
  lv_obj_set_style_text_font(l, theme::small(), 0);
  lv_obj_set_style_text_color(l, theme::dim(), 0);
  lv_obj_center(l);
  lv_obj_add_event_cb(back, [](lv_event_t*) { goBack(); }, LV_EVENT_CLICKED, nullptr);

  line(col, day.celebration.name.c_str(), theme::large(), theme::fg(), 300);
  const String byline = day.celebration.byline();
  if (!byline.isEmpty()) line(col, byline.c_str(), theme::small(), theme::dim(), 290);

  if (!text.found) {
    lv_obj_t* none = line(col, tr("O texto chega na próxima sincronização.", "The text arrives with the next sync.",
                                  "El texto llega con la próxima sincronización."),
                          theme::italic(), theme::dim(), 280);
    lv_obj_set_style_pad_top(none, 14, 0);
    return;
  }
  for (const TextBlock& block : text.body) {
    if (block.heading) {
      lv_obj_t* h = line(col, upper(block.text).c_str(), theme::tiny(), accent, 296, 2);
      lv_obj_set_style_pad_top(h, 16, 0);
    } else {
      // Running text reads from a straight left edge, even on a round glass.
      lv_obj_t* p = line(col, block.text.c_str(), theme::body(), theme::fg(), 296);
      lv_obj_set_style_text_align(p, LV_TEXT_ALIGN_LEFT, 0);
      lv_obj_set_style_text_line_space(p, 4, 0);
      lv_obj_set_style_pad_top(p, 4, 0);
    }
  }
  if (text.more) {
    lv_obj_t* more = line(col, tr("continua no app Ordo", "it goes on in the Ordo app", "continúa en la app Ordo"), theme::italic(),
                          theme::dim(), 280);
    lv_obj_set_style_pad_top(more, 14, 0);
  }
}

}  // namespace

void buildCelebration(lv_obj_t* scr) {
  curtain = nullptr;
  texts = nullptr;
  summary = nullptr;
  readPill = nullptr;
  hint = nullptr;
  const model::DayInfo& day = S.day.day;
  if (!day.celebration.present()) return;

  const CelebrationText& text = textFor(day.celebration.slug);
  const Art art = S.actions.art && !day.celebration.slug.isEmpty() ? S.actions.art(day.celebration.slug) : Art();
  hasArt = art.pixels != nullptr && art.side > 0;
  if (hasArt) buildWithArt(scr, day, text, art);
  else buildInType(scr, day, text);
}

// The picture or the text of today's celebration has just been stored.
void celebrationContentArrived() {
  words = CelebrationText();
  wordsSlug = String();
  if (S.mode == Mode::Pages && S.page == Page::Celebration) rebuildVisible();
}

void openReading() {
  S.mode = Mode::Reading;
  transition([]() {
    if (S.mode != Mode::Reading) return;
    clear(S.sub);
    buildReading(S.sub);
    load(S.sub);
  });
}

}  // namespace ui::detail
