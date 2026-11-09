#pragma once

#include <Arduino.h>

namespace net {

// Connects with the credentials saved on the device; without any, opens an
// access point named "Ordo-XXXX" with a captive portal where the person
// picks a network. Non-blocking: call wifiLoop() from the main loop.
void wifiBegin();
void wifiLoop();

bool wifiConnected();
bool wifiPortalActive();
// The access point name shown on screen while the portal is open.
String wifiPortalSsid();

// Forgets the saved network. Used by the factory reset.
void wifiForget();

}  // namespace net
