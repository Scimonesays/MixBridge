#pragma once

#include "mixbridge/ring_buffer.hpp"
#include "mixbridge/types.hpp"
#include "mixbridge/meters.hpp"
#include "mixbridge/effect_processor.hpp"

#include <atomic>
#include <cstdint>
#include <string>

namespace mixbridge {

// Control + audio slot. Audio thread only reads atomics / ring / tone phase.
struct StarterVoiceControl {
  std::atomic<int32_t> note{-1};
  std::atomic<float> velocity{0.0f};
  std::atomic<bool> gate{false};
};

struct StarterVoiceState {
  double phase = 0.0;
  float envelope = 0.0f;
  int32_t latched_note = -1;
};

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
  std::atomic<uint32_t> instrument_preset{0};
  std::atomic<uint32_t> instrument_next_voice{0};
  StarterVoiceControl instrument_voices[kStarterVoices];
  StarterVoiceState instrument_state[kStarterVoices];
  std::atomic<RealtimeEffect*> effect{nullptr};

  // Non-realtime metadata (do not touch from audio thread).
  std::wstring device_id;
  std::string name;
  std::string effect_name;
  std::string effect_path;

  SpscFloatRing ring{kRingFrames};
  AtomicMeter meter{};
  double tone_phase = 0.0;  // audio-thread only
};

}  // namespace mixbridge
