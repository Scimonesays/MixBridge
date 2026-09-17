#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mixbridge {

enum class SourceKind : uint8_t {
  PhysicalCapture = 0,
  SystemLoopback = 1,
  ProcessLoopback = 2,
  ToneFixture = 3,
};

struct SourceNode {
  uint32_t id = 0;
  SourceKind kind = SourceKind::ToneFixture;
  std::string name;
  float gain = 1.0f;
  float pan = 0.0f;  // -1 left .. +1 right
  bool mute = false;
  bool solo = false;
  bool monitor = true;
  bool broadcast = true;
  double tone_hz = 0.0;  // for ToneFixture
};

struct MixBuses {
  std::vector<float> monitor;    // interleaved stereo
  std::vector<float> broadcast;  // interleaved stereo
};

class MixGraph {
public:
  static constexpr uint32_t kRate = 48000;
  static constexpr uint32_t kChannels = 2;

  uint32_t add_source(SourceNode node);
  bool remove_source(uint32_t id);
  SourceNode* find(uint32_t id);
  const std::vector<SourceNode>& sources() const { return sources_; }

  // Render `frames` of mix from tone fixtures + injected PCM (later).
  void process(uint32_t frames, MixBuses& out);

  void set_master_gain(float g) { master_gain_ = g; }
  float master_gain() const { return master_gain_; }

  // Soft safety limiter on broadcast bus (transparent for normal levels).
  static void apply_safety_limiter(float* interleaved, std::size_t samples);

private:
  std::vector<SourceNode> sources_;
  uint32_t next_id_ = 1;
  float master_gain_ = 1.0f;
  double tone_phase_[64]{};
};

}  // namespace mixbridge
