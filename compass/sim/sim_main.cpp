// Draws every screen of the compass to image files, on a computer.
//   sim <out-dir> <today.json>
// The frames are raw PPM; to_png.py cuts the round glass and writes PNGs.

#include <lvgl.h>

#include <fstream>
#include <sstream>
#include <string>

#include "ui/ui.h"
#include "ui/ui_internal.h"

namespace {

constexpr int kSize = 466;
uint32_t nowMs = 0;
uint16_t frame[kSize * kSize];
std::string outDir;
int shot = 0;

void flush(lv_display_t* d, const lv_area_t* area, uint8_t* px) {
  const int w = lv_area_get_width(area);
  const uint16_t* src = reinterpret_cast<const uint16_t*>(px);
  for (int y = area->y1; y <= area->y2; y++) {
    memcpy(&frame[y * kSize + area->x1], src, static_cast<size_t>(w) * 2);
    src += w;
  }
  lv_display_flush_ready(d);
}

void settle(int steps = 12) {
  for (int i = 0; i < steps; i++) {
    nowMs += 33;
    lv_timer_handler();
  }
}

void snap(const char* name) {
  settle();
  char path[512];
  snprintf(path, sizeof(path), "%s/%02d-%s.ppm", outDir.c_str(), ++shot, name);
  std::ofstream f(path, std::ios::binary);
  f << "P6\n" << kSize << " " << kSize << "\n255\n";
  for (int i = 0; i < kSize * kSize; i++) {
    const uint16_t c = frame[i];
    const unsigned char rgb[3] = {static_cast<unsigned char>(((c >> 11) & 0x1F) * 255 / 31),
                                  static_cast<unsigned char>(((c >> 5) & 0x3F) * 255 / 63),
                                  static_cast<unsigned char>((c & 0x1F) * 255 / 31)};
    f.write(reinterpret_cast<const char*>(rgb), 3);
  }
}

// A pointer for the simulator: taps go through LVGL's own input path, so a
// control that rebuilds the screen from inside its click handler is tested
// the way the glass would test it.
bool pointerDown = false;
lv_point_t pointerAt = {0, 0};

void pointerRead(lv_indev_t*, lv_indev_data_t* data) {
  data->point = pointerAt;
  data->state = pointerDown ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void tap(int32_t x, int32_t y) {
  pointerAt.x = x;
  pointerAt.y = y;
  pointerDown = true;
  settle(4);
  pointerDown = false;
  settle(4);
}

void tapObject(lv_obj_t* obj) {
  lv_area_t area;
  lv_obj_update_layout(obj);
  lv_obj_get_coords(obj, &area);
  tap((area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2);
}

// Taps every row of the list on screen that has a label, top to bottom,
// scrolling each into view first. Rows that open another screen are left to
// the caller; this is for the ones that change a value in place.
void tapRow(const char* label) {
  lv_obj_t* col = lv_obj_get_child(lv_screen_active(), 0);
  const uint32_t count = lv_obj_get_child_count(col);
  for (uint32_t i = 0; i < count; i++) {
    lv_obj_t* row = lv_obj_get_child(col, static_cast<int32_t>(i));
    lv_obj_t* first = lv_obj_get_child(row, 0);
    if (!first || !lv_obj_check_type(first, &lv_label_class)) continue;
    if (strcmp(lv_label_get_text(first), label) != 0) continue;
    lv_obj_scroll_to_view(row, LV_ANIM_OFF);
    settle(2);
    tapObject(row);
    return;
  }
  fprintf(stderr, "sim: row \"%s\" not found\n", label);
}

// Pokes everything a finger or a button can reach. Nothing here checks a
// picture; it is there to crash (under the address sanitizer, loudly) if a
// screen frees something it still uses.
void exercise(const model::Today& today) {
  using namespace ui;
  setToday(today);
  goHome();
  for (int lap = 0; lap < 2; lap++) {
    for (int i = 0; i < 7; i++) { onSwipe(Swipe::Up); settle(2); }
    for (int i = 0; i < 7; i++) { onSwipe(Swipe::Down); settle(2); }
    for (int i = 0; i < 7; i++) { onBootPress(); settle(2); }
  }
  onBootLongPress();

  // Menu rows by touch, each tool, and the way back.
  for (int item = 0; item < 4; item++) {
    detail::showPage(Page::Menu);
    settle(2);
    tap(233, 141 + item * 66);
    settle(2);
    if (item == 0) {
      for (int i = 0; i < 40; i++) { tap(233, 150); }           // past the end of the rosary and round again
      for (int mode = 0; mode < 4; mode++) { tap(233, 324); for (int i = 0; i < 5; i++) onBootPress(); settle(2); }
      onBootLongPress();
      onSwipe(Swipe::Left);
      onSwipe(Swipe::Right);
    } else if (item == 1) {
      tap(233, 216);                                           // 10 min
      settle(4);
      setSilence(true, 599);
      setSilence(true, 598);
      tap(233, 374);                                           // Encerrar
      settle(2);
      onBootPress();
    } else if (item == 2) {
      tap(233, 233);
    } else {
      for (const char* row : {"Apaga em", "Brilho", "Escurecer à noite", "Tela inicial", "Toque acorda", "Acesa na tomada", "Som",
                              "Volume", "Sino às 6h, 12h e 18h", "Sincronizar a cada", "Sincronizar agora"}) {
        tapRow(row);
        tapRow(row);
      }
      tapRow("Manhã");
      for (int i = 0; i < 3; i++) { tap(111, 267); tap(177, 267); tap(289, 267); tap(355, 267); }
      tap(300, 364);                                           // Pronto
      settle(2);
      tapRow("Completas");
      tap(170, 364);                                           // Desligar
      settle(2);
      tapRow("Wi-Fi");
      tapRow("Casa");
      tap(170, 346);                                           // Cancelar
      settle(2);
      tapRow("Sobre");
      onSwipe(Swipe::Up);
      onSwipe(Swipe::Right);
      for (const char* row : {"Desligar", "Desvincular do Ordo", "Apagar tudo"}) {
        tapRow(row);
        tap(170, 346);                                         // Cancelar
        settle(2);
      }
      onBootPress();
    }
  }

  // Notices over the day, dismissed every way there is.
  detail::showPage(Page::Now);
  showReminder(1); settle(2); tap(233, 233); settle(2);
  showHourBell(18); settle(2); onSwipe(Swipe::Down); settle(2);
  showSilenceDone(); settle(2); onBootPress(); settle(2);
  showReminder(3); settle(2); setToday(today); settle(2); dismissNotice();

  // A status that changes under every screen.
  Status st;
  st.linked = true;
  for (int minute = 0; minute < 6; minute++) {
    st.clockValid = minute % 2 == 0;
    st.minuteOfDay = 23 * 60 + 57 + minute;
    st.batteryPercent = 100 - minute * 19;
    st.charging = minute % 3 == 0;
    st.syncing = minute % 2 == 1;
    setStatus(st);
    onSwipe(Swipe::Up);
    settle(2);
  }
  clearDay();
  showMessage("Sem dia", "nada para mostrar", false);
  settle(2);
  setToday(today);
  settle(2);
}

// Clicks the n-th row of a scrolling list (the first child of the screen).
void clickRow(lv_obj_t* screen, const char* label) {
  lv_obj_t* col = lv_obj_get_child(screen, 0);
  const uint32_t count = lv_obj_get_child_count(col);
  for (uint32_t i = 0; i < count; i++) {
    lv_obj_t* row = lv_obj_get_child(col, static_cast<int32_t>(i));
    lv_obj_t* first = lv_obj_get_child(row, 0);
    if (!first || !lv_obj_check_type(first, &lv_label_class)) continue;
    if (strcmp(lv_label_get_text(first), label) == 0) {
      lv_obj_send_event(row, LV_EVENT_CLICKED, nullptr);
      return;
    }
  }
  fprintf(stderr, "sim: row \"%s\" not found\n", label);
}

}  // namespace

uint32_t millis() { return nowMs; }

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: sim <out-dir> <today.json>\n");
    return 2;
  }
  outDir = argv[1];
  std::ifstream in(argv[2]);
  std::stringstream buffer;
  buffer << in.rdbuf();
  const std::string json = buffer.str();

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return nowMs; });
  static uint16_t draw[kSize * kSize];
  lv_display_t* disp = lv_display_create(kSize, kSize);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, flush);
  lv_display_set_buffers(disp, draw, nullptr, sizeof(draw), LV_DISPLAY_RENDER_MODE_PARTIAL);

  model::Today today;
  if (!model::parseToday(json.c_str(), today)) {
    fprintf(stderr, "sim: could not parse %s\n", argv[2]);
    return 1;
  }

  lv_indev_t* pointer = lv_indev_create();
  lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(pointer, pointerRead);

  ui::Prefs prefs;
  prefs.reminder[0] = 7 * 60;
  prefs.reminder[2] = 18 * 60;
  ui::Actions actions;
  actions.prefsChanged = [](const ui::Prefs&) {};
  actions.diagnostics = []() {
    ui::Diagnostics d;
    d.firmware = "0.2.0";
    d.board = "ESP32-S3-Touch-AMOLED-1.75C";
    d.deviceName = "Chaveiro";
    d.batteryPercent = 82;
    d.batteryMillivolts = 4020;
    d.wakesToday = 23;
    d.syncsToday = 5;
    d.clockDriftPpm = 412;
    d.networks = {"Casa", "Paróquia São Paulo", "iPhone de Douglas"};
    return d;
  };
  actions.silence = [](int minutes) { ui::setSilence(minutes > 0, static_cast<uint32_t>(minutes) * 60); };
  ui::init(prefs, actions);

  ui::Status st;
  st.clockValid = true;
  st.minuteOfDay = 14 * 60 + 32;
  st.batteryPercent = 82;
  st.linked = true;
  st.syncedMinutesAgo = 12;

  // `sim <out> <json> exercise`: poke every control instead of drawing.
  if (argc > 3 && strcmp(argv[3], "exercise") == 0) {
    actions.bell = [](int) {};
    actions.syncNow = []() {};
    actions.openWifiPortal = []() {};
    actions.forgetNetwork = [](int) {};
    actions.unlink = []() {};
    actions.factoryReset = []() {};
    actions.powerOff = []() {};
    actions.sleepNow = []() {};
    ui::init(prefs, actions);
    ui::Status st0;
    st0.clockValid = true;
    st0.minuteOfDay = 14 * 60 + 32;
    st0.linked = true;
    ui::setStatus(st0);
    exercise(today);
    exercise(model::viewFor(today, today.day.days + 3));
    exercise(model::viewFor(today, today.day.days + 40));
    printf("exercise finished\n");
    return 0;
  }

  // ---- before there is a day
  ui::showSplash();
  snap("abertura");
  ui::showWifiPortal("Ordo-7A3F", ui::PortalStep::WaitingPhone, "");
  snap("wifi-qr");
  ui::showWifiPortal("Ordo-7A3F", ui::PortalStep::PhoneConnected, "");
  snap("wifi-celular");
  ui::showWifiPortal("Ordo-7A3F", ui::PortalStep::Connecting, "Casa");
  snap("wifi-conectando");
  ui::showWifiPortal("Ordo-7A3F", ui::PortalStep::Connected, "Casa");
  snap("wifi-conectado");
  ui::showLink("KQ7-MD4", "https://oficio.app/aparelhos/vincular?code=KQ7-MD4");
  ui::setLinkCountdown(9 * 60 + 41);
  snap("vinculo");
  ui::showMessage("Vinculado", "buscando o dia de hoje…", true);
  snap("mensagem");

  // ---- the day
  ui::setStatus(st);
  ui::setToday(today);
  snap("agora");
  ui::detail::showPage(ui::Page::Compass);
  snap("bussola");
  ui::detail::showPage(ui::Page::Readings);
  snap("leituras");
  ui::detail::showPage(ui::Page::Offices);
  snap("oficios");
  ui::detail::showPage(ui::Page::Week);
  snap("semana");
  ui::detail::showPage(ui::Page::Menu);
  snap("menu");

  // ---- tools
  ui::setBeadsCount(12);
  ui::detail::openBeads();
  snap("contas");
  ui::detail::S.prefs.beadsMode = 3;
  ui::setBeadsCount(37);
  ui::detail::openBeads();
  snap("contas-corda");
  ui::detail::S.prefs.beadsMode = 0;
  ui::setBeadsCount(33);
  ui::detail::openBeads();
  snap("contas-completo");
  ui::setBeadsCount(12);
  ui::detail::openSilence();
  snap("silencio");
  ui::setSilence(true, 14 * 60 + 20);
  ui::detail::openSilence();
  snap("silencio-correndo");
  ui::setSilence(false, 0);
  ui::detail::openTorch();
  snap("lanterna");

  // ---- settings
  ui::detail::openSettings();
  snap("ajustes");
  lv_obj_scroll_to_y(lv_obj_get_child(ui::detail::S.settings, 0), 430, LV_ANIM_OFF);
  snap("ajustes-som");
  lv_obj_scroll_to_y(lv_obj_get_child(ui::detail::S.settings, 0), 1010, LV_ANIM_OFF);
  snap("ajustes-rede");
  clickRow(ui::detail::S.settings, "Tarde");
  snap("ajustes-lembrete");
  ui::detail::openSettings();
  clickRow(ui::detail::S.settings, "Wi-Fi");
  snap("ajustes-wifi");
  ui::detail::openSettings();
  clickRow(ui::detail::S.settings, "Sobre");
  snap("ajustes-sobre");
  ui::detail::openSettings();
  clickRow(ui::detail::S.settings, "Apagar tudo");
  snap("ajustes-confirmar");

  // ---- notices over the day
  ui::detail::showPage(ui::Page::Now);
  ui::showReminder(2);
  snap("lembrete");
  ui::showHourBell(12);
  snap("sino");
  ui::showSilenceDone();
  snap("silencio-fim");
  ui::showLowBattery();
  snap("bateria");

  // ---- other days
  model::Today done = today;
  for (auto& office : done.offices) office.done = true;
  done.completedToday = 4;
  done.streakCurrent = 13;
  ui::setToday(done);
  ui::detail::showPage(ui::Page::Now);
  snap("agora-completo");
  ui::detail::showPage(ui::Page::Compass);
  snap("bussola-completo");

  // The day turned twice without a network: a feast from the week in store.
  st.minuteOfDay = 7 * 60 + 5;
  st.syncedMinutesAgo = 2 * 24 * 60;
  ui::setStatus(st);
  ui::setToday(model::viewFor(today, today.day.days + 2));
  ui::detail::showPage(ui::Page::Now);
  snap("agora-sem-rede");
  ui::detail::showPage(ui::Page::Offices);
  snap("oficios-sem-rede");
  ui::detail::showPage(ui::Page::Readings);
  snap("leituras-festa");

  // Past the week in store: only what the ring knows.
  ui::setToday(model::viewFor(today, today.day.days + 13));
  ui::detail::showPage(ui::Page::Compass);
  snap("bussola-so-anel");

  // No clock yet.
  st.clockValid = false;
  st.syncedMinutesAgo = UINT32_MAX;
  st.batteryPercent = 9;
  ui::setStatus(st);
  ui::setToday(today);
  ui::detail::showPage(ui::Page::Now);
  snap("agora-sem-relogio");

  printf("%d frames in %s\n", shot, outDir.c_str());
  return 0;
}
