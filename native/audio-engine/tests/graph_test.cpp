#include "mixbridge/engine.hpp"
#include "mixbridge/ring_buffer.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
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
  const auto a = engine.graph().add_source(mixbridge::SourceNode{
    .kind = mixbridge::SourceKind::ToneFixture,
    .name = "A",
    .tone_hz = 440.0,
  });
  const auto b = engine.graph().add_source(mixbridge::SourceNode{
    .kind = mixbridge::SourceKind::ToneFixture,
    .name = "B",
    .tone_hz = 1000.0,
  });
  (void)a;
  (void)b;

  mixbridge::MixBuses buses;
  engine.render_offline(48000, buses);  // 1 second
  if (buses.broadcast.size() != 48000 * 2) return fail("broadcast size");

  // Energy should exist on both buses.
  double sum = 0.0;
  for (float v : buses.broadcast) sum += static_cast<double>(v) * v;
  const double rms = std::sqrt(sum / buses.broadcast.size());
  if (rms < 0.01) return fail("broadcast near silence");

  // Mute all -> silence
  if (auto* sa = engine.graph().find(a)) sa->mute = true;
  if (auto* sb = engine.graph().find(b)) sb->mute = true;
  engine.render_offline(1024, buses);
  double sum2 = 0.0;
  for (float v : buses.broadcast) sum2 += static_cast<double>(v) * v;
  if (sum2 > 1e-8) return fail("muted not silent");

  std::printf("PASS engine_graph_test rms=%.4f\n", rms);
  return 0;
}
