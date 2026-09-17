#include "mixbridge/engine.hpp"
#include "mixbridge/ring_buffer.hpp"
#include "mixbridge/analyze.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int fail(const char* msg) {
  std::fprintf(stderr, "FAIL: %s\n", msg);
  return 1;
}

int main() {
  mixbridge::SpscFloatRing ring(1024);
  std::vector<float> in(100, 0.5f);
  std::vector<float> out(100, 0.0f);
  if (ring.write(in.data(), in.size()) != in.size()) return fail("ring write");
  if (ring.read(out.data(), out.size()) != out.size()) return fail("ring read");
  if (std::fabs(out[0] - 0.5f) > 1e-6f) return fail("ring value");

  mixbridge::Engine engine;
  std::string err;
  if (!engine.init(err)) return fail(err.c_str());
  const auto a = engine.add_tone({.hz = 440.0f, .name = "A"}, err);
  const auto b = engine.add_tone({.hz = 1000.0f, .name = "B"}, err);
  if (!a || !b) return fail("add");

  std::vector<float> mon(48000 * 2), bc(48000 * 2);
  engine.render_offline(48000, mon.data(), bc.data());
  auto stats = mixbridge::analyze_pcm(bc.data(), bc.size(), 2, 48000);
  if (stats.near_silence) return fail("broadcast silence");

  engine.set_mute(a, true);
  engine.set_mute(b, true);
  engine.render_offline(1024, mon.data(), bc.data());
  double sum = 0.0;
  for (size_t i = 0; i < 1024 * 2; ++i) sum += static_cast<double>(bc[i]) * bc[i];
  if (sum > 1e-8) return fail("muted not silent");

  std::printf("PASS engine_graph_test rms=%.4f\n", stats.rms);
  return 0;
}
