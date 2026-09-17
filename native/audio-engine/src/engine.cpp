#include "mixbridge/engine.hpp"

namespace mixbridge {

Engine::Engine() : control_meter_ring_(1u << 14) {}
Engine::~Engine() { stop(); }

void Engine::render_offline(uint32_t frames, MixBuses& out) {
  graph_.process(frames, out);
  stats_.frames_rendered.fetch_add(frames, std::memory_order_relaxed);
}

bool Engine::start() {
  stats_.running.store(true, std::memory_order_release);
  return true;
}

void Engine::stop() {
  stats_.running.store(false, std::memory_order_release);
}

}  // namespace mixbridge
