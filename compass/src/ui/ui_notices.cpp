// Full-screen notices: the splash, plain messages, Wi-Fi setup, the link
// code, and the ones that sit on top of the day for a moment (a reminder,
// the bell of the hours, the end of a time of silence).

#include "ui_internal.h"

namespace ui::detail {
namespace {

lv_obj_t* noticeCountdown = nullptr;

// A notice interrupts: it does not wait for a dip, and it ends one that is
// under way. The controller often blocks right after showing one (the bell,
// a request to the API), so it has to be whole after a single pass of LVGL.
lv_obj_t* begin(bool overDay, uint32_t seconds = 0) {
  haltTransition();
  S.shownOnce = true;
  S.mode = Mode::Notice;
  S.noticeOverDay = overDay;
  S.noticeSeconds = seconds;
  noticeCountdown = nullptr;
  clear(S.notice);
  return S.notice;
}

// Puts the finished notice on the glass. If it cut a screen change short,
// the glass may be dark: it is painted first and lit after.
void present(lv_obj_t* scr) {
  load(scr);
  revealNow();
}

lv_obj_t* spinner(lv_obj_t* scr, int32_t y) {
  lv_obj_t* s = lv_spinner_create(scr);
  lv_spinner_set_anim_params(s, 1200, 200);
  lv_obj_set_size(s, 40, 40);
  lv_obj_set_style_arc_color(s, theme::track(), LV_PART_MAIN);
  lv_obj_set_style_arc_color(s, theme::gold(), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(s, 4, LV_PART_MAIN);
  lv_obj_set_style_arc_width(s, 4, LV_PART_INDICATOR);
  lv_obj_align(s, LV_ALIGN_TOP_MID, 0, y);
  return s;
}

// A QR code on a paper-white card with its own quiet zone.
lv_obj_t* qr(lv_obj_t* scr, const String& data, int32_t size, int32_t y) {
  lv_obj_t* card = lv_obj_create(scr);
  lv_obj_remove_style_all(card);
  lv_obj_set_size(card, size + 20, size + 20);
  lv_obj_set_style_bg_color(card, theme::paper(), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 12, 0);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y);
  inert(card);
  lv_obj_t* code = lv_qrcode_create(card);
  lv_qrcode_set_size(code, size);
  lv_qrcode_set_dark_color(code, lv_color_hex(0x000000));
  lv_qrcode_set_light_color(code, theme::paper());
  lv_qrcode_update(code, data.c_str(), data.length());
  lv_obj_center(code);
  return card;
}

const char* officeFull(int i) {
  static const char* pt[] = {"Oração da Manhã", "Oração do Meio-dia", "Oração da Tarde", "Completas"};
  static const char* en[] = {"Morning Prayer", "Midday Prayer", "Evening Prayer", "Compline"};
  static const char* es[] = {"Oración de la Mañana", "Oración del Mediodía", "Oración de la Tarde", "Completas"};
  if (i < 0 || i > 3) return "";
  return tr(pt[i], en[i], es[i]);
}

lv_obj_t* bellIcon(lv_obj_t* scr, int32_t y, lv_color_t color) {
  lv_obj_t* bell = lv_label_create(scr);
  lv_label_set_text(bell, LV_SYMBOL_BELL);
  lv_obj_set_style_text_font(bell, theme::iconsLarge(), 0);
  lv_obj_set_style_text_color(bell, color, 0);
  lv_obj_align(bell, LV_ALIGN_TOP_MID, 0, y);
  return bell;
}

void tapDismisses(lv_obj_t* scr) {
  lv_obj_add_event_cb(scr, [](lv_event_t*) { dismissNotice(); }, LV_EVENT_CLICKED, nullptr);
}

}  // namespace
}  // namespace ui::detail

namespace ui {

using namespace detail;

void showSplash() {
  lv_obj_t* scr = begin(false);
  ringArc(scr, 454, 3, 0.0f, 1.0f, theme::track());
  text(scr, "Ordo", theme::display(), theme::fg(), 186, 0, 300);
  text(scr, "bússola litúrgica", theme::italic(), theme::dim(), 244, 0, 300);
  present(scr);
}

void showMessage(const String& title, const String& body, bool withSpinner) {
  lv_obj_t* scr = begin(false);
  text(scr, title.c_str(), theme::large(), theme::fg(), withSpinner ? 140 : 162, 0, 320);
  text(scr, body.c_str(), theme::body(), theme::dim(), withSpinner ? 196 : 218, 0, 300);
  if (withSpinner) spinner(scr, 300);
  present(scr);
}

void showWifiPortal(const String& ssid, PortalStep step, const String& network) {
  lv_obj_t* scr = begin(false);
  text(scr, "WI-FI", theme::tiny(), theme::dim(), 56, 4, 200);

  switch (step) {
    case PortalStep::WaitingPhone: {
      // Phones join the compass' own network straight from this code; the
      // page to pick the home network then opens by itself.
      qr(scr, "WIFI:T:nopass;S:" + ssid + ";;", 150, 88);
      text(scr, ssid.c_str(), theme::reading(), theme::gold(), 268, 0, 300);
      text(scr, tr("Aponte a câmera do celular, ou entre nesta rede. A página para escolher o seu Wi-Fi abre sozinha.",
                   "Point your phone's camera here, or join this network. The page to pick your Wi-Fi opens by itself.",
                   "Apunte la cámara del móvil, o entre en esta red. La página para elegir su Wi-Fi se abre sola."),
           theme::small(), theme::dim(), 304, 0, 300);
      break;
    }
    case PortalStep::PhoneConnected:
      text(scr, tr("Celular conectado", "Phone connected", "Móvil conectado"), theme::large(), theme::fg(), 150, 0, 320);
      text(scr, tr("Escolha a sua rede na página que abriu no celular.", "Pick your network on the page that opened on the phone.",
                   "Elija su red en la página que se abrió en el móvil."),
           theme::body(), theme::dim(), 206, 0, 290);
      text(scr, ssid.c_str(), theme::small(), theme::gold(), 300, 0, 280);
      break;
    case PortalStep::Connecting:
      text(scr, tr("Conectando", "Connecting", "Conectando"), theme::large(), theme::fg(), 150, 0, 320);
      text(scr, network.c_str(), theme::body(), theme::gold(), 206, 0, 290);
      spinner(scr, 276);
      break;
    case PortalStep::Connected: {
      lv_obj_t* ok = lv_label_create(scr);
      lv_label_set_text(ok, LV_SYMBOL_OK);
      lv_obj_set_style_text_font(ok, theme::iconsLarge(), 0);
      lv_obj_set_style_text_color(ok, theme::green(), 0);
      lv_obj_align(ok, LV_ALIGN_TOP_MID, 0, 122);
      text(scr, tr("Conectado", "Connected", "Conectado"), theme::large(), theme::fg(), 170, 0, 320);
      text(scr, network.c_str(), theme::body(), theme::gold(), 226, 0, 290);
      break;
    }
    case PortalStep::Failed:
      text(scr, tr("Não conectou", "Could not connect", "No se conectó"), theme::large(), theme::fg(), 150, 0, 320);
      text(scr, tr("Confira a senha e tente de novo na página do celular.", "Check the password and try again on the phone's page.",
                   "Revise la contraseña e inténtelo de nuevo en la página del móvil."),
           theme::body(), theme::dim(), 206, 0, 290);
      text(scr, network.c_str(), theme::small(), theme::gold(), 300, 0, 280);
      break;
  }
  present(scr);
}

void showLink(const String& userCode, const String& verificationUri) {
  lv_obj_t* scr = begin(false);
  text(scr, tr("VINCULE AO ORDO", "LINK TO ORDO", "VINCULE A ORDO"), theme::tiny(), theme::dim(), 54, 3, 260);
  qr(scr, verificationUri, 150, 82);
  text(scr, userCode.c_str(), theme::code(), theme::fg(), 266, 3, 340);
  text(scr, tr("No app: Configurações › Aparelhos", "In the app: Settings › Devices", "En la app: Ajustes › Dispositivos"), theme::small(),
       theme::dim(), 332, 0, 300);
  noticeCountdown = text(scr, "", theme::tiny(), theme::faint(), 362, 1, 240);
  present(scr);
}

void setLinkCountdown(uint32_t secondsLeft) {
  if (!noticeCountdown || lv_screen_active() != S.notice) return;
  char buf[40];
  snprintf(buf, sizeof(buf), tr("expira em %lu:%02lu", "expires in %lu:%02lu", "caduca en %lu:%02lu"),
           static_cast<unsigned long>(secondsLeft / 60), static_cast<unsigned long>(secondsLeft % 60));
  lv_label_set_text(noticeCountdown, buf);
}

void showReminder(int office) {
  lv_obj_t* scr = begin(true, 25);
  const model::Today& t = S.day;
  const lv_color_t accent = t.day.hasColor ? t.day.color : theme::gold();
  ringArc(scr, 452, 5, 0.0f, 1.0f, accent);
  // The bell breathes, not the ring: a ring as wide as the glass would have
  // the whole screen repainted at every step.
  breathe(bellIcon(scr, 106, theme::gold()), LV_OPA_30, [](void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
  });
  text(scr, officeFull(office), theme::large(), theme::fg(), 162, 0, 320);
  const bool done = t.officesKnown && office >= 0 && office < 4 && t.offices[office].done;
  text(scr, done ? tr("já orada hoje", "already prayed today", "ya orada hoy")
                 : (t.officesKnown ? tr("ainda não orada hoje", "not prayed yet today", "aún no orada hoy")
                                   : tr("é hora de orar", "it is time to pray", "es hora de orar")),
       theme::body(), theme::dim(), 244, 0, 300);
  if (t.officesKnown && t.streakCurrent > 1) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%d %s", t.streakCurrent, tr("DIAS SEGUIDOS", "DAY STREAK", "DÍAS SEGUIDOS"));
    text(scr, buf, theme::tiny(), theme::gold(), 286, 2, 260);
  }
  text(scr, tr("toque para fechar", "tap to close", "toque para cerrar"), theme::tiny(), theme::faint(), 372, 0, 240);
  tapDismisses(scr);
  present(scr);
}

void showHourBell(int hour) {
  lv_obj_t* scr = begin(true, 10);
  const lv_color_t accent = S.day.day.hasColor ? S.day.day.color : theme::gold();
  ringArc(scr, 452, 5, 0.0f, 1.0f, accent);
  bellIcon(scr, 70, theme::gold());
  text(scr, clockText(hour * 60).c_str(), theme::digits(), theme::fg(), 124, 0, 340);
  text(scr, hour == 12 ? tr("MEIO-DIA", "NOON", "MEDIODÍA") : tr("SINO DAS HORAS", "BELL OF THE HOURS", "CAMPANA DE LAS HORAS"),
       theme::tiny(), theme::dim(), 268, 3, 300);
  if (!S.day.day.title.isEmpty()) text(scr, S.day.day.title.c_str(), theme::title(), accent, 298, 0, 300);
  tapDismisses(scr);
  present(scr);
}

void showSilenceDone() {
  lv_obj_t* scr = begin(true, 15);
  ringArc(scr, 452, 5, 0.0f, 1.0f, theme::gold());
  text(scr, tr("Amém", "Amen", "Amén"), theme::display(), theme::fg(), 176, 0, 300);
  text(scr, tr("o tempo de silêncio terminou", "the time of silence is over", "el tiempo de silencio terminó"), theme::italic(),
       theme::dim(), 240, 0, 300);
  tapDismisses(scr);
  present(scr);
}

void showLowBattery() {
  lv_obj_t* scr = begin(false);
  lv_obj_t* icon = lv_label_create(scr);
  lv_label_set_text(icon, LV_SYMBOL_BATTERY_EMPTY);
  lv_obj_set_style_text_font(icon, theme::iconsLarge(), 0);
  lv_obj_set_style_text_color(icon, theme::red(), 0);
  lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 130);
  text(scr, tr("Bateria no fim", "Battery flat", "Batería agotada"), theme::large(), theme::fg(), 178, 0, 320);
  text(scr, tr("Desligando para proteger a bateria. Ligue no cabo.", "Switching off to protect the battery. Plug it in.",
               "Apagando para proteger la batería. Conéctelo."),
       theme::body(), theme::dim(), 234, 0, 290);
  present(scr);
}

void showUpdating(const String& version, int percent) {
  static lv_obj_t* arc = nullptr;
  static lv_obj_t* figure = nullptr;
  // Built once; after that only the arc and the figure move.
  if (S.mode != Mode::Notice || lv_screen_active() != S.notice || !arc || noticeCountdown != figure) {
    lv_obj_t* scr = begin(false);
    ringArc(scr, 440, 6, 0.0f, 1.0f, theme::track());
    arc = ringArc(scr, 440, 6, 0.0f, 0.001f, theme::gold());
    text(scr, tr("ATUALIZANDO", "UPDATING", "ACTUALIZANDO"), theme::tiny(), theme::dim(), 132, 3, 260);
    figure = text(scr, "0%", theme::display(), theme::fg(), 172, 0, 260);
    char buf[48];
    snprintf(buf, sizeof(buf), "v%s", version.c_str());
    text(scr, buf, theme::label(), theme::gold(), 236, 0, 260);
    text(scr, tr("não desligue; ela reinicia sozinha", "do not switch it off; it restarts by itself",
                 "no la apague; se reinicia sola"),
         theme::italic(), theme::dim(), 286, 0, 260);
    noticeCountdown = figure;
    present(scr);
  }
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  lv_arc_set_bg_end_angle(arc, static_cast<float>(percent) * 3.6f);
  lv_label_set_text_fmt(figure, "%d%%", percent);
}

void dismissNotice() {
  if (S.dayShown) showPage(S.page);
}

}  // namespace ui
