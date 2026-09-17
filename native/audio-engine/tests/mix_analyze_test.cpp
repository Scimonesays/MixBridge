#include "mixbridge/engine.hpp"
#include "mixbridge/analyze.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int fail(const char* msg) {
  std::fprintf(stderr, "FAIL: %s\n", msg);
  return 1;
}

int main() {
  mixbridge::Engine engine;
  std::string err;
  if (!engine.init(err)) return fail(err.c_str());

  const auto a = engine.add_tone({.hz = 440.0f, .name = "A"}, err);
  const auto b = engine.add_tone({.hz = 1000.0f, .name = "B"}, err);
  if (!a || !b) return fail("add tone");

  std::vector<float> mon(48000 * 2), bc(48000 * 2);
  engine.render_offline(48000, mon.data(), bc.data());

  if (!mixbridge::has_frequency(bc.data(), 48000, 2, 48000, 440.0) ||
      !mixbridge::has_frequency(bc.data(), 48000, 2, 48000, 1000.0)) {
    return fail("both tones should be present");
  }

  engine.set_mute(a, true);
  engine.render_offline(48000, mon.data(), bc.data());
  if (mixbridge::has_frequency(bc.data(), 48000, 2, 48000, 440.0, 8.0)) return fail("440 should be muted");
  if (!mixbridge::has_frequency(bc.data(), 48000, 2, 48000, 1000.0)) return fail("1000 should remain");

  engine.set_mute(a, false);
  engine.set_mute(b, true);
  engine.render_offline(48000, mon.data(), bc.data());
  if (!mixbridge::has_frequency(bc.data(), 48000, 2, 48000, 440.0)) return fail("440 should remain");
  if (mixbridge::has_frequency(bc.data(), 48000, 2, 48000, 1000.0, 8.0)) return fail("1000 should be muted");

  engine.set_mute(b, false);
  engine.set_gain(a, 0.5f);  // -6 dB-ish
  std::vector<float> loud(48000 * 2), quiet(48000 * 2);
  engine.set_gain(a, 1.0f);
  engine.set_mute(b, true);
  std::vector<float> loud_mon(48000 * 2);
  engine.render_offline(48000, loud_mon.data(), loud.data());
  auto loud_stats = mixbridge::analyze_pcm(loud.data(), loud.size(), 2, 48000);
  engine.set_gain(a, 0.5f);
  std::vector<float> quiet_mon(48000 * 2);
  engine.render_offline(48000, quiet_mon.data(), quiet.data());
  auto quiet_stats = mixbridge::analyze_pcm(quiet.data(), quiet.size(), 2, 48000);
  if (quiet_stats.rms <= 0.0 || loud_stats.rms <= 0.0) return fail("gain rms");
  const double ratio = quiet_stats.rms / loud_stats.rms;
  if (ratio < 0.35 || ratio > 0.65) {
    std::fprintf(stderr, "gain ratio=%.3f\n", ratio);
    return fail("gain -6dB not within tolerance");
  }

  // Limiter: absurd master gain should still bound peak roughly.
  engine.set_mute(b, false);
  engine.set_gain(a, 4.0f);
  engine.set_gain(b, 4.0f);
  engine.set_master_gain(4.0f);
  std::vector<float> lim_mon(48000 * 2), lim_bc(48000 * 2);
  engine.render_offline(48000, lim_mon.data(), lim_bc.data());
  auto lim = mixbridge::analyze_pcm(lim_bc.data(), lim_bc.size(), 2, 48000);
  if (lim.peak > 1.05) return fail("limiter failed to bound peak");

  std::printf("PASS engine_mix_analyze_test\n");
  return 0;
}
