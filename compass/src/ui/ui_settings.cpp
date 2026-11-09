// Settings: one scrolling list of everything the owner can change, and the
// small screens behind some of its rows (reminder time, Wi-Fi, about,
// confirmations).

#include "ui_internal.h"

namespace ui::detail {
namespace {

enum class Row : uintptr_t {
  Back,
  ScreenOff,
  Brightness,
  NightDim,
  HomeFace,
  Animations,
  TouchWakes,
  DeskOn,
  Sound,
  Volume,
  HourBell,
  ReminderSource,
  Reminder0,
  Reminder1,
  Reminder2,
  Reminder3,
  Wifi,
  SyncEvery,
  SyncNow,
  Update,
  About,
  PowerOff,
  Unlink,
  Erase,
  AddNetwork,
  Network0,  // + index
};

enum class Confirm { PowerOff, Unlink, Erase, Forget };

int32_t savedScroll = 0;
lv_obj_t* list = nullptr;
int editOffice = 0;
int editMinutes = 0;
lv_obj_t* editFigure = nullptr;
Confirm pending = Confirm::PowerOff;
int pendingIndex = 0;

// The rows of the list whose value can change while it is on screen. A tap
// rewrites the one label instead of building the list again: nothing jumps,
// and the scroll stays where the finger left it.
struct Shown {
  uintptr_t id;
  lv_obj_t* name;
  lv_obj_t* value;
};
std::vector<Shown> shown;

void openTimeEditor(int office);
void openWifi();
void buildUpdate(lv_obj_t* scr);
bool followingApp();
void openAbout();
void openConfirm(Confirm what, int index = 0);
void onRow(lv_event_t* e);

const char* yesNo(bool on) { return on ? tr("Sim", "Yes", "Sí") : tr("Não", "No", "No"); }

const char* level(uint8_t value) {
  switch (value) {
    case 0: return tr("Baixo", "Low", "Bajo");
    case 1: return tr("Médio", "Medium", "Medio");
    default: return tr("Alto", "High", "Alto");
  }
}

String screenOffText(uint8_t seconds) {
  char buf[16];
  if (seconds >= 60) snprintf(buf, sizeof(buf), "%u min", seconds / 60);
  else snprintf(buf, sizeof(buf), "%u s", seconds);
  return String(buf);
}

String syncText(uint8_t hours) {
  if (hours == 0) return String(tr("Ao acordar", "On wake", "Al despertar"));
  char buf[16];
  snprintf(buf, sizeof(buf), "%u h", hours);
  return String(buf);
}

const char* faceName(uint8_t face) {
  switch (face) {
    case 1: return tr("Bússola", "Compass", "Brújula");
    case 2: return tr("Leituras", "Readings", "Lecturas");
    case 3: return tr("Ofícios", "Offices", "Oficios");
    case 4: return tr("Semana", "Week", "Semana");
    default: return tr("Agora", "Now", "Ahora");
  }
}

String syncedText() {
  if (S.status.syncing) return String("…");
  const uint32_t ago = S.status.syncedMinutesAgo;
  if (ago == UINT32_MAX) return String("›");
  char buf[24];
  if (ago < 1) snprintf(buf, sizeof(buf), "%s", tr("agora", "just now", "ahora"));
  else if (ago < 60) snprintf(buf, sizeof(buf), tr("há %lu min", "%lu min ago", "hace %lu min"), static_cast<unsigned long>(ago));
  else if (ago < 48 * 60) snprintf(buf, sizeof(buf), tr("há %lu h", "%lu h ago", "hace %lu h"), static_cast<unsigned long>(ago / 60));
  else snprintf(buf, sizeof(buf), tr("há %lu dias", "%lu days ago", "hace %lu días"), static_cast<unsigned long>(ago / 1440));
  return String(buf);
}

// What a row says on its right-hand side, and whether it says it in gold
// (something is on, or set) or quietly.
bool valueOf(Row id, String& value, lv_color_t& color) {
  const Prefs& p = S.prefs;
  color = theme::gold();
  switch (id) {
    case Row::ScreenOff: value = screenOffText(p.screenSeconds); return true;
    case Row::Brightness: value = level(p.brightness); return true;
    case Row::NightDim: value = yesNo(p.nightDim); break;
    case Row::HomeFace: value = faceName(p.homeFace); return true;
    case Row::Animations: value = yesNo(p.animations); break;
    case Row::TouchWakes: value = yesNo(p.touchWakes); break;
    case Row::DeskOn: value = yesNo(p.deskAlwaysOn); break;
    case Row::Sound: value = yesNo(p.sound); break;
    case Row::Volume: value = level(p.volume); return true;
    case Row::HourBell: value = yesNo(p.hourBell); break;
    case Row::ReminderSource:
      value = p.remindersFromApp ? tr("Do app", "From the app", "De la app") : tr("Daqui", "From here", "De aquí");
      // Asked to follow the app, but the app has none set: the ones here ring.
      if (p.remindersFromApp && !S.day.hasReminders) color = theme::faint();
      return true;
    case Row::Reminder0:
    case Row::Reminder1:
    case Row::Reminder2:
    case Row::Reminder3: {
      const int16_t at = reminderFor(static_cast<int>(id) - static_cast<int>(Row::Reminder0));
      value = at >= 0 ? clockText(at) : String(tr("Desligado", "Off", "Apagado"));
      // The app's times are shown, not edited, here.
      if (at < 0 || followingApp()) color = theme::faint();
      return true;
    }
    case Row::Update:
      switch (S.status.update) {
        case UpdateState::Available: value = String("v") + S.status.updateVersion; break;
        case UpdateState::Checking: value = "…"; color = theme::faint(); break;
        default: value = "›"; color = theme::faint(); break;
      }
      return true;
    case Row::SyncEvery: value = syncText(p.syncHours); return true;
    case Row::SyncNow:
      value = syncedText();
      color = S.status.syncing ? theme::gold() : theme::faint();
      return true;
    default: return false;
  }
  // The yes-or-no rows: "no" is said quietly.
  if (value == yesNo(false)) color = theme::faint();
  return true;
}

lv_obj_t* column(lv_obj_t* scr) { return scrollColumn(scr); }

// The reminders follow the app when the owner lets them and the app has any.
bool followingApp() { return S.prefs.remindersFromApp && S.day.hasReminders; }

void caption(lv_obj_t* col, const char* label) {
  lv_obj_t* l = lv_label_create(col);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, theme::tiny(), 0);
  lv_obj_set_style_text_color(l, theme::faint(), 0);
  lv_obj_set_style_text_letter_space(l, 3, 0);
  lv_obj_set_style_pad_top(l, 16, 0);
  lv_obj_set_style_pad_bottom(l, 2, 0);
}

// A long name (a Wi-Fi network) is cut short rather than run into the value.
void fitName(lv_obj_t* name, lv_obj_t* value) {
  int32_t valueWidth = 0;
  if (value) {
    lv_obj_update_layout(value);
    valueWidth = lv_obj_get_width(value) + 12;
  }
  lv_obj_set_size(name, 286 - valueWidth, 24);
}

lv_obj_t* row(lv_obj_t* col, Row id, const char* label, const char* value, lv_color_t valueColor = theme::gold(),
              lv_color_t labelColor = theme::fg()) {
  lv_obj_t* btn = lv_button_create(col);
  lv_obj_set_size(btn, 326, 54);
  lv_obj_set_style_pad_hor(btn, 18, 0);
  lv_obj_set_style_radius(btn, 18, 0);
  lv_obj_set_style_bg_color(btn, theme::surface(), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(btn, theme::border(), 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x262B33), LV_STATE_PRESSED);
  lv_obj_t* l = lv_label_create(btn);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, theme::body(), 0);
  lv_obj_set_style_text_color(l, labelColor, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

  // A row with a setting says its own value; the rest say what they were given.
  String own;
  lv_color_t ownColor = valueColor;
  const bool live = valueOf(id, own, ownColor);
  lv_obj_t* v = nullptr;
  if (live || (value && *value)) {
    v = lv_label_create(btn);
    lv_label_set_text(v, live ? own.c_str() : value);
    lv_obj_set_style_text_font(v, theme::body(), 0);
    lv_obj_set_style_text_color(v, live ? ownColor : valueColor, 0);
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);
  }
  fitName(l, v);
  if (live) shown.push_back({static_cast<uintptr_t>(id), l, v});
  lv_obj_add_event_cb(btn, onRow, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(id)));
  return btn;
}

lv_obj_t* row(lv_obj_t* col, Row id, const char* label) { return row(col, id, label, nullptr); }

void backRow(lv_obj_t* col) {
  lv_obj_t* btn = lv_button_create(col);
  lv_obj_set_size(btn, 150, 44);
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
  lv_obj_add_event_cb(btn, onRow, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(Row::Back)));
}

void heading(lv_obj_t* col, const char* label) {
  lv_obj_t* l = lv_label_create(col);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, theme::large(), 0);
  lv_obj_set_style_text_color(l, theme::fg(), 0);
  lv_obj_set_style_pad_bottom(l, 4, 0);
}

void changed() {
  if (S.actions.prefsChanged) S.actions.prefsChanged(S.prefs);
  refreshSettings();
}

void rememberScroll() {
  if (list && S.mode == Mode::Settings) savedScroll = lv_obj_get_scroll_y(list);
}

void onRow(lv_event_t* e) {
  Prefs& p = S.prefs;
  const Row id = static_cast<Row>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
  switch (id) {
    case Row::Back: goBack(); return;
    case Row::ScreenOff:
      p.screenSeconds = p.screenSeconds <= 10 ? 15 : (p.screenSeconds <= 15 ? 30 : (p.screenSeconds <= 30 ? 60 : 10));
      break;
    case Row::Brightness: p.brightness = (p.brightness + 1) % 3; break;
    case Row::NightDim: p.nightDim = !p.nightDim; break;
    case Row::HomeFace: p.homeFace = (p.homeFace + 1) % 5; break;
    case Row::Animations: p.animations = !p.animations; break;
    case Row::TouchWakes: p.touchWakes = !p.touchWakes; break;
    case Row::DeskOn: p.deskAlwaysOn = !p.deskAlwaysOn; break;
    case Row::Sound: p.sound = !p.sound; break;
    case Row::Volume:
      p.volume = (p.volume + 1) % 3;
      if (p.sound && S.actions.bell) S.actions.bell(1);
      break;
    case Row::HourBell: p.hourBell = !p.hourBell; break;
    case Row::ReminderSource: p.remindersFromApp = !p.remindersFromApp; break;
    case Row::Reminder0:
    case Row::Reminder1:
    case Row::Reminder2:
    case Row::Reminder3:
      // Following the app, the times are the app's to change.
      if (followingApp()) return;
      rememberScroll();
      openTimeEditor(static_cast<int>(id) - static_cast<int>(Row::Reminder0));
      return;
    case Row::Update: rememberScroll(); openUpdate(); return;
    case Row::Wifi: rememberScroll(); openWifi(); return;
    case Row::SyncEvery:
      p.syncHours = p.syncHours == 0 ? 2 : (p.syncHours == 2 ? 4 : (p.syncHours == 4 ? 6 : (p.syncHours == 6 ? 12 : 0)));
      break;
    case Row::SyncNow:
      if (S.actions.syncNow) S.actions.syncNow();
      return;
    case Row::About: rememberScroll(); openAbout(); return;
    case Row::PowerOff: rememberScroll(); openConfirm(Confirm::PowerOff); return;
    case Row::Unlink: rememberScroll(); openConfirm(Confirm::Unlink); return;
    case Row::Erase: rememberScroll(); openConfirm(Confirm::Erase); return;
    case Row::AddNetwork:
      if (S.actions.openWifiPortal) S.actions.openWifiPortal();
      return;
    default:
      if (static_cast<int>(id) >= static_cast<int>(Row::Network0)) {
        openConfirm(Confirm::Forget, static_cast<int>(id) - static_cast<int>(Row::Network0));
      }
      return;
  }
  changed();
}

void buildList() {
  lv_obj_t* scr = S.settings;
  clear(scr);
  shown.clear();
  list = column(scr);
  backRow(list);
  heading(list, tr("Ajustes", "Settings", "Ajustes"));

  caption(list, tr("TELA", "SCREEN", "PANTALLA"));
  row(list, Row::ScreenOff, tr("Apaga em", "Off after", "Se apaga en"));
  row(list, Row::Brightness, tr("Brilho", "Brightness", "Brillo"));
  row(list, Row::NightDim, tr("Escurecer à noite", "Dim at night", "Atenuar de noche"));
  row(list, Row::HomeFace, tr("Tela inicial", "Home screen", "Pantalla inicial"));
  row(list, Row::Animations, tr("Animações", "Animations", "Animaciones"));
  row(list, Row::TouchWakes, tr("Toque acorda", "Touch wakes", "El toque despierta"));
  row(list, Row::DeskOn, tr("Acesa na tomada", "On while charging", "Encendida al cargar"));

  caption(list, tr("SOM", "SOUND", "SONIDO"));
  row(list, Row::Sound, tr("Som", "Sound", "Sonido"));
  row(list, Row::Volume, tr("Volume", "Volume", "Volumen"));
  row(list, Row::HourBell, tr("Sino às 6h, 12h e 18h", "Bell at 6, 12 and 18", "Campana 6h, 12h y 18h"));

  caption(list, tr("LEMBRETES", "REMINDERS", "RECORDATORIOS"));
  row(list, Row::ReminderSource, tr("Horários", "Times", "Horarios"));
  for (int i = 0; i < 4; i++) row(list, static_cast<Row>(static_cast<int>(Row::Reminder0) + i), officeName(i));

  caption(list, tr("REDE", "NETWORK", "RED"));
  row(list, Row::Wifi, "Wi-Fi", "›", theme::faint());
  row(list, Row::SyncEvery, tr("Sincronizar a cada", "Sync every", "Sincronizar cada"));
  row(list, Row::SyncNow, tr("Sincronizar agora", "Sync now", "Sincronizar ahora"));

  caption(list, tr("APARELHO", "DEVICE", "DISPOSITIVO"));
  row(list, Row::Update, tr("Atualização", "Update", "Actualización"));
  row(list, Row::About, tr("Sobre", "About", "Acerca de"), "›", theme::faint());
  row(list, Row::PowerOff, tr("Desligar", "Power off", "Apagar"), "›", theme::faint());
  if (S.status.linked) {
    row(list, Row::Unlink, tr("Desvincular do Ordo", "Unlink from Ordo", "Desvincular de Ordo"), "›", theme::faint());
  }
  row(list, Row::Erase, tr("Apagar tudo", "Erase everything", "Borrar todo"), "›", theme::faint(), theme::red());

  lv_obj_update_layout(list);
  lv_obj_scroll_to_y(list, savedScroll, LV_ANIM_OFF);
  // Opened from the menu, the top of the list arrives row by row.
  if (savedScroll == 0) {
    const uint32_t count = lv_obj_get_child_count(list);
    for (uint32_t i = 0; i < count && i < 8; i++) riseIn(lv_obj_get_child(list, static_cast<int32_t>(i)), i * 35, 10);
  }
}

// ------------------------------------------------------------ sub-screens

// Opens a screen behind a row. `build` draws it on the screen it is given.
void (*subBuilder)(lv_obj_t*) = nullptr;

void openSub(void (*build)(lv_obj_t*)) {
  S.mode = Mode::SettingsSub;
  subBuilder = build;
  transition([]() {
    if (S.mode != Mode::SettingsSub || !subBuilder) return;
    clear(S.sub);
    editFigure = nullptr;
    subBuilder(S.sub);
    load(S.sub);
  });
}

void buildTimeEditor(lv_obj_t* scr) {
  backChip(scr);
  char title[64];
  snprintf(title, sizeof(title), "%s  ·  %s", tr("LEMBRETE", "REMINDER", "RECORDATORIO"), upper(officeName(editOffice)).c_str());
  text(scr, title, theme::tiny(), theme::dim(), 78, 3, 300);
  editFigure = text(scr, clockText(editMinutes).c_str(), theme::digits(), theme::fg(), 110, 0, 340);

  struct Step {
    const char* label;
    int delta;
    int32_t x;
  };
  static const Step steps[] = {{"−", -60, -122}, {"+", 60, -56}, {"−", -5, 56}, {"+", 5, 122}};
  for (const Step& step : steps) {
    lv_obj_t* btn = lv_button_create(scr);
    lv_obj_set_size(btn, 58, 58);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, theme::surface(), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x262B33), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, theme::border(), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, step.x, 238);
    lv_obj_t* l = lv_label_create(btn);
    lv_label_set_text(l, step.label);
    lv_obj_set_style_text_font(l, theme::large(), 0);
    lv_obj_set_style_text_color(l, theme::fg(), 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          const int delta = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
          editMinutes = ((editMinutes + delta) % 1440 + 1440) % 1440;
          // Only the figure changes; the buttons stay under the finger.
          if (editFigure) lv_label_set_text(editFigure, clockText(editMinutes).c_str());
        },
        LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(step.delta)));
  }
  lv_obj_t* hours = text(scr, tr("HORA", "HOUR", "HORA"), theme::tiny(), theme::faint(), 302, 2, 120);
  lv_obj_align(hours, LV_ALIGN_TOP_MID, -89, 302);
  lv_obj_t* minutes = text(scr, "MIN", theme::tiny(), theme::faint(), 302, 2, 120);
  lv_obj_align(minutes, LV_ALIGN_TOP_MID, 89, 302);

  lv_obj_t* buttons = lv_obj_create(scr);
  lv_obj_remove_style_all(buttons);
  lv_obj_set_size(buttons, 340, 56);
  lv_obj_align(buttons, LV_ALIGN_TOP_MID, 0, 336);
  lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(buttons, 12, 0);
  pill(
      buttons, tr("Desligar", "Turn off", "Apagar"), false,
      [](lv_event_t*) {
        S.prefs.reminder[editOffice] = -1;
        if (S.actions.prefsChanged) S.actions.prefsChanged(S.prefs);
        openSettings();
      },
      nullptr);
  pill(
      buttons, tr("Pronto", "Done", "Listo"), true,
      [](lv_event_t*) {
        S.prefs.reminder[editOffice] = static_cast<int16_t>(editMinutes);
        if (S.actions.prefsChanged) S.actions.prefsChanged(S.prefs);
        openSettings();
      },
      nullptr);
}

void openTimeEditor(int office) {
  static const int defaults[] = {7 * 60, 12 * 60, 18 * 60, 21 * 60 + 30};
  editOffice = office;
  editMinutes = S.prefs.reminder[office] >= 0 ? S.prefs.reminder[office] : defaults[office];
  openSub(buildTimeEditor);
}

void buildWifi(lv_obj_t* scr) {
  lv_obj_t* col = column(scr);
  backRow(col);
  heading(col, "Wi-Fi");
  const Diagnostics info = S.actions.diagnostics ? S.actions.diagnostics() : Diagnostics();
  if (info.networks.empty()) {
    lv_obj_t* l = lv_label_create(col);
    lv_label_set_text(l, tr("Nenhuma rede guardada", "No saved network", "Ninguna red guardada"));
    lv_obj_set_style_text_font(l, theme::small(), 0);
    lv_obj_set_style_text_color(l, theme::dim(), 0);
  } else {
    caption(col, tr("REDES GUARDADAS", "SAVED NETWORKS", "REDES GUARDADAS"));
    for (size_t i = 0; i < info.networks.size(); i++) {
      row(col, static_cast<Row>(static_cast<int>(Row::Network0) + static_cast<int>(i)), info.networks[i].c_str(),
          tr("esquecer", "forget", "olvidar"), theme::faint());
    }
  }
  caption(col, tr("NOVA", "NEW", "NUEVA"));
  row(col, Row::AddNetwork, tr("Adicionar rede", "Add a network", "Añadir una red"), "+", theme::gold());
  lv_obj_t* hint = lv_label_create(col);
  lv_label_set_text(hint, tr("Casa, igreja, o roteador do celular: ele entra na que encontrar.",
                             "Home, church, your phone's hotspot: it joins whichever it finds.",
                             "Casa, iglesia, el móvil: entra en la que encuentre."));
  lv_obj_set_width(hint, 300);
  lv_label_set_long_mode(hint, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(hint, theme::italic(), 0);
  lv_obj_set_style_text_color(hint, theme::dim(), 0);
  lv_obj_set_style_pad_top(hint, 10, 0);
}

void openWifi() { openSub(buildWifi); }

void aboutLine(lv_obj_t* col, const char* label, const char* value) {
  lv_obj_t* box = lv_obj_create(col);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, 320, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(box, 8, 0);
  lv_obj_t* l = lv_label_create(box);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, theme::tiny(), 0);
  lv_obj_set_style_text_color(l, theme::faint(), 0);
  lv_obj_set_style_text_letter_space(l, 2, 0);
  lv_obj_t* v = lv_label_create(box);
  lv_label_set_text(v, value);
  lv_obj_set_width(v, 300);
  lv_label_set_long_mode(v, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(v, theme::body(), 0);
  lv_obj_set_style_text_color(v, theme::fg(), 0);
}

void buildAbout(lv_obj_t* scr) {
  lv_obj_t* col = column(scr);
  backRow(col);
  heading(col, "Ordo");
  const Diagnostics info = S.actions.diagnostics ? S.actions.diagnostics() : Diagnostics();
  char buf[96];

  snprintf(buf, sizeof(buf), "v%s", info.firmware.c_str());
  aboutLine(col, tr("VERSÃO", "VERSION", "VERSIÓN"), buf);
  if (!info.deviceName.isEmpty()) aboutLine(col, tr("NOME NO ORDO", "NAME IN ORDO", "NOMBRE EN ORDO"), info.deviceName.c_str());
  if (info.batteryPercent >= 0) {
    snprintf(buf, sizeof(buf), "%d%%  ·  %d,%02d V", info.batteryPercent, info.batteryMillivolts / 1000,
             (info.batteryMillivolts % 1000) / 10);
    aboutLine(col, tr("BATERIA", "BATTERY", "BATERÍA"), buf);
  }
  if (S.status.syncedMinutesAgo == UINT32_MAX) {
    snprintf(buf, sizeof(buf), "%s", tr("ainda não sincronizou", "not synchronised yet", "aún sin sincronizar"));
  } else if (S.status.syncedMinutesAgo < 60) {
    snprintf(buf, sizeof(buf), tr("há %lu min", "%lu min ago", "hace %lu min"),
             static_cast<unsigned long>(S.status.syncedMinutesAgo));
  } else {
    snprintf(buf, sizeof(buf), tr("há %lu h", "%lu h ago", "hace %lu h"),
             static_cast<unsigned long>(S.status.syncedMinutesAgo / 60));
  }
  aboutLine(col, tr("ÚLTIMA SINCRONIZAÇÃO", "LAST SYNC", "ÚLTIMA SINCRONIZACIÓN"), buf);
  snprintf(buf, sizeof(buf), tr("acordou %lu vezes, sincronizou %lu", "woke %lu times, synced %lu",
                               "despertó %lu veces, sincronizó %lu"),
           static_cast<unsigned long>(info.wakesToday), static_cast<unsigned long>(info.syncsToday));
  aboutLine(col, tr("HOJE", "TODAY", "HOY"), buf);
  snprintf(buf, sizeof(buf), "%+ld ppm", static_cast<long>(info.clockDriftPpm));
  aboutLine(col, tr("DESVIO DO RELÓGIO", "CLOCK DRIFT", "DESVÍO DEL RELOJ"), buf);
  aboutLine(col, tr("PLACA", "BOARD", "PLACA"), info.board.c_str());
}

void openAbout() { openSub(buildAbout); }

void buildConfirm(lv_obj_t* scr) {
  const Confirm what = pending;
  const char* title = "";
  const char* body = "";
  const char* action = "";
  bool danger = false;
  switch (what) {
    case Confirm::PowerOff:
      title = tr("Desligar?", "Power off?", "¿Apagar?");
      body = tr("Para ligar de novo, aperte o botão PWR. Lembretes e sino não tocam com ele desligado.",
                "Press PWR to switch it back on. Reminders and the bell do not ring while it is off.",
                "Pulse PWR para encenderlo. Los recordatorios y la campana no suenan apagado.");
      action = tr("Desligar", "Power off", "Apagar");
      break;
    case Confirm::Unlink:
      title = tr("Desvincular?", "Unlink?", "¿Desvincular?");
      body = tr("Ele esquece a sua conta e volta a pedir um código. As redes Wi-Fi ficam.",
                "It forgets your account and asks for a code again. Wi-Fi networks stay.",
                "Olvida su cuenta y vuelve a pedir un código. Las redes Wi-Fi se conservan.");
      action = tr("Desvincular", "Unlink", "Desvincular");
      danger = true;
      break;
    case Confirm::Erase:
      title = tr("Apagar tudo?", "Erase everything?", "¿Borrar todo?");
      body = tr("Conta, redes Wi-Fi e ajustes são apagados e ele reinicia como novo.",
                "Account, Wi-Fi networks and settings are erased and it restarts as new.",
                "Cuenta, redes Wi-Fi y ajustes se borran y se reinicia como nuevo.");
      action = tr("Apagar", "Erase", "Borrar");
      danger = true;
      break;
    case Confirm::Forget:
      title = tr("Esquecer a rede?", "Forget this network?", "¿Olvidar la red?");
      body = tr("Ele deixa de entrar nela sozinho.", "It stops joining it by itself.", "Deja de entrar en ella solo.");
      action = tr("Esquecer", "Forget", "Olvidar");
      break;
  }
  text(scr, title, theme::large(), theme::fg(), 118, 0, 320);
  text(scr, body, theme::body(), theme::dim(), 172, 0, 310);

  lv_obj_t* buttons = lv_obj_create(scr);
  lv_obj_remove_style_all(buttons);
  lv_obj_set_size(buttons, 360, 56);
  lv_obj_align(buttons, LV_ALIGN_TOP_MID, 0, 318);
  lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(buttons, 12, 0);
  pill(buttons, tr("Cancelar", "Cancel", "Cancelar"), false, [](lv_event_t*) { goBack(); }, nullptr);
  pill(
      buttons, action, true,
      [](lv_event_t*) {
        switch (pending) {
          case Confirm::PowerOff:
            if (S.actions.powerOff) S.actions.powerOff();
            break;
          case Confirm::Unlink:
            if (S.actions.unlink) S.actions.unlink();
            break;
          case Confirm::Erase:
            if (S.actions.factoryReset) S.actions.factoryReset();
            break;
          case Confirm::Forget:
            if (S.actions.forgetNetwork) S.actions.forgetNetwork(pendingIndex);
            openWifi();
            break;
        }
      },
      nullptr, danger ? theme::red() : theme::gold());
}

void openConfirm(Confirm what, int index) {
  pending = what;
  pendingIndex = index;
  openSub(buildConfirm);
}

// The firmware: what is installed, what is on offer, and the two things to
// do about it. Built again whenever the search changes state.
void buildUpdate(lv_obj_t* scr) {
  backChip(scr);
  const Diagnostics info = S.actions.diagnostics ? S.actions.diagnostics() : Diagnostics();
  const Status& st = S.status;
  text(scr, tr("ATUALIZAÇÃO", "UPDATE", "ACTUALIZACIÓN"), theme::tiny(), theme::dim(), 82, 3, 260);
  char installed[48];
  snprintf(installed, sizeof(installed), "v%s", info.firmware.c_str());

  if (st.update == UpdateState::Available) {
    char offer[48];
    snprintf(offer, sizeof(offer), "v%s", st.updateVersion.c_str());
    text(scr, offer, theme::display(), theme::gold(), 110, 0, 300);
    char from[72];
    snprintf(from, sizeof(from), tr("instalada: %s", "installed: %s", "instalada: %s"), installed);
    text(scr, from, theme::small(), theme::dim(), 168, 0, 280);
    if (!st.updateNotes.isEmpty()) {
      lv_obj_t* notes = text(scr, st.updateNotes.c_str(), theme::small(), theme::fg(), 198, 0, 300);
      lv_label_set_long_mode(notes, LV_LABEL_LONG_MODE_DOTS);
      lv_obj_set_height(notes, 88);
    }
    // Writing a firmware on a battery that may give out half way is how a
    // device ends up needing a cable.
    const bool enough = st.onUsb || st.batteryPercent < 0 || st.batteryPercent >= 40;
    if (enough) {
      lv_obj_t* go = pill(
          scr, tr("Instalar", "Install", "Instalar"), true,
          [](lv_event_t*) {
            if (S.actions.installUpdate) S.actions.installUpdate();
          },
          nullptr);
      lv_obj_align(go, LV_ALIGN_TOP_MID, 0, 300);
      text(scr, tr("leva um minuto; mantenha perto do Wi-Fi", "takes a minute; keep it near the Wi-Fi",
                   "tarda un minuto; manténgala cerca del Wi-Fi"),
           theme::tiny(), theme::faint(), 362, 0, 250);
    } else {
      text(scr, tr("Ligue no cabo para instalar: a bateria está abaixo de 40%.", "Plug it in to install: the battery is under 40%.",
                   "Conéctela para instalar: la batería está por debajo del 40%."),
           theme::small(), theme::red(), 304, 0, 270);
    }
    return;
  }

  text(scr, installed, theme::display(), theme::fg(), 124, 0, 300);
  const char* state = "";
  lv_color_t color = theme::dim();
  switch (st.update) {
    case UpdateState::Checking: state = tr("procurando…", "looking…", "buscando…"); break;
    case UpdateState::UpToDate: state = tr("é a versão mais recente", "this is the latest version", "es la versión más reciente"); color = theme::green(); break;
    case UpdateState::Failed: state = tr("não deu para procurar agora", "could not look right now", "no se pudo buscar ahora"); color = theme::red(); break;
    default: state = tr("procura sozinha uma vez por dia, na tomada", "it looks by itself once a day, when plugged in",
                        "busca sola una vez al día, enchufada"); break;
  }
  text(scr, state, theme::small(), color, 196, 0, 270);
  if (st.update != UpdateState::Checking) {
    lv_obj_t* look = pill(
        scr, tr("Procurar agora", "Look now", "Buscar ahora"), false,
        [](lv_event_t*) {
          if (S.actions.checkUpdate) S.actions.checkUpdate();
        },
        nullptr);
    lv_obj_align(look, LV_ALIGN_TOP_MID, 0, 272);
  }
}

}  // namespace

void openSettings() {
  const bool fresh = S.mode != Mode::Settings && S.mode != Mode::SettingsSub;
  if (fresh) savedScroll = 0;
  S.mode = Mode::Settings;
  transition([]() {
    if (S.mode != Mode::Settings) return;
    buildList();
    load(S.settings);
  });
}

// Brings the values on screen up to date: after a tap, or when the status
// changes under the list (a sync starting or ending).
void openUpdate() { openSub(buildUpdate); }

void refreshSettings() {
  // The update screen follows the search as it goes.
  if (S.mode == Mode::SettingsSub && subBuilder == buildUpdate && lv_screen_active() == S.sub) {
    static UpdateState shownState = UpdateState::Unknown;
    static String shownVersion;
    if (shownState != S.status.update || shownVersion != S.status.updateVersion) {
      shownState = S.status.update;
      shownVersion = S.status.updateVersion;
      clear(S.sub);
      buildUpdate(S.sub);
    }
    return;
  }
  if (S.mode != Mode::Settings || lv_screen_active() != S.settings) return;
  for (const Shown& item : shown) {
    String value;
    lv_color_t color = theme::gold();
    if (!valueOf(static_cast<Row>(item.id), value, color)) continue;
    if (value == lv_label_get_text(item.value)) continue;
    lv_label_set_text(item.value, value.c_str());
    lv_obj_set_style_text_color(item.value, color, 0);
    fitName(item.name, item.value);
    pulse(item.value);
  }
}

}  // namespace ui::detail
