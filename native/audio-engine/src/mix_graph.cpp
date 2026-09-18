#include "mixbridge/mix_graph.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mixbridge {
namespace {

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
    } else {
      const auto got = static_cast<uint32_t>(slot.ring.read(src_buf, need));
      for (uint32_t i = got; i < need; ++i) src_buf[i] = 0.0f;
    }

    auto fx = slot.fx_process.load(std::memory_order_relaxed);
    void* fx_ctx = slot.fx_ctx.load(std::memory_order_relaxed);
    if (fx && fx_ctx && !slot.fx_bypass.load(std::memory_order_relaxed)) {
      fx(fx_ctx, src_buf, frames);
    }

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
