#pragma once

#include "mixbridge/types.hpp"

#include <atomic>
#include <cmath>

namespace mixbridge {

// Realtime-safe meter accumulators. UI reads snapshots at 30–60 Hz.
struct AtomicMeter {
  std::atomic<float> peak{0.0f};
  std::atomic<float> rms_energy{0.0f};  // sum of squares / count tracking via energy+count
  std::atomic<uint32_t> sample_count{0};
  std::atomic<uint32_t> clip{0};

  void reset() {
    peak.store(0.0f, std::memory_order_relaxed);
    rms_energy.store(0.0f, std::memory_order_relaxed);
    sample_count.store(0, std::memory_order_relaxed);
    clip.store(0, std::memory_order_relaxed);
  }

  // Called from audio thread only.
  void accumulate(const float* interleaved, uint32_t frames, uint32_t channels) {
    float local_peak = peak.load(std::memory_order_relaxed);
    float energy = rms_energy.load(std::memory_order_relaxed);
    uint32_t count = sample_count.load(std::memory_order_relaxed);
    uint32_t clipped = clip.load(std::memory_order_relaxed);
    const uint32_t n = frames * channels;
    for (uint32_t i = 0; i < n; ++i) {
      const float a = interleaved[i] < 0 ? -interleaved[i] : interleaved[i];
      if (a > local_peak) local_peak = a;
      energy += interleaved[i] * interleaved[i];
      ++count;
      if (a >= 0.999f) clipped = 1;
    }
    peak.store(local_peak, std::memory_order_relaxed);
    rms_energy.store(energy, std::memory_order_relaxed);
    sample_count.store(count, std::memory_order_relaxed);
    clip.store(clipped, std::memory_order_relaxed);
  }

  // Called from non-realtime thread. Resets accumulators after read.
  MeterSnapshot snapshot_and_reset() {
    MeterSnapshot s;
    const float p = peak.exchange(0.0f, std::memory_order_relaxed);
    const float e = rms_energy.exchange(0.0f, std::memory_order_relaxed);
    const uint32_t c = sample_count.exchange(0, std::memory_order_relaxed);
    const uint32_t cl = clip.exchange(0, std::memory_order_relaxed);
    s.peak = p;
    s.rms = (c > 0) ? std::sqrt(e / static_cast<float>(c)) : 0.0f;
    s.clip = cl != 0;
    return s;
  }
};

}  // namespace mixbridge
