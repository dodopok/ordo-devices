// The menu at the end of the carousel and the three tools behind it: the
// bead counter, the time of silence and the torch.

#include "ui_internal.h"

namespace ui::detail {
namespace {

enum class Item : uintptr_t { Beads, Silence, Torch, Settings };

// What the shared tool screen currently holds, so a count or a second going
// by can change it in place instead of building it again.
enum class Built { Nothing, Beads, SilenceSetup, SilenceRunning, Torch };
Built built = Built::Nothing;

// ------------------------------------------------------------------ flame

// A candle flame, painted point by point: a drop that is narrow at the tip
// and round at the base, white-hot at the core, gold around it and amber at
// the edge. Each size is painted once into a buffer that lives for as long
// as the firmware runs; every screen that shows a flame points at the same
// pixels, and only the opacity moves after that.
lv_draw_buf_t* paintFlame(int32_t w, int32_t h) {
  lv_draw_buf_t* buf = lv_draw_buf_create(w, h, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
  if (!buf) return nullptr;

  auto mix = [](uint32_t from, uint32_t to, float f) -> lv_color32_t {
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    auto channel = [&](int shift) -> uint8_t {
      const float a = static_cast<float>((from >> shift) & 0xFF);
      const float b = static_cast<float>((to >> shift) & 0xFF);
      return static_cast<uint8_t>(a + (b - a) * f);
    };
    lv_color32_t c;
    c.red = channel(16);
    c.green = channel(8);
    c.blue = channel(0);
    c.alpha = 255;
    return c;
  };

  for (int32_t py = 0; py < h; py++) {
    lv_color32_t* row = reinterpret_cast<lv_color32_t*>(buf->data + static_cast<size_t>(py) * buf->header.stride);
    const float t = (static_cast<float>(py) + 0.5f) / static_cast<float>(h);
    const float half = (w / 2.0f - 1.5f) * sinf(static_cast<float>(M_PI) * powf(t, 1.7f));
    for (int32_t px = 0; px < w; px++) {
      const float dx = fabsf(static_cast<float>(px) + 0.5f - w / 2.0f);
      const float edge = half - dx;
      if (edge <= 0) {
        row[px].red = row[px].green = row[px].blue = row[px].alpha = 0;
        continue;
      }
      // Distance from the core, which sits low in the flame.
      const float nx = dx / (w / 2.0f);
      const float ny = (t - 0.76f) / 0.42f;
      const float r = sqrtf(nx * nx * 2.2f + ny * ny);
      lv_color32_t color = r < 0.42f ? mix(0xFFF8E1, 0xFBD76B, r / 0.42f) : mix(0xFBD76B, 0xD9772E, (r - 0.42f) / 0.75f);
      color.alpha = static_cast<uint8_t>((edge >= 1.6f ? 1.0f : edge / 1.6f) * 255.0f);
      row[px] = color;
    }
  }
  return buf;
}

// `small`: the one on the menu tile. Otherwise the one of the silence screen.
lv_obj_t* flame(lv_obj_t* parent, bool small) {
  static lv_draw_buf_t* large = nullptr;
  static lv_draw_buf_t* little = nullptr;
  lv_draw_buf_t*& buf = small ? little : large;
  if (!buf) buf = small ? paintFlame(30, 46) : paintFlame(68, 104);
  if (!buf) return nullptr;
  lv_obj_t* image = lv_image_create(parent);
  lv_image_set_src(image, buf);
  inert(image);
  breathe(image, LV_OPA_80, [](void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
  });
  return image;
}

void candle(lv_obj_t* scr, int32_t y) {
  lv_obj_t* image = flame(scr, false);
  if (!image) return;
  lv_obj_align(image, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_t* wick = dot(scr, 6, lv_color_hex(0x4A4640), theme::bg(), 0);
  lv_obj_set_size(wick, 5, 16);
  lv_obj_set_style_radius(wick, 2, 0);
  lv_obj_align(wick, LV_ALIGN_TOP_MID, 0, y + 100);
}

// ------------------------------------------------------------------- menu

int beadsTotal() {
  static const int totals[] = {33, 33, 50, 100};
  return totals[S.prefs.beadsMode % 4];
}

const char* beadsModeName(int mode) {
  switch (mode % 4) {
    case 0: return tr("Terço anglicano", "Anglican rosary", "Rosario anglicano");
    case 1: return tr("Corda de 33", "Rope of 33", "Cuerda de 33");
    case 2: return tr("Corda de 50", "Rope of 50", "Cuerda de 50");
    default: return tr("Corda de 100", "Rope of 100", "Cuerda de 100");
  }
}

// The drawings on the tiles, each inside a 60 x 56 box whose top-left corner
// is (x, y) on the tile.
void drawBeads(lv_obj_t* tile, int32_t x, int32_t y) {
  // A small rosary: a ring of beads, three of them prayed, and the cross.
  const int count = 11;
  for (int i = 0; i < count; i++) {
    const float angle = (static_cast<float>(i) + 1.0f) / (count + 1.0f) * 2.0f * static_cast<float>(M_PI) - static_cast<float>(M_PI) / 2.0f;
    const bool lit = i < 4;
    lv_obj_t* d = dot(tile, 6, lit ? theme::gold() : theme::faint(), theme::bg(), 0);
    lv_obj_align(d, LV_ALIGN_TOP_LEFT, x + 30 + static_cast<int32_t>(lroundf(21.0f * cosf(angle))) - 3,
                 y + 30 + static_cast<int32_t>(lroundf(21.0f * sinf(angle))) - 3);
  }
  lv_obj_t* v = dot(tile, 3, theme::gold(), theme::bg(), 0);
  lv_obj_set_size(v, 3, 12);
  lv_obj_set_style_radius(v, 1, 0);
  lv_obj_align(v, LV_ALIGN_TOP_LEFT, x + 29, y + 1);
  lv_obj_t* h = dot(tile, 3, theme::gold(), theme::bg(), 0);
  lv_obj_set_size(h, 9, 3);
  lv_obj_set_style_radius(h, 1, 0);
  lv_obj_align(h, LV_ALIGN_TOP_LEFT, x + 26, y + 4);
}

void drawCandle(lv_obj_t* tile, int32_t x, int32_t y) {
  lv_obj_t* image = flame(tile, true);
  if (image) lv_obj_align(image, LV_ALIGN_TOP_LEFT, x + 15, y);
  lv_obj_t* wick = dot(tile, 4, lv_color_hex(0x4A4640), theme::bg(), 0);
  lv_obj_set_size(wick, 3, 9);
  lv_obj_set_style_radius(wick, 1, 0);
  lv_obj_align(wick, LV_ALIGN_TOP_LEFT, x + 28, y + 44);
}

void drawTorch(lv_obj_t* tile, int32_t x, int32_t y) {
  // A small sun: a disc and eight rays.
  lv_obj_t* disc = dot(tile, 20, theme::fg(), theme::bg(), 0);
  lv_obj_align(disc, LV_ALIGN_TOP_LEFT, x + 20, y + 18);
  for (int i = 0; i < 8; i++) {
    const float angle = static_cast<float>(i) * static_cast<float>(M_PI) / 4.0f;
    lv_obj_t* ray = dot(tile, 5, theme::fg(), theme::bg(), 0);
    lv_obj_align(ray, LV_ALIGN_TOP_LEFT, x + 30 + static_cast<int32_t>(lroundf(20.0f * cosf(angle))) - 2,
                 y + 28 + static_cast<int32_t>(lroundf(20.0f * sinf(angle))) - 2);
  }
}

void drawSliders(lv_obj_t* tile, int32_t x, int32_t y) {
  // Three sliders, each with its knob somewhere else.
  static const int32_t knobs[3] = {12, 34, 22};
  for (int i = 0; i < 3; i++) {
    lv_obj_t* track = dot(tile, 3, theme::faint(), theme::bg(), 0);
    lv_obj_set_size(track, 44, 3);
    lv_obj_set_style_radius(track, 1, 0);
    lv_obj_align(track, LV_ALIGN_TOP_LEFT, x + 8, y + 12 + i * 15);
    lv_obj_t* knob = dot(tile, 11, theme::fg(), theme::surface(), 0);
    lv_obj_align(knob, LV_ALIGN_TOP_LEFT, x + 8 + knobs[i] - 5, y + 8 + i * 15);
  }
}

void open(Item item) {
  switch (item) {
    case Item::Beads: openBeads(); break;
    case Item::Silence: openSilence(); break;
    case Item::Torch: openTorch(); break;
    case Item::Settings: openSettings(); break;
  }
}

// One of the four tiles: a drawing, a name, and a line saying where things
// stand ("12 de 33", "ainda 14 min").
void tile(lv_obj_t* scr, int column, int row, Item item, const char* name, const char* state, lv_color_t stateColor) {
  constexpr int32_t size = 146;
  constexpr int32_t gap = 12;
  lv_obj_t* btn = lv_button_create(scr);
  lv_obj_remove_style_all(btn);
  lv_obj_set_size(btn, size, size);
  lv_obj_align(btn, LV_ALIGN_TOP_LEFT, kCenter + (column == 0 ? -(size + gap / 2) : gap / 2),
               kCenter + (row == 0 ? -(size + gap / 2) : gap / 2));
  lv_obj_set_style_radius(btn, 40, 0);
  lv_obj_set_style_bg_color(btn, theme::surface(), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(btn, theme::border(), 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  // A touch lights the tile up before it opens.
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x262B33), LV_STATE_PRESSED);
  lv_obj_set_style_border_color(btn, theme::gold(), LV_STATE_PRESSED);
  lv_obj_set_scrollable(btn, false);

  // The drawing sits towards the middle of the glass, where the tile is
  // widest inside the circle; so do the words.
  const int32_t boxX = (size - 60) / 2;
  switch (item) {
    case Item::Beads: drawBeads(btn, boxX, 14); break;
    case Item::Silence: drawCandle(btn, boxX, 16); break;
    case Item::Torch: drawTorch(btn, boxX, 14); break;
    case Item::Settings: drawSliders(btn, boxX, 14); break;
  }
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, name);
  lv_obj_set_style_text_font(label, theme::label(), 0);
  lv_obj_set_style_text_color(label, theme::fg(), 0);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 80);
  if (state && *state) {
    lv_obj_t* line = lv_label_create(btn);
    lv_label_set_text(line, state);
    lv_obj_set_style_text_font(line, theme::tiny(), 0);
    lv_obj_set_style_text_color(line, stateColor, 0);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 108);
  }
  lv_obj_add_event_cb(
      btn, [](lv_event_t* e) { open(static_cast<Item>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)))); },
      LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(item)));
  // The four arrive one after the other, reading order.
  riseIn(btn, static_cast<uint32_t>(row * 2 + column) * 60, 14);
}

// ------------------------------------------------------------------- beads

std::vector<lv_obj_t*> beadDots;
lv_obj_t* beadFigure = nullptr;
lv_obj_t* beadCaption = nullptr;
int beadsShown = -1;  // the count the dots on screen stand for; -1: none yet

// The Anglican rosary: an invitatory bead, then four groups of a cruciform
// bead and seven weeks beads. 33 in all.
bool isLargeBead(int index) { return S.prefs.beadsMode % 4 == 0 && (index == 0 || (index - 1) % 8 == 0); }

String beadLabel(int count) {
  const int total = beadsTotal();
  if (count >= total) return String(tr("Amém", "Amen", "Amén"));
  if (S.prefs.beadsMode % 4 != 0) {
    return String(tr("Senhor Jesus Cristo, Filho de Deus, tem misericórdia de mim", "Lord Jesus Christ, Son of God, have mercy on me",
                     "Señor Jesucristo, Hijo de Dios, ten piedad de mí"));
  }
  if (count == 0) return String(tr("Cruz", "Cross", "Cruz"));
  const int bead = count - 1;
  if (bead == 0) return String(tr("Invitatória", "Invitatory", "Invitatoria"));
  if ((bead - 1) % 8 == 0) return String(tr("Cruciforme", "Cruciform", "Cruciforme"));
  char buf[48];
  snprintf(buf, sizeof(buf), tr("Semana %d  ·  %d de 7", "Week %d  ·  %d of 7", "Semana %d  ·  %d de 7"), (bead - 1) / 8 + 1,
           (bead - 1) % 8);
  return String(buf);
}

lv_color_t beadLit(int index, bool complete) {
  return complete || isLargeBead(index) ? theme::gold() : theme::fg();
}

void styleBead(int index, int count, bool complete) {
  lv_obj_t* d = beadDots[static_cast<size_t>(index)];
  const bool prayed = index < count;
  const lv_color_t lit = beadLit(index, complete);
  lv_obj_set_style_bg_color(d, prayed ? lit : theme::bg(), 0);
  lv_obj_set_style_border_width(d, prayed ? 0 : 2, 0);
  lv_obj_set_style_border_color(d, index == count ? theme::fg() : theme::faint(), 0);
}

// A ring that opens out from the bead just prayed and fades: the answer to
// a press, for a thumb that cannot feel the glass.
void ripple(int index, bool complete) {
  if (!S.prefs.animations) return;
  const int total = beadsTotal();
  int32_t cx = 0, cy = 0;
  polarPoint(206.0f, static_cast<float>(index + 1) / static_cast<float>(total + 1), cx, cy);
  lv_obj_t* ring = dot(S.tool, 12, theme::bg(), beadLit(index, complete), 2);
  lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
  // Its centre rides along in the user data, packed into one pointer.
  lv_obj_set_user_data(ring, reinterpret_cast<void*>(static_cast<uintptr_t>((static_cast<uint32_t>(cx) << 16) | static_cast<uint32_t>(cy))));
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, ring);
  lv_anim_set_values(&a, 0, 100);
  lv_anim_set_duration(&a, 420);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
    lv_obj_t* r = static_cast<lv_obj_t*>(obj);
    const uintptr_t packed = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(r));
    const int32_t x = static_cast<int32_t>((packed >> 16) & 0xFFFF);
    const int32_t y = static_cast<int32_t>(packed & 0xFFFF);
    const int32_t size = 12 + (v * 30) / 100;
    lv_obj_set_size(r, size, size);
    lv_obj_align(r, LV_ALIGN_TOP_LEFT, x - size / 2, y - size / 2);
    lv_obj_set_style_border_opa(r, static_cast<lv_opa_t>(255 - (v * 255) / 100), 0);
  });
  lv_anim_set_completed_cb(&a, [](lv_anim_t* anim) { lv_obj_delete_async(static_cast<lv_obj_t*>(anim->var)); });
  lv_anim_start(&a);
}

// Makes the dots and the words on screen say `S.beads`, touching only what
// changed since the last time.
void beadsRefresh(bool pressed) {
  if (built != Built::Beads) return;
  const int total = beadsTotal();
  const int count = S.beads > total ? total : S.beads;
  const bool complete = count >= total;
  const bool wasComplete = beadsShown >= total;
  if (beadsShown < 0 || count < beadsShown || complete != wasComplete) {
    for (int i = 0; i < total; i++) styleBead(i, count, complete);
  } else {
    for (int i = beadsShown > 0 ? beadsShown - 1 : 0; i <= count && i < total; i++) styleBead(i, count, complete);
  }
  beadsShown = count;

  char buf[12];
  snprintf(buf, sizeof(buf), "%d", count);
  lv_label_set_text(beadFigure, buf);
  lv_obj_set_style_text_color(beadFigure, complete ? theme::gold() : theme::fg(), 0);
  lv_label_set_text(beadCaption, beadLabel(count).c_str());
  const bool prayer = S.prefs.beadsMode % 4 != 0 && !complete;
  lv_obj_set_style_text_font(beadCaption, prayer ? theme::italic() : theme::label(), 0);
  lv_obj_set_style_text_color(beadCaption, complete ? theme::gold() : (prayer ? theme::dim() : theme::fg()), 0);
  if (pressed && count > 0) ripple(count - 1, complete);
}

void buildBeads() {
  lv_obj_t* scr = S.tool;
  clear(scr);
  built = Built::Beads;
  beadDots.clear();
  beadsShown = -1;
  const int total = beadsTotal();

  // The cross at the top of the Anglican rosary.
  if (S.prefs.beadsMode % 4 == 0) {
    lv_obj_t* v = dot(scr, 4, theme::gold(), theme::bg(), 0);
    lv_obj_set_size(v, 4, 18);
    lv_obj_set_style_radius(v, 1, 0);
    lv_obj_align(v, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_t* h = dot(scr, 4, theme::gold(), theme::bg(), 0);
    lv_obj_set_size(h, 12, 4);
    lv_obj_set_style_radius(h, 1, 0);
    lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 17);
  }
  const int32_t small = total > 60 ? 6 : (total > 40 ? 9 : 11);
  for (int i = 0; i < total; i++) {
    lv_obj_t* d = dot(scr, isLargeBead(i) ? small + 6 : small, theme::bg(), theme::faint(), 2);
    placePolar(d, 206.0f, static_cast<float>(i + 1) / static_cast<float>(total + 1));
    // The string is laid out bead by bead, round the glass.
    fadeIn(d, static_cast<uint32_t>(i) * (total > 40 ? 4 : 9), 160);
    beadDots.push_back(d);
  }

  beadFigure = text(scr, "0", theme::digits(), theme::fg(), 84, 0, 300);
  char buf[16];
  snprintf(buf, sizeof(buf), tr("DE %d", "OF %d", "DE %d"), total);
  text(scr, buf, theme::tiny(), theme::dim(), 212, 3, 200);
  beadCaption = text(scr, "", theme::label(), theme::fg(), 238, 0, 300);

  lv_obj_t* mode = pill(
      scr, beadsModeName(S.prefs.beadsMode), false,
      [](lv_event_t*) {
        S.prefs.beadsMode = (S.prefs.beadsMode + 1) % 4;
        S.beads = 0;
        S.dirty[static_cast<int>(Page::Menu)] = true;
        if (S.actions.prefsChanged) S.actions.prefsChanged(S.prefs);
        openBeads();
      },
      nullptr);
  lv_obj_align(mode, LV_ALIGN_TOP_MID, 0, 300);
  text(scr, tr("BOOT ou toque conta  ·  segurar zera", "BOOT or tap counts  ·  hold resets",
               "BOOT o toque cuenta  ·  mantener reinicia"),
       theme::tiny(), theme::faint(), 362, 0, 280);

  // A tap anywhere on the glass counts one bead.
  lv_obj_add_event_cb(scr, [](lv_event_t*) { beadsCountOne(); }, LV_EVENT_CLICKED, nullptr);
  beadsRefresh(false);
}

// ----------------------------------------------------------------- silence

lv_obj_t* silenceFigure = nullptr;

String silenceText() {
  char buf[16];
  snprintf(buf, sizeof(buf), "%lu:%02lu", static_cast<unsigned long>(S.silenceRemaining / 60),
           static_cast<unsigned long>(S.silenceRemaining % 60));
  return String(buf);
}

void buildSilence() {
  lv_obj_t* scr = S.tool;
  clear(scr);
  silenceFigure = nullptr;

  if (S.silenceRunning) {
    built = Built::SilenceRunning;
    candle(scr, 62);
    silenceFigure = text(scr, silenceText().c_str(), theme::display(), theme::fg(), 200, 0, 240);
    text(scr, tr("SILÊNCIO", "SILENCE", "SILENCIO"), theme::tiny(), theme::dim(), 262, 3, 240);
    text(scr, tr("o sino toca ao terminar", "the bell rings at the end", "la campana suena al terminar"), theme::italic(),
         theme::dim(), 292, 0, 280);
    lv_obj_t* stop = pill(
        scr, tr("Encerrar", "End", "Terminar"), false,
        [](lv_event_t*) {
          if (S.actions.silence) S.actions.silence(0);
        },
        nullptr);
    lv_obj_align(stop, LV_ALIGN_TOP_MID, 0, 350);
    return;
  }

  built = Built::SilenceSetup;
  backChip(scr);
  text(scr, tr("SILÊNCIO", "SILENCE", "SILENCIO"), theme::tiny(), theme::dim(), 92, 3, 240);
  text(scr, tr("Um sino no começo e outro no fim", "A bell at the start and another at the end",
               "Una campana al principio y otra al final"),
       theme::italic(), theme::fg(), 126, 0, 300);

  lv_obj_t* row = lv_obj_create(scr);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, 380, 60);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 186);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 12, 0);
  for (uintptr_t minutes : {5u, 10u, 20u}) {
    char label[16];
    snprintf(label, sizeof(label), "%u min", static_cast<unsigned>(minutes));
    pill(
        row, label, false,
        [](lv_event_t* e) {
          const int minutes = static_cast<int>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
          if (S.actions.silence) S.actions.silence(minutes);
        },
        reinterpret_cast<void*>(minutes));
  }
  candle(scr, 272);
}

}  // namespace

void buildMenu(lv_obj_t* scr) {
  char beads[24] = "";
  if (S.beads > 0) snprintf(beads, sizeof(beads), tr("%d de %d", "%d of %d", "%d de %d"), S.beads > beadsTotal() ? beadsTotal() : S.beads, beadsTotal());
  tile(scr, 0, 0, Item::Beads, tr("Contas", "Beads", "Cuentas"), beads[0] ? beads : tr("terço e corda", "rosary and rope", "rosario y cuerda"),
       beads[0] ? theme::gold() : theme::faint());
  char silence[24] = "";
  if (S.silenceRunning) {
    snprintf(silence, sizeof(silence), tr("ainda %lu min", "%lu min left", "quedan %lu min"),
             static_cast<unsigned long>((S.silenceRemaining + 59) / 60));
  }
  tile(scr, 1, 0, Item::Silence, tr("Silêncio", "Silence", "Silencio"), S.silenceRunning ? silence : "5 · 10 · 20 min",
       S.silenceRunning ? theme::gold() : theme::faint());
  tile(scr, 0, 1, Item::Torch, tr("Lanterna", "Torch", "Linterna"), tr("até 1 minuto", "up to 1 minute", "hasta 1 minuto"), theme::faint());
  tile(scr, 1, 1, Item::Settings, tr("Ajustes", "Settings", "Ajustes"), tr("tela, som, rede", "screen, sound, network", "pantalla, sonido, red"),
       theme::faint());
}

// ------------------------------------------------------------------- beads

void openBeads() {
  S.mode = Mode::Beads;
  transition([]() {
    if (S.mode != Mode::Beads) return;
    buildBeads();
    load(S.tool);
  });
}

void beadsCountOne() {
  const int total = beadsTotal();
  if (S.beads >= total) S.beads = 1;
  else S.beads++;
  S.dirty[static_cast<int>(Page::Menu)] = true;
  if (S.beads == total && S.actions.bell) S.actions.bell(1);
  if (S.mode == Mode::Beads) beadsRefresh(true);
}

void beadsReset() {
  S.beads = 0;
  S.dirty[static_cast<int>(Page::Menu)] = true;
  if (S.mode == Mode::Beads) beadsRefresh(false);
}

// ----------------------------------------------------------------- silence

void openSilence() {
  S.mode = Mode::Silence;
  S.dirty[static_cast<int>(Page::Menu)] = true;
  transition([]() {
    if (S.mode != Mode::Silence) return;
    buildSilence();
    load(S.tool);
  });
}

void refreshSilence() {
  S.dirty[static_cast<int>(Page::Menu)] = true;
  if (S.mode == Mode::Silence && built == Built::SilenceRunning && silenceFigure) {
    lv_label_set_text(silenceFigure, silenceText().c_str());
  }
}

// ------------------------------------------------------------------- torch

void openTorch() {
  S.mode = Mode::Torch;
  transition([]() {
    if (S.mode != Mode::Torch) return;
    lv_obj_t* scr = S.tool;
    clear(scr);
    built = Built::Torch;
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    text(scr, tr("toque para apagar", "tap to switch off", "toque para apagar"), theme::small(), lv_color_hex(0x8A8A8A), 392, 0,
         240);
    lv_obj_add_event_cb(scr, [](lv_event_t*) { goBack(); }, LV_EVENT_CLICKED, nullptr);
    load(scr);
  });
}

}  // namespace ui::detail
