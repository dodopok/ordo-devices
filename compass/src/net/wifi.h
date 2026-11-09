#pragma once

#include <Arduino.h>

#include "../store/settings.h"

// Wi-Fi for a device that leaves home. The radio is off unless something
// asked for it: a sync, or the setup page. Up to five networks are kept
// (home, church, a phone's hotspot) and the compass joins whichever is in
// range, remembering the access point so the next join takes about a second.
namespace net {

enum class WifiState { Off, Connecting, Connected, Failed };

// Reads the stored networks. On a device upgraded from firmware 0.1 it also
// adopts the network the old captive portal had saved. The radio stays off.
void wifiBegin(store::Settings* settings);
bool wifiConfigured();

// Starts joining the best known network in range. Returns at once; follow
// wifiState(). Does nothing while already connecting or connected.
void wifiConnect();
void wifiOff();
WifiState wifiState();
bool wifiConnected();
// Forgets every network, including what the Wi-Fi driver keeps for itself.
void wifiForgetAll();

// Drives joins and the setup page. Call from loop().
void wifiLoop();

// ---- setup page
// The compass opens its own network ("Ordo-XXXX", no password) and serves
// one page with the networks in range. Only ever started on a first boot
// with no network, or when the owner asks for it in settings; it closes by
// itself after cfg::kPortalTimeoutMs.
enum class PortalState { Off, WaitingPhone, PhoneConnected, Connecting, Connected, Failed };

void portalStart();
void portalStop();
PortalState portalState();
const String& portalSsid();
// The network the phone asked the compass to join.
const String& portalNetwork();

}  // namespace net
