#include "mixbridge/mix_graph.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mixbridge {

uint32_t MixGraph::add_source(SourceNode node) {
  node.id = next_id_++;
  sources_.push_back(std::move(node));
  return sources_.back().id;
}

bool MixGraph::remove_source(uint32_t id) {
  const auto it = std::remove_if(sources_.begin(), sources_.end(),
                                 [&](const SourceNode& s) { return s.id == id; });
  if (it == sources_.end()) return false;
  sources_.erase(it, sources_.end());
  return true;
}

SourceNode* MixGraph::find(uint32_t id) {
  for (auto& s : sources_) {
    if (s.id == id) return &s;
  }
  return nullptr;
}

void MixGraph::apply_safety_limiter(float* interleaved, std::size_t samples) {
  // Simple tanh soft clip — protects against catastrophic peaks without a hard squash.
  for (std::size_t i = 0; i < samples; ++i) {
    interleaved[i] = std::tanh(interleaved[i] * 1.2f) / std::tanh(1.2f);
  }
}

void MixGraph::process(uint32_t frames, MixBuses& out) {
  const std::size_t samples = static_cast<std::size_t>(frames) * kChannels;
  out.monitor.assign(samples, 0.0f);
  out.broadcast.assign(samples, 0.0f);

  bool any_solo = false;
  for (const auto& s : sources_) {
    if (s.solo && !s.mute) any_solo = true;
  }

  std::size_t idx = 0;
  for (auto& s : sources_) {
    if (s.mute) {
      ++idx;
      continue;
    }
    if (any_solo && !s.solo) {
      ++idx;
      continue;
    }

    double& phase = tone_phase_[idx % 64];
    const double inc = (s.kind == SourceKind::ToneFixture && s.tone_hz > 0.0)
                         ? (2.0 * std::numbers::pi * s.tone_hz / static_cast<double>(kRate))
                         : 0.0;

    const float pan = std::clamp(s.pan, -1.0f, 1.0f);
    const float gL = s.gain * (0.5f * (1.0f - pan));
    const float gR = s.gain * (0.5f * (1.0f + pan));

    for (uint32_t i = 0; i < frames; ++i) {
      float sample = 0.0f;
      if (inc > 0.0) {
        sample = static_cast<float>(std::sin(phase) * 0.2);
        phase += inc;
      }
      const float l = sample * gL * master_gain_;
      const float r = sample * gR * master_gain_;
      if (s.monitor) {
        out.monitor[i * 2] += l;
        out.monitor[i * 2 + 1] += r;
      }
      if (s.broadcast) {
        out.broadcast[i * 2] += l;
        out.broadcast[i * 2 + 1] += r;
      }
    }
    ++idx;
  }

  apply_safety_limiter(out.broadcast.data(), out.broadcast.size());
}

}  // namespace mixbridge
