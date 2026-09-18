#include "mixbridge/mix_graph.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mixbridge {
namespace {

float midi_note_hz(int32_t note) {
  return 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
}

void render_starter_instrument(SourceSlot& slot, uint32_t frames, float* dst) {
  const uint32_t samples = frames * kEngineChannels;
  for (uint32_t i = 0; i < samples; ++i) dst[i] = 0.0f;

  const uint32_t preset = std::min(slot.instrument_preset.load(std::memory_order_relaxed), 1u);
  const float attack_seconds = preset == 1 ? 0.16f : 0.008f;
  const float release_seconds = preset == 1 ? 0.85f : 0.22f;
  const float attack_step = 1.0f / (attack_seconds * static_cast<float>(kEngineRate));
  const float release_step = 1.0f / (release_seconds * static_cast<float>(kEngineRate));

  for (uint32_t v = 0; v < kStarterVoices; ++v) {
    auto& control = slot.instrument_voices[v];
    auto& state = slot.instrument_state[v];
    const int32_t note = control.note.load(std::memory_order_relaxed);
    const bool gate = control.gate.load(std::memory_order_acquire);
    const float velocity = std::clamp(control.velocity.load(std::memory_order_relaxed), 0.0f, 1.0f);
    if (note < 0 && state.envelope <= 0.0001f) continue;

    if (note >= 0 && note != state.latched_note) {
      state.latched_note = note;
      state.phase = 0.0;
    }
    const float hz = note >= 0 ? midi_note_hz(note) : 0.0f;
    const double inc = 2.0 * std::numbers::pi * static_cast<double>(hz) /
                       static_cast<double>(kEngineRate);

    for (uint32_t i = 0; i < frames; ++i) {
      if (gate) state.envelope = std::min(1.0f, state.envelope + attack_step);
      else state.envelope = std::max(0.0f, state.envelope - release_step);
      if (state.envelope <= 0.0001f || hz <= 0.0f) continue;

      const double p = state.phase;
      float wave = 0.0f;
      if (preset == 1) {
        wave = static_cast<float>(
          0.76 * std::sin(p) + 0.17 * std::sin(p * 2.0) + 0.07 * std::sin(p * 0.5));
      } else {
        wave = static_cast<float>(
          0.78 * std::sin(p) + 0.16 * std::sin(p * 2.0) + 0.06 * std::sin(p * 3.0));
      }
      const float sample = wave * state.envelope * velocity * 0.10f;
      dst[i * 2] += sample;
      dst[i * 2 + 1] += sample;
      state.phase += inc;
      if (state.phase > 2.0 * std::numbers::pi) state.phase -= 2.0 * std::numbers::pi;
    }
  }
}

void process_chunk(
  SourceSlot* slots,
  uint32_t frames,
  float* monitor_out,
  float* broadcast_out,
  float master_gain) {
  bool any_solo = false;
  for (uint32_t s = 0; s < kMaxSources; ++s) {
    if (!slots[s].active.load(std::memory_order_relaxed)) continue;
    if (slots[s].solo.load(std::memory_order_relaxed) &&
        !slots[s].mute.load(std::memory_order_relaxed)) {
      any_solo = true;
      break;
    }
  }

  alignas(64) float src_buf[512 * 2];
  alignas(64) float dry_backup[512 * 2];

  for (uint32_t s = 0; s < kMaxSources; ++s) {
    auto& slot = slots[s];
    if (!slot.active.load(std::memory_order_relaxed)) continue;
    if (slot.mute.load(std::memory_order_relaxed)) continue;
    if (any_solo && !slot.solo.load(std::memory_order_relaxed)) continue;

    const auto kind = static_cast<SourceKind>(slot.kind.load(std::memory_order_relaxed));
    const float gain = slot.gain.load(std::memory_order_relaxed) * master_gain;
    const float pan = std::clamp(slot.pan.load(std::memory_order_relaxed), -1.0f, 1.0f);
    const float gL = gain * (0.5f * (1.0f - pan));
    const float gR = gain * (0.5f * (1.0f + pan));
    const bool to_mon = slot.monitor.load(std::memory_order_relaxed);
    const bool to_bc = slot.broadcast.load(std::memory_order_relaxed);

    const uint32_t need = frames * kEngineChannels;
    if (kind == SourceKind::ToneFixture) {
      const float hz = slot.tone_hz.load(std::memory_order_relaxed);
      const double inc = (hz > 0.0f) ? (2.0 * std::numbers::pi * static_cast<double>(hz) /
                                        static_cast<double>(kEngineRate))
                                     : 0.0;
      for (uint32_t i = 0; i < frames; ++i) {
        const float sample = static_cast<float>(std::sin(slot.tone_phase) * 0.2);
        slot.tone_phase += inc;
        src_buf[i * 2] = sample;
        src_buf[i * 2 + 1] = sample;
      }
    } else if (kind == SourceKind::StarterInstrument) {
      render_starter_instrument(slot, frames, src_buf);
    } else {
      const auto got = static_cast<uint32_t>(slot.ring.read(src_buf, need));
      for (uint32_t i = got; i < need; ++i) src_buf[i] = 0.0f;
    }

    if (auto* effect = slot.effect.load(std::memory_order_acquire)) {
      std::copy(src_buf, src_buf + need, dry_backup);
      if (!effect->process(src_buf, frames)) {
        std::copy(dry_backup, dry_backup + need, src_buf);
      }
    }

    // Meter is intentionally post-FX: the source card shows what the user hears.
    slot.meter.accumulate(src_buf, frames, kEngineChannels);

    for (uint32_t i = 0; i < frames; ++i) {
      const float l = src_buf[i * 2] * gL;
      const float r = src_buf[i * 2 + 1] * gR;
      if (to_mon) {
        monitor_out[i * 2] += l;
        monitor_out[i * 2 + 1] += r;
      }
      if (to_bc) {
        broadcast_out[i * 2] += l;
        broadcast_out[i * 2 + 1] += r;
      }
    }
  }
}

}  // namespace

void MixGraph::apply_safety_limiter(float* interleaved, std::size_t samples) {
  // Soft clip that asymptotes at ±1.0 (transparent at modest levels).
  for (std::size_t i = 0; i < samples; ++i) {
    interleaved[i] = std::tanh(interleaved[i]);
  }
}

void MixGraph::process(
  SourceSlot* slots,
  uint32_t frames,
  float* monitor_out,
  float* broadcast_out,
  float master_gain,
  bool apply_broadcast_limiter) {
  const uint32_t samples = frames * kEngineChannels;
  for (uint32_t i = 0; i < samples; ++i) {
    monitor_out[i] = 0.0f;
    broadcast_out[i] = 0.0f;
  }

  constexpr uint32_t kChunk = 512;
  uint32_t offset = 0;
  while (offset < frames) {
    const uint32_t n = std::min(kChunk, frames - offset);
    process_chunk(slots, n, monitor_out + offset * kEngineChannels,
                  broadcast_out + offset * kEngineChannels, master_gain);
    offset += n;
  }

  if (apply_broadcast_limiter) {
    apply_safety_limiter(broadcast_out, samples);
  }
}

}  // namespace mixbridge
