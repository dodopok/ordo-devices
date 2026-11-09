#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "../model/today.h"

namespace ui {

enum class Screen { Compass, Readings, Offices };

// Creates the screens. Call once after the display and touch are up.
void init();

// Full-screen notices, used while there is nothing liturgical to show yet.
void showSplash();
void showMessage(const String& title, const String& text, bool spinner);
void showWifiPortal(const String& ssid);
// The link screen: code, QR of the verification URL and a countdown.
void showLink(const String& userCode, const String& verificationUri);
void setLinkCountdown(uint32_t secondsLeft);

// Replaces the day shown on the three liturgical screens. `offline` marks
// the day as the last one the compass managed to fetch.
void setToday(const model::Today& today, bool offline);
void setBattery(int percent, bool charging);

// Brings the compass face back (PWR button, wake).
void goHome();
bool showingDay();

}  // namespace ui
