// Draws every screen of the compass to image files, on a computer.
//   sim <out-dir> <today.json>            every screen, one frame each
//   sim <out-dir> <today.json> exercise   pokes every control (run under ASAN)
//   sim <out-dir> <today.json> movie      a walk through the screens, frame by
//                                         frame, with the dips and entrances
// The frames are raw PPM; to_png.py cuts the round glass and writes PNGs, and
// to_gif.py strings the movie's frames together.

#include <lvgl.h>

#include <fstream>
#include <sstream>
#include <string>

#include <vector>

#include "content/art.h"
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

// ---- what the firmware keeps in flash, here read from a folder
std::string contentDir = "content";

std::string slurp(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

// The picture goes through the same decoder as on the board.
ui::Art artFor(const String& slug) {
  static std::vector<uint16_t> pixels(static_cast<size_t>(content::kArtSide) * content::kArtSide);
  static std::string shown;
  ui::Art art;
  const std::string bytes = slurp(contentDir + "/" + slug + ".jpg");
  if (bytes.empty()) return art;
  if (shown != slug) {
    if (!content::decodeArt(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), pixels.data())) {
      fprintf(stderr, "sim: %s.jpg did not decode\n", slug.c_str());
      shown.clear();
      return art;
    }
    shown = slug;
  }
  art.pixels = reinterpret_cast<const uint8_t*>(pixels.data());
  art.side = content::kArtSide;
  return art;
}

ui::CelebrationText textFor(const String& slug) {
  ui::CelebrationText text;
  const std::string json = slurp(contentDir + "/" + slug + ".pt-BR.json");
  model::parseCelebrationText(json.c_str(), json.size(), text);
  return text;
}

// A day with a celebration, as a sync would have left it: the sample's
// Wednesday, with the offices and the week it had on Monday.
model::Today celebrationDay(const model::Today& base, int ahead) {
  model::Today day = model::viewFor(base, base.day.days + ahead);
  day.officesKnown = true;
  for (int i = 0; i < 4; i++) day.offices[i] = base.offices[i];
  day.completedToday = base.completedToday;
  for (int i = 0; i < 7; i++) day.week[i] = base.week[i];
  day.hasWeek = base.hasWeek;
  return day;
}

// With animations on, a screen change takes a dip and a rise: every wait is
// stretched so that what follows finds the screen it expects.
bool animated = false;
// In a movie every step is a frame on disk.
bool filming = false;
void writeFrame(const char* path, uint8_t light);

void step() {
  nowMs += 33;
  lv_timer_handler();
  if (filming) {
    char path[512];
    snprintf(path, sizeof(path), "%s/f%04d.ppm", outDir.c_str(), ++shot);
    writeFrame(path, ui::brightnessScale());
  }
}

void settle(int steps = 12) {
  if (animated) steps += 14;
  for (int i = 0; i < steps; i++) step();
}

// The glass as the eye sees it: what LVGL drew, times the panel's light.
void writeFrame(const char* path, uint8_t light) {
  std::ofstream f(path, std::ios::binary);
  f << "P6\n" << kSize << " " << kSize << "\n255\n";
  for (int i = 0; i < kSize * kSize; i++) {
    const uint16_t c = frame[i];
    const unsigned char rgb[3] = {static_cast<unsigned char>(((c >> 11) & 0x1F) * 255 / 31 * light / 255),
                                  static_cast<unsigned char>(((c >> 5) & 0x3F) * 255 / 63 * light / 255),
                                  static_cast<unsigned char>((c & 0x1F) * 255 / 31 * light / 255)};
    f.write(reinterpret_cast<const char*>(rgb), 3);
  }
}

void snap(const char* name) {
  settle();
  char path[512];
  snprintf(path, sizeof(path), "%s/%02d-%s.ppm", outDir.c_str(), ++shot, name);
  writeFrame(path, 255);
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
  for (int i = 0; i < 4; i++) step();
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
  fprintf(stderr, "sim: row \"%s\" not found (mode %d, %s)\n", label, static_cast<int>(ui::detail::S.mode),
          lv_screen_active() == ui::detail::S.settings ? "list" : (lv_screen_active() == ui::detail::S.sub ? "sub" : "other"));
}

int carouselFailures = 0;

// Pokes everything a finger or a button can reach. Nothing here checks a
// picture; it is there to crash (under the address sanitizer, loudly) if a
// screen frees something it still uses.
void exercise(const model::Today& today) {
  using namespace ui;
  using ui::detail::S;
  setToday(today);
  goHome();
  for (int lap = 0; lap < 2; lap++) {
    for (int i = 0; i < 7; i++) { onSwipe(Swipe::Up); settle(2); }
    for (int i = 0; i < 7; i++) { onSwipe(Swipe::Down); settle(2); }
    for (int i = 0; i < 7; i++) { onBootPress(); settle(2); }
  }
  onBootLongPress();

  // Another home screen moves to the front of the carousel; "Agora" follows
  // it, and nothing drops out.
  {
    const uint8_t was = S.prefs.homeFace;
    S.prefs.homeFace = 3;  // Ofícios
    const bool feast = today.day.celebration.present();
    const Page expected[] = {Page::Offices, Page::Now, Page::Celebration, Page::Compass, Page::Readings, Page::Week, Page::Menu};
    Page at = Page::Offices;
    for (int i = 0; i < 7; i++) {
      if (expected[i] == Page::Celebration && !feast) continue;
      if (at != expected[i]) carouselFailures++;
      at = detail::neighbour(at, true);
    }
    if (at != Page::Offices) carouselFailures++;                         // all the way round
    if (detail::neighbour(Page::Offices, false) != Page::Menu) carouselFailures++;
    goHome();
    settle(2);
    onSwipe(Swipe::Up);
    settle(2);
    if (S.page != Page::Now) carouselFailures++;
    S.prefs.homeFace = was;
    goHome();
    settle(2);
  }

  // The four tiles of the menu by touch, each tool, and the way back.
  for (int item = 0; item < 4; item++) {
    detail::showPage(Page::Menu);
    settle(2);
    tap(item % 2 == 0 ? 154 : 312, item < 2 ? 154 : 312);
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
      for (const char* row : {"Apaga em", "Brilho", "Escurecer à noite", "Toque acorda", "Acesa na tomada", "Som", "Volume",
                              "Sino às 6h, 12h e 18h", "Sincronizar a cada", "Sincronizar agora"}) {
        tapRow(row);
        tapRow(row);
      }
      for (int i = 0; i < 5; i++) tapRow("Tela inicial");        // all five and round again
      // Following the app, the reminder rows only show the app's times...
      S.prefs.remindersFromApp = true;
      tapRow("Manhã");
      settle(2);
      // ...and set here, each opens its editor.
      S.prefs.remindersFromApp = false;
      tapRow("Horários");
      tapRow("Horários");
      S.prefs.remindersFromApp = false;
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
      settle(2);
      // The update screen, in each state the search can be in.
      tapRow("Atualização");
      for (UpdateState state : {UpdateState::Checking, UpdateState::Available, UpdateState::UpToDate, UpdateState::Failed}) {
        Status st = S.status;
        st.update = state;
        st.updateVersion = state == UpdateState::Available ? "9.9.9" : "";
        st.batteryPercent = state == UpdateState::Available ? 20 : 80;
        setStatus(st);
        settle(1);
        tap(233, 296);                                         // Procurar agora
        tap(233, 324);                                         // Instalar
        settle(1);
      }
      onBootPress();
      settle(2);
      for (const char* row : {"Desligar", "Desvincular do Ordo", "Apagar tudo"}) {
        tapRow(row);
        tap(170, 346);                                         // Cancelar
        settle(2);
      }
      onBootPress();
    }
  }

  // The celebration, if the day has one: open, read, back, closed, with a
  // picture or its text arriving in the middle.
  if (today.day.celebration.present()) {
    detail::showPage(Page::Celebration);
    settle(2);
    tap(233, 200);                                             // opens
    settle(6);
    tap(233, 412);                                             // Ler tudo
    settle(2);
    onSwipe(Swipe::Up);
    onSwipe(Swipe::Down);
    contentArrived(today.day.celebration.slug);
    onSwipe(Swipe::Right);                                     // back to the page, still open
    settle(2);
    tap(233, 120);                                             // closes
    contentArrived(today.day.celebration.slug);
    tap(233, 200);
    onSwipe(Swipe::Up);                                        // leaves in the middle of the slide
    settle(2);
    onSwipe(Swipe::Down);
    tap(233, 412);
    onBootPress();
    onBootPress();
    settle(2);
  }

  // Notices over the day, dismissed every way there is.
  detail::showPage(Page::Now);
  showReminder(1); settle(2); tap(233, 233); settle(2);
  showHourBell(18); settle(2); onSwipe(Swipe::Down); settle(2);
  showSilenceDone(); settle(2); onBootPress(); settle(2);
  showStreakReminder(); settle(2); tap(233, 233); settle(2);
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

  // An impatient hand: swipes, presses and notices that land in the middle
  // of a screen change, with no wait in between.
  for (int i = 0; i < 40; i++) {
    switch (i % 8) {
      case 0: onSwipe(Swipe::Up); break;
      case 1: onSwipe(Swipe::Left); step(); break;
      case 2: onBootPress(); step(); step(); break;
      case 3: detail::showPage(Page::Menu); step(); detail::openBeads(); onBootPress(); break;
      case 4: step(); step(); step(); onBootPress(); onBootLongPress(); detail::goBack(); break;
      case 5: detail::openSettings(); step(); showReminder(i % 4); step(); break;
      case 6: dismissNotice(); detail::openSilence(); setSilence(true, 300); step(); setSilence(false, 0); break;
      case 7: detail::openTorch(); step(); step(); goHome(); for (int k = 0; k < i; k++) step(); break;
    }
    if (i % 5 == 0) { detail::showPage(Page::Celebration); step(); tap(233, 200); detail::openReading(); step(); showUpdating("9.9.9", i); }
    if (i % 7 == 0) { detail::openUpdate(); step(); Status st = S.status; st.update = static_cast<UpdateState>(i % 5); st.updateVersion = "9.9.9"; setStatus(st); }
  }
  settle(20);
}

// A walk through the screens for the movie: what a thumb would do on the way
// from waking it to counting a few beads and changing a setting.
void film(const model::Today& today) {
  using namespace ui;
  auto wait = [](int frames) { for (int i = 0; i < frames; i++) step(); };
  setToday(today);                       // the wake: the light rises on "Agora"
  wait(48);
  onSwipe(Swipe::Up);                    // the celebration: its picture whole
  wait(60);
  tap(233, 200);                         // a touch: the words come up
  wait(50);
  tap(233, 412);                         // Ler tudo
  wait(40);
  lv_obj_scroll_to_y(lv_obj_get_child(lv_screen_active(), 0), 260, LV_ANIM_ON);
  wait(45);
  onSwipe(Swipe::Right);                 // back to the page
  wait(36);
  for (int i = 0; i < 5; i++) {          // round the carousel to the menu
    onSwipe(Swipe::Up);
    wait(42);
  }
  tap(154, 154);                         // Contas
  wait(34);
  for (int i = 0; i < 4; i++) { tap(233, 150); wait(8); }
  onSwipe(Swipe::Down);                  // back to the menu
  wait(30);
  tap(312, 312);                         // Ajustes
  wait(34);
  tapRow("Brilho");
  wait(10);
  tapRow("Tela inicial");
  wait(10);
  tapRow("Tela inicial");
  wait(18);
  onSwipe(Swipe::Right);
  wait(30);
  onBootLongPress();                     // home
  wait(45);
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

  const char* mode = argc > 3 ? argv[3] : "";
  ui::Prefs prefs;
  prefs.reminder[0] = 7 * 60;
  prefs.reminder[2] = 18 * 60;
  // The stills are taken with animations off: each call below leaves its
  // screen finished, with nothing half way.
  prefs.animations = false;
  ui::Actions actions;
  actions.prefsChanged = [](const ui::Prefs&) {};
  actions.diagnostics = []() {
    ui::Diagnostics d;
    d.firmware = "0.4.1";
    d.board = "ESP32-S3-Touch-AMOLED-1.75C";
    d.deviceName = "Chaveiro";
    d.batteryPercent = 82;
    d.batteryMillivolts = 4020;
    d.wakesToday = 23;
    d.syncsToday = 5;
    d.clockDriftPpm = 412;
    d.freeMemoryKb = 148;
    d.lowestMemoryKb = 86;
    d.nextWake = "18:00";
    d.networks = {"Casa", "Paróquia São Paulo", "iPhone de Douglas"};
    return d;
  };
  actions.silence = [](int minutes) { ui::setSilence(minutes > 0, static_cast<uint32_t>(minutes) * 60); };
  if (const char* dir = getenv("ORDO_SIM_CONTENT")) contentDir = dir;
  actions.art = artFor;
  actions.celebrationText = textFor;
  ui::init(prefs, actions);

  ui::Status st;
  st.clockValid = true;
  st.minuteOfDay = 14 * 60 + 32;
  st.batteryPercent = 82;
  st.linked = true;
  st.wifiBars = 3;
  st.syncedMinutesAgo = 12;

  // `sim <out> <json> movie`: frames of a walk through the screens.
  if (strcmp(mode, "movie") == 0) {
    prefs.animations = true;
    actions.bell = [](int) {};
    ui::init(prefs, actions);
    ui::setStatus(st);
    filming = true;
    film(celebrationDay(today, 2));
    printf("%d frames in %s\n", shot, outDir.c_str());
    return 0;
  }

  // `sim <out> <json> exercise`: poke every control instead of drawing, once
  // with every change immediate and once through the dips and entrances.
  if (strcmp(mode, "exercise") == 0) {
    actions.bell = [](int) {};
    actions.syncNow = []() {};
    actions.openWifiPortal = []() {};
    actions.forgetNetwork = [](int) {};
    actions.unlink = []() {};
    actions.factoryReset = []() {};
    actions.powerOff = []() {};
    actions.sleepNow = []() {};
    actions.checkUpdate = []() {};
    actions.installUpdate = []() {};
    int moves = 0;
    actions.brightnessMoved = [&moves]() { moves++; };
    for (int pass = 0; pass < 2; pass++) {
      animated = pass == 1;
      prefs.animations = animated;
      ui::init(prefs, actions);
      ui::detail::S.prefs = prefs;
      ui::Status st0;
      st0.clockValid = true;
      st0.minuteOfDay = 14 * 60 + 32;
      st0.linked = true;
      ui::setStatus(st0);
      exercise(today);
      exercise(celebrationDay(today, 2));                       // a celebration with its picture
      exercise(model::viewFor(today, today.day.days + 5));      // one whose picture never came
      exercise(model::viewFor(today, today.day.days + 3));
      exercise(model::viewFor(today, today.day.days + 40));
      if (carouselFailures) {
        printf("the carousel is out of order (%d)\n", carouselFailures);
        return 1;
      }
      if (ui::changingScreen() || ui::brightnessScale() != 255) {
        printf("exercise left the glass dark (scale %d)\n", ui::brightnessScale());
        return 1;
      }
    }
    printf("exercise finished, the light moved %d times\n", moves);
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
  ui::showStreakReminder();
  snap("sequencia");
  ui::showLowBattery();
  snap("bateria");

  // ---- a day with a celebration
  ui::setToday(celebrationDay(today, 2));
  ui::detail::showPage(ui::Page::Now);
  snap("agora-comemoracao");
  ui::detail::showPage(ui::Page::Celebration);
  snap("comemoracao");
  tap(233, 200);
  snap("comemoracao-aberta");
  ui::detail::openReading();
  snap("comemoracao-texto");
  lv_obj_scroll_to_y(lv_obj_get_child(ui::detail::S.sub, 0), 330, LV_ANIM_OFF);
  snap("comemoracao-texto-2");
  // Its picture has not come (or the CMS has none): the same page, in type.
  ui::setToday(celebrationDay(today, 5));
  ui::detail::showPage(ui::Page::Celebration);
  snap("comemoracao-sem-foto");
  // A day of abstinence.
  ui::setToday(celebrationDay(today, 4));
  ui::detail::showPage(ui::Page::Now);
  snap("agora-jejum");
  ui::setToday(today);

  // ---- the firmware update
  st.update = ui::UpdateState::Available;
  st.updateVersion = "0.4.1";
  st.updateNotes = "Fotos abrem mais depressa e o sino das horas ganhou volume próprio.";
  ui::setStatus(st);
  ui::detail::openUpdate();
  snap("atualizacao-disponivel");
  ui::showUpdating("0.4.1", 37);
  snap("atualizando");
  st.update = ui::UpdateState::UpToDate;
  st.updateVersion = "";
  st.updateNotes = "";
  ui::setStatus(st);
  ui::detail::openUpdate();
  snap("atualizacao-em-dia");
  st.update = ui::UpdateState::Failed;
  st.updateDetail = "conexão -1, TLS -0x2700";
  ui::setStatus(st);
  ui::detail::openUpdate();
  snap("atualizacao-falhou");
  st.update = ui::UpdateState::Unknown;
  st.updateDetail = "";
  ui::setStatus(st);
  // Another home screen: it comes first, "Agora" second.
  ui::detail::S.prefs.homeFace = 1;
  ui::goHome();
  snap("inicio-bussola");
  ui::onSwipe(ui::Swipe::Up);
  snap("inicio-bussola-agora");
  ui::detail::S.prefs.homeFace = 0;
  ui::detail::showPage(ui::Page::Now);

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
