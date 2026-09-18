#pragma once

#include "mixbridge/ring_buffer.hpp"
#include "mixbridge/types.hpp"
#include "mixbridge/meters.hpp"

#include <atomic>
#include <cstdint>
#include <string>

namespace mixbridge {

// Control + audio slot. Audio thread only reads atomics / ring / tone phase.
struct SourceSlot {
  std::atomic<bool> active{false};
  std::atomic<uint32_t> id{0};
  std::atomic<uint32_t> kind{static_cast<uint32_t>(SourceKind::ToneFixture)};
  std::atomic<float> gain{1.0f};
  std::atomic<float> pan{0.0f};
  std::atomic<bool> mute{false};
  std::atomic<bool> solo{false};
  std::atomic<bool> monitor{true};
  std::atomic<bool> broadcast{true};
  std::atomic<float> tone_hz{0.0f};
  std::atomic<uint32_t> process_id{0};

  // Non-realtime metadata (do not touch from audio thread).
  std::wstring device_id;
  std::string name;

  // Optional FX hook (set from control thread; process from audio thread).
  using FxProcessFn = void (*)(void* ctx, float* interleaved_stereo, uint32_t frames);
  std::atomic<void*> fx_ctx{nullptr};
  std::atomic<FxProcessFn> fx_process{nullptr};
  std::atomic<bool> fx_bypass{true};
  std::string fx_name;

  SpscFloatRing ring{kRingFrames};
  AtomicMeter meter{};
  double tone_phase = 0.0;  // audio-thread only
};

}  // namespace mixbridge
