#include "mixbridge/engine.hpp"
#include "mixbridge/analyze.hpp"

#include <cstdio>
#include <string>
#include <vector>

int main() {
  mixbridge::Engine engine;
  std::string err;
  if (!engine.init(err)) {
    std::fprintf(stderr, "init failed: %s\n", err.c_str());
    return 1;
  }

  const auto a = engine.add_tone({.hz = 440.0f, .name = "fixture-440"}, err);
  const auto b = engine.add_tone({.hz = 1000.0f, .name = "fixture-1000"}, err);
  std::printf("sources a=%u b=%u\n", a, b);

  std::vector<float> mon(480 * 2), bc(480 * 2);
  engine.render_offline(480, mon.data(), bc.data());
  auto stats = mixbridge::analyze_pcm(bc.data(), bc.size(), 2, 48000);
  std::printf("mixbridge_audio_engine offline_ok=1 peak=%.4f rms=%.4f dominant=%.1f state=%s\n",
              stats.peak, stats.rms, stats.dominant_hz,
              mixbridge::engine_state_name(engine.state()));

  const auto caps = engine.list_capture_devices();
  const auto rends = engine.list_render_devices();
  std::printf("capture_devices=%zu render_devices=%zu\n", caps.size(), rends.size());
  engine.shutdown();
  return 0;
}
