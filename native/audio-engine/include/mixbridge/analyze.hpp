#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

namespace mixbridge {

struct AnalyzeResult {
  double peak = 0.0;
  double rms = 0.0;
  double dominant_hz = 0.0;
  bool near_silence = true;
};

inline AnalyzeResult analyze_pcm(const float* samples, size_t count, uint32_t channels, uint32_t rate) {
  AnalyzeResult r;
  if (!samples || count == 0 || channels == 0 || rate == 0) return r;
  double sum_sq = 0.0;
  double peak = 0.0;
  for (size_t i = 0; i < count; ++i) {
    const double a = std::fabs(samples[i]);
    peak = std::max(peak, a);
    sum_sq += static_cast<double>(samples[i]) * samples[i];
  }
  r.peak = peak;
  r.rms = std::sqrt(sum_sq / static_cast<double>(count));
  r.near_silence = r.rms < 1e-4;

  const size_t frames = count / channels;
  const size_t n = std::min(frames, static_cast<size_t>(rate));
  if (n < 64) return r;

  auto goertzel = [&](double freq) {
    const double w = 2.0 * std::numbers::pi * freq / static_cast<double>(rate);
    const double coeff = 2.0 * std::cos(w);
    double s0 = 0, s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; ++i) {
      double x = samples[i * channels];
      if (channels > 1) x = 0.5 * (samples[i * channels] + samples[i * channels + 1]);
      s0 = x + coeff * s1 - s2;
      s2 = s1;
      s1 = s0;
    }
    return s1 * s1 + s2 * s2 - coeff * s1 * s2;
  };

  double best_p = 0.0;
  double best_f = 0.0;
  for (int f = 50; f <= 4000; f += 5) {
    const double p = goertzel(static_cast<double>(f));
    if (p > best_p) {
      best_p = p;
      best_f = f;
    }
  }
  r.dominant_hz = best_f;
  return r;
}

inline double goertzel_power(const float* samples, size_t frames, uint32_t channels, uint32_t rate, double freq) {
  if (!samples || frames < 64 || channels == 0) return 0.0;
  const size_t n = std::min(frames, static_cast<size_t>(rate));
  const double w = 2.0 * std::numbers::pi * freq / static_cast<double>(rate);
  const double coeff = 2.0 * std::cos(w);
  double s0 = 0, s1 = 0, s2 = 0;
  for (size_t i = 0; i < n; ++i) {
    double x = samples[i * channels];
    if (channels > 1) x = 0.5 * (samples[i * channels] + samples[i * channels + 1]);
    s0 = x + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

inline bool has_frequency(const float* samples, size_t frames, uint32_t channels, uint32_t rate,
                          double freq, double relative_to_noise = 20.0) {
  const double signal = goertzel_power(samples, frames, channels, rate, freq);
  const double noise = goertzel_power(samples, frames, channels, rate, freq + 137.0);
  if (signal <= 0.0) return false;
  return signal > noise * relative_to_noise && signal > 1e-6;
}

}  // namespace mixbridge
