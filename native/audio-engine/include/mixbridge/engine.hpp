#pragma once

#include "mixbridge/mix_graph.hpp"
#include "mixbridge/ring_buffer.hpp"

#include <atomic>
#include <cstdint>
#include <string>

namespace mixbridge {

struct EngineStats {
  std::atomic<uint64_t> xruns{0};
  std::atomic<uint64_t> frames_rendered{0};
  std::atomic<bool> running{false};
};

class Engine {
public:
  Engine();
  ~Engine();

  MixGraph& graph() { return graph_; }
  EngineStats& stats() { return stats_; }

  // Offline/process fixture path used by unit tests and early integration.
  void render_offline(uint32_t frames, MixBuses& out);

  bool start();
  void stop();

private:
  MixGraph graph_;
  EngineStats stats_;
  SpscFloatRing control_meter_ring_;
};

}  // namespace mixbridge
