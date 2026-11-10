#pragma once

#include <Arduino.h>

#include <vector>

// The pictures and texts of the celebrations, kept in the LittleFS partition
// so that a wake without a network still has them. Files are named by a hash
// of the celebration's slug (see model::contentKey) and written whole or not
// at all: the compass can lose power or be put to sleep at any moment.
namespace store {

enum class ContentKind { Art, Text };

// Mounts the volume, formatting it the first time. Safe to call again.
bool contentBegin();
bool contentHas(const String& slug, ContentKind kind);
bool contentWrite(const String& slug, ContentKind kind, const uint8_t* data, size_t length);
// The whole file, in a buffer the caller frees with free(); nullptr when
// there is none. Large ones land in PSRAM.
uint8_t* contentRead(const String& slug, ContentKind kind, size_t& length);
void contentRemove(const String& slug, ContentKind kind);
// Deletes what belongs to no slug in `keep`: the days that have gone by.
void contentPrune(const std::vector<String>& keep);
void contentWipe();

}  // namespace store
