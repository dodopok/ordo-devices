#include "rtc_state.h"

#include <esp_attr.h>
#include <string.h>

namespace sys {
namespace {
RTC_DATA_ATTR RtcState state;
}

RtcState& rtc() { return state; }

bool rtcBegin() {
  if (state.magic == kRtcMagic) return true;
  memset(&state, 0, sizeof(state));
  state.magic = kRtcMagic;
  state.firedDay = -1;
  state.countersDay = -1;
  return false;
}

}  // namespace sys
