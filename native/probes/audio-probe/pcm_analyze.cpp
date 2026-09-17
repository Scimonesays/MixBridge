#include "wasapi_common.hpp"

#include <algorithm>
#include <fstream>
#include <numbers>

AnalyzeStats analyze_pcm_f32(const float* samples, size_t count, UINT32 channels, UINT32 rate) {
  AnalyzeStats s{};
  if (!samples || count == 0 || channels == 0 || rate == 0) return s;

  double sum_sq = 0.0;
  double peak = 0.0;
  for (size_t i = 0; i < count; ++i) {
    const double v = std::fabs(static_cast<double>(samples[i]));
    peak = std::max(peak, v);
    sum_sq += static_cast<double>(samples[i]) * static_cast<double>(samples[i]);
  }
  s.peak = peak;
  s.rms = std::sqrt(sum_sq / static_cast<double>(count));
  s.near_silence = s.rms < 1e-4;

  // Goertzel scan for dominant frequency on channel 0 (mono mix of L).
  const size_t frames = count / channels;
  const size_t n = std::min(frames, static_cast<size_t>(rate));  // up to 1s
  if (n < 64) return s;

  auto goertzel = [&](double freq) {
    const double w = 2.0 * std::numbers::pi * freq / static_cast<double>(rate);
    const double coeff = 2.0 * std::cos(w);
    double s0 = 0.0, s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < n; ++i) {
      double x = samples[i * channels];
      if (channels > 1) x = 0.5 * (samples[i * channels] + samples[i * channels + 1]);
      s0 = x + coeff * s1 - s2;
      s2 = s1;
      s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return power;
  };

  double best_power = 0.0;
  double best_freq = 0.0;
  for (int freq = 50; freq <= 4000; freq += 5) {
    const double p = goertzel(static_cast<double>(freq));
    if (p > best_power) {
      best_power = p;
      best_freq = static_cast<double>(freq);
    }
  }
  s.dominant_hz = best_freq;
  return s;
}

bool write_wav_f32(const std::wstring& path, const float* samples, size_t frames, UINT32 channels, UINT32 rate) {
  if (!samples || frames == 0 || channels == 0) return false;
  // Write as 16-bit PCM WAV for easy inspection.
  std::ofstream out(path, std::ios::binary);
  if (!out) return false;
  const uint16_t audio_format = 1;
  const uint16_t bits = 16;
  const uint32_t byte_rate = rate * channels * (bits / 8);
  const uint16_t block_align = static_cast<uint16_t>(channels * (bits / 8));
  const uint32_t data_bytes = static_cast<uint32_t>(frames * block_align);
  const uint32_t riff_size = 36 + data_bytes;

  auto write_u32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
  auto write_u16 = [&](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };

  out.write("RIFF", 4);
  write_u32(riff_size);
  out.write("WAVE", 4);
  out.write("fmt ", 4);
  write_u32(16);
  write_u16(audio_format);
  write_u16(static_cast<uint16_t>(channels));
  write_u32(rate);
  write_u32(byte_rate);
  write_u16(block_align);
  write_u16(bits);
  out.write("data", 4);
  write_u32(data_bytes);

  for (size_t i = 0; i < frames * channels; ++i) {
    float x = std::clamp(samples[i], -1.0f, 1.0f);
    int16_t v = static_cast<int16_t>(x * 32767.0f);
    out.write(reinterpret_cast<const char*>(&v), 2);
  }
  return static_cast<bool>(out);
}
