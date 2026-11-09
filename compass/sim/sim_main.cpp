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
