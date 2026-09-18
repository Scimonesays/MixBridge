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

class HalfEffect final : public mixbridge::RealtimeEffect {
public:
  bool process(float* audio, uint32_t frames) noexcept override {
    for (uint32_t i = 0; i < frames * mixbridge::kEngineChannels; ++i) audio[i] *= 0.5f;
    return true;
  }
};

static double rms(const float* audio, size_t samples) {
  double sum = 0.0;
  for (size_t i = 0; i < samples; ++i) sum += static_cast<double>(audio[i]) * audio[i];
  return samples ? std::sqrt(sum / static_cast<double>(samples)) : 0.0;
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

  // Prove the plugin-agnostic realtime hook runs before metering/routing.
  mixbridge::Engine fx_engine;
  if (!fx_engine.init(err)) return fail(err.c_str());
  const auto fx_source = fx_engine.add_tone({.hz = 440.0f, .name = "fx"}, err);
  if (!fx_source) return fail("add fx source");
  std::vector<float> dry(4096 * 2), wet(4096 * 2), scratch(4096 * 2);
  fx_engine.render_offline(4096, dry.data(), scratch.data());
  HalfEffect half;
  auto* fx_slot = fx_engine.slot_by_id(fx_source);
  if (!fx_slot) return fail("fx slot");
  fx_slot->effect.store(&half, std::memory_order_release);
  fx_engine.render_offline(4096, wet.data(), scratch.data());
  const double ratio = rms(wet.data(), wet.size()) / std::max(1e-12, rms(dry.data(), dry.size()));
  if (ratio < 0.46 || ratio > 0.54) return fail("effect hook gain ratio");

  std::printf("PASS engine_graph_test rms=%.4f fx_ratio=%.3f\n", stats.rms, ratio);
  return 0;
}
