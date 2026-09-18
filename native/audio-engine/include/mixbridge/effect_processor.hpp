#pragma once

#include <cstdint>
#include <string_view>

namespace mixbridge {

// Realtime-safe effect contract used by the mixer. Implementations must not
// allocate, block, log, touch the filesystem, or acquire UI/control locks in
// process(). Returning false asks the mixer to fall back to the dry block.
class RealtimeEffect {
public:
  virtual ~RealtimeEffect() = default;
  virtual bool process(float* interleaved_stereo, uint32_t frames) noexcept = 0;
  virtual void set_bypass(bool) noexcept {}
  virtual bool bypass() const noexcept { return false; }
  virtual bool faulted() const noexcept { return false; }
  virtual std::string_view name() const noexcept { return {}; }
};

}  // namespace mixbridge
