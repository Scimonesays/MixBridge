#pragma once

#include <cstdint>

namespace mixbridge {

// Realtime-safe effect contract used by the mixer. Implementations must not
// allocate, block, log, touch the filesystem, or acquire UI/control locks in
// process(). Returning false asks the mixer to fall back to the dry block.
class RealtimeEffect {
public:
  virtual ~RealtimeEffect() = default;
  virtual bool process(float* interleaved_stereo, uint32_t frames) noexcept = 0;
};

}  // namespace mixbridge
