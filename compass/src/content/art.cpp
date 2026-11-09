#include "art.h"

#include <string.h>

#include <JPEGDEC.h>

namespace content {
namespace {

// The decoder keeps some 18 KB of tables: too much for a stack, and needed
// for a moment a few times a day. One instance, made when first used.
JPEGDEC* decoder = nullptr;

struct Target {
  uint16_t* pixels;
  int side;
};

// The decoder hands over the picture in blocks of whole MCUs, which overhang
// the right and bottom edges of a picture whose side is not a multiple of
// sixteen. Only what falls inside the square is kept.
int keep(JPEGDRAW* draw) {
  const Target* target = static_cast<const Target*>(draw->pUser);
  for (int row = 0; row < draw->iHeight; row++) {
    const int y = draw->y + row;
    if (y < 0 || y >= target->side) continue;
    int width = draw->iWidth;
    if (draw->x + width > target->side) width = target->side - draw->x;
    if (draw->x < 0 || width <= 0) continue;
    memcpy(target->pixels + static_cast<size_t>(y) * target->side + draw->x, draw->pPixels + static_cast<size_t>(row) * draw->iWidth,
           static_cast<size_t>(width) * sizeof(uint16_t));
  }
  return 1;
}

}  // namespace

bool decodeArt(const uint8_t* jpeg, size_t length, uint16_t* out, int side) {
  if (!jpeg || !out || side <= 0 || length < 64 || length > 512 * 1024) return false;
  memset(out, 0, static_cast<size_t>(side) * side * sizeof(uint16_t));
  if (!decoder) decoder = new JPEGDEC();
  // openRAM only reads; the cast is the library's signature.
  if (!decoder->openRAM(const_cast<uint8_t*>(jpeg), static_cast<int>(length), keep)) return false;
  bool ok = decoder->getWidth() == side && decoder->getHeight() == side;
  if (ok) {
    Target target{out, side};
    decoder->setUserPointer(&target);
    decoder->setPixelType(RGB565_LITTLE_ENDIAN);
    ok = decoder->decode(0, 0, 0) == 1;
  }
  decoder->close();
  if (!ok) memset(out, 0, static_cast<size_t>(side) * side * sizeof(uint16_t));
  return ok;
}

}  // namespace content
