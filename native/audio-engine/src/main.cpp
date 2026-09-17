#include "mixbridge/engine.hpp"

#include <cstdio>

int main() {
  mixbridge::Engine engine;
  engine.graph().add_source(mixbridge::SourceNode{
    .kind = mixbridge::SourceKind::ToneFixture,
    .name = "fixture-440",
    .tone_hz = 440.0,
  });
  engine.graph().add_source(mixbridge::SourceNode{
    .kind = mixbridge::SourceKind::ToneFixture,
    .name = "fixture-1000",
    .monitor = true,
    .broadcast = true,
    .tone_hz = 1000.0,
  });

  if (!engine.start()) {
    std::fprintf(stderr, "engine start failed\n");
    return 1;
  }

  mixbridge::MixBuses buses;
  engine.render_offline(480, buses);
  std::printf("mixbridge_audio_engine offline_ok=1 monitor_samples=%zu broadcast_samples=%zu frames=%llu\n",
              buses.monitor.size(),
              buses.broadcast.size(),
              static_cast<unsigned long long>(engine.stats().frames_rendered.load()));
  engine.stop();
  return 0;
}
