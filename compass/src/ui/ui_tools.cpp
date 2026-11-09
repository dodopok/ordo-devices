// The menu at the end of the carousel and the three tools behind it: the
// bead counter, the time of silence and the torch.

#include "ui_internal.h"

namespace ui::detail {
namespace {

enum class Item : uintptr_t { Beads, Silence, Torch, Settings };

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

// The Anglican rosary: an invitatory bead, then four groups of a cruciform
// bead and seven weeks beads. 33 in all.
bool isLargeBead(int index) { return S.prefs.beadsMode % 4 == 0 && (index == 0 || (index - 1) % 8 == 0); }

String beadLabel(int count) {
  const int total = beadsTotal();
  if (count >= total) return String(tr("Amém", "Amen", "Amén"));
  if (S.prefs.beadsMode % 4 != 0) return String();
  if (count == 0) return String(tr("Cruz", "Cross", "Cruz"));
  const int bead = count - 1;
  if (bead == 0) return String(tr("Invitatória", "Invitatory", "Invitatoria"));
  if ((bead - 1) % 8 == 0) return String(tr("Cruciforme", "Cruciform", "Cruciforme"));
  char buf[48];
  snprintf(buf, sizeof(buf), tr("Semana %d  ·  %d de 7", "Week %d  ·  %d of 7", "Semana %d  ·  %d de 7"), (bead - 1) / 8 + 1,
           (bead - 1) % 8);
  return String(buf);
}

void menuRow(lv_obj_t* scr, const char* label, int32_t y, Item item) {
  lv_obj_t* btn = lv_button_create(scr);
  lv_obj_set_size(btn, 300, 58);
  lv_obj_set_style_pad_hor(btn, 22, 0);
  lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_style_radius(btn, 18, 0);
  lv_obj_set_style_bg_color(btn, theme::surface(), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(btn, theme::border(), 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_t* l = lv_label_create(btn);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, theme::label(), 0);
  lv_obj_set_style_text_color(l, theme::fg(), 0);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t* chevron = lv_label_create(btn);
  lv_label_set_text(chevron, "›");
  lv_obj_set_style_text_font(chevron, theme::reading(), 0);
  lv_obj_set_style_text_color(chevron, theme::faint(), 0);
  lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, 0, -2);
  lv_obj_add_event_cb(
      btn,
      [](lv_event_t* e) {
        switch (static_cast<Item>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)))) {
          case Item::Beads: openBeads(); break;
          case Item::Silence: openSilence(); break;
          case Item::Torch: openTorch(); break;
          case Item::Settings: openSettings(); break;
        }
      },
      LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(item)));
}

// A candle flame, painted point by point: a drop that is narrow at the tip
// and round at the base, white-hot at the core, gold around it and amber at
// the edge. It is painted once into a buffer that lives for as long as the
// firmware runs; every screen that shows a flame points at the same pixels,
// and only the opacity moves after that.
constexpr int32_t kFlameW = 68;
constexpr int32_t kFlameH = 104;

lv_draw_buf_t* flamePixels() {
  static lv_draw_buf_t* buf = nullptr;
  if (buf) return buf;
  buf = lv_draw_buf_create(kFlameW, kFlameH, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
  if (!buf) return nullptr;
  constexpr int32_t w = kFlameW;
  constexpr int32_t h = kFlameH;

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

void flame(lv_obj_t* scr, int32_t y) {
  constexpr int32_t h = kFlameH;
  lv_draw_buf_t* buf = flamePixels();
  if (!buf) return;
  lv_obj_t* canvas = lv_image_create(scr);
  lv_image_set_src(canvas, buf);
  inert(canvas);
  lv_obj_align(canvas, LV_ALIGN_TOP_MID, 0, y);

  lv_obj_t* wick = dot(scr, 6, lv_color_hex(0x4A4640), theme::bg(), 0);
  lv_obj_set_size(wick, 5, 16);
  lv_obj_set_style_radius(wick, 2, 0);
  lv_obj_align(wick, LV_ALIGN_TOP_MID, 0, y + h - 4);
  breathe(canvas, LV_OPA_80, [](void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
  });
}

}  // namespace

void buildMenu(lv_obj_t* scr) {
  text(scr, tr("MAIS", "MORE", "MÁS"), theme::tiny(), theme::dim(), 80, 3, 200);
  menuRow(scr, tr("Contas", "Beads", "Cuentas"), 112, Item::Beads);
  menuRow(scr, tr("Silêncio", "Silence", "Silencio"), 178, Item::Silence);
  menuRow(scr, tr("Lanterna", "Torch", "Linterna"), 244, Item::Torch);
  menuRow(scr, tr("Ajustes", "Settings", "Ajustes"), 310, Item::Settings);
}

// ------------------------------------------------------------------- beads

void openBeads() {
  S.mode = Mode::Beads;
  lv_obj_t* scr = S.tool;
  clear(scr);
  const int total = beadsTotal();
  const int count = S.beads > total ? total : S.beads;
  const bool complete = count >= total;

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
    const bool large = isLargeBead(i);
    const bool prayed = i < count;
    const int32_t size = large ? small + 6 : small;
    const lv_color_t lit = complete ? theme::gold() : (large ? theme::gold() : theme::fg());
    lv_obj_t* d = dot(scr, size, prayed ? lit : theme::bg(), prayed ? lit : (i == count ? theme::fg() : theme::faint()),
                      prayed ? 0 : 2);
    placePolar(d, 206.0f, static_cast<float>(i + 1) / static_cast<float>(total + 1));
  }

  char buf[12];
  snprintf(buf, sizeof(buf), "%d", count);
  text(scr, buf, theme::digits(), complete ? theme::gold() : theme::fg(), 84, 0, 300);
  snprintf(buf, sizeof(buf), tr("DE %d", "OF %d", "DE %d"), total);
  text(scr, buf, theme::tiny(), theme::dim(), 212, 3, 200);

  const String label = beadLabel(count);
  if (!label.isEmpty()) {
    text(scr, label.c_str(), theme::label(), complete ? theme::gold() : theme::fg(), 242, 0, 300);
  } else {
    text(scr, tr("Senhor Jesus Cristo, Filho de Deus, tem misericórdia de mim",
                 "Lord Jesus Christ, Son of God, have mercy on me",
                 "Señor Jesucristo, Hijo de Dios, ten piedad de mí"),
         theme::italic(), theme::dim(), 236, 0, 300);
  }

  lv_obj_t* mode = pill(
      scr, beadsModeName(S.prefs.beadsMode), false,
      [](lv_event_t*) {
        S.prefs.beadsMode = (S.prefs.beadsMode + 1) % 4;
        S.beads = 0;
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
  load(scr);
}

void beadsCountOne() {
  const int total = beadsTotal();
  if (S.beads >= total) S.beads = 1;
  else S.beads++;
  if (S.beads == total && S.actions.bell) S.actions.bell(1);
  if (S.mode == Mode::Beads) openBeads();
}

void beadsReset() {
  S.beads = 0;
  if (S.mode == Mode::Beads) openBeads();
}

// ----------------------------------------------------------------- silence

void openSilence() {
  S.mode = Mode::Silence;
  lv_obj_t* scr = S.tool;
  clear(scr);

  if (S.silenceRunning) {
    flame(scr, 62);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu:%02lu", static_cast<unsigned long>(S.silenceRemaining / 60),
             static_cast<unsigned long>(S.silenceRemaining % 60));
    text(scr, buf, theme::display(), theme::fg(), 200, 0, 240);
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
    load(scr);
    return;
  }

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
  flame(scr, 272);
  load(scr);
}

// ------------------------------------------------------------------- torch

void openTorch() {
  S.mode = Mode::Torch;
  lv_obj_t* scr = S.tool;
  clear(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
  text(scr, tr("toque para apagar", "tap to switch off", "toque para apagar"), theme::small(), lv_color_hex(0x8A8A8A), 392, 0,
       240);
  lv_obj_add_event_cb(scr, [](lv_event_t*) { goBack(); }, LV_EVENT_CLICKED, nullptr);
  load(scr);
}

}  // namespace ui::detail
