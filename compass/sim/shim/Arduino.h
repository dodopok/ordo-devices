// A sliver of Arduino for the screen simulator: just what src/ui and
// src/model use. Not part of the firmware.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

class String : public std::string {
 public:
  String() = default;
  String(const char* s) : std::string(s ? s : "") {}
  String(const std::string& s) : std::string(s) {}
  String(char c) : std::string(1, c) {}
  bool isEmpty() const { return empty(); }
  bool startsWith(const char* prefix) const { return rfind(prefix, 0) == 0; }
  bool startsWith(const String& prefix) const { return rfind(prefix, 0) == 0; }
};

uint32_t millis();
