#pragma once

#include "mixbridge/source_slot.hpp"
#include "mixbridge/types.hpp"

#include <cstddef>

namespace mixbridge {

// Realtime mixer: no heap allocation. Outputs are caller-provided.
class MixGraph {
public:
  // slots: array of kMaxSources; views filled with interleaved stereo float or nullptr.
  // scratch_mono used for temporary mono→stereo if needed (frames floats).
  void process(
    SourceSlot* slots,
    uint32_t frames,
    float* monitor_out,
    float* broadcast_out,
    float master_gain,
    bool apply_broadcast_limiter);

  static void apply_safety_limiter(float* interleaved, std::size_t samples);
};

}  // namespace mixbridge
