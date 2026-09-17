#include "wasapi_common.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void generate_tone(std::vector<float>& out, UINT32 rate, UINT32 channels, double seconds, double freq, float amp) {
  const size_t frames = static_cast<size_t>(seconds * rate);
  out.resize(frames * channels);
  double phase = 0.0;
  const double inc = 2.0 * 3.14159265358979323846 * freq / rate;
  for (size_t i = 0; i < frames; ++i) {
    const float s = static_cast<float>(std::sin(phase) * amp);
    phase += inc;
    for (UINT32 c = 0; c < channels; ++c) out[i * channels + c] = s;
  }
}

void generate_silence(std::vector<float>& out, UINT32 rate, UINT32 channels, double seconds) {
  out.assign(static_cast<size_t>(seconds * rate) * channels, 0.0f);
}

void generate_impulse(std::vector<float>& out, UINT32 rate, UINT32 channels) {
  out.assign(static_cast<size_t>(rate / 10) * channels, 0.0f);  // 100ms
  if (!out.empty()) {
    for (UINT32 c = 0; c < channels; ++c) out[c] = 1.0f;
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::string expect = "tone";
  double freq = 440.0;
  UINT32 rate = 48000;
  UINT32 channels = 2;
  double seconds = 0.5;
  double tol_db = -40.0;
  std::string generate;
  std::wstring wav_in;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](double& dst) {
      if (i + 1 < argc) dst = std::atof(argv[++i]);
    };
    auto needu = [&](UINT32& dst) {
      if (i + 1 < argc) dst = static_cast<UINT32>(std::atoi(argv[++i]));
    };
    auto needs = [&](std::string& dst) {
      if (i + 1 < argc) dst = argv[++i];
    };
    if (a == "--expect") needs(expect);
    else if (a == "--freq") need(freq);
    else if (a == "--rate") needu(rate);
    else if (a == "--channels") needu(channels);
    else if (a == "--seconds") need(seconds);
    else if (a == "--tol-db") need(tol_db);
    else if (a == "--generate") needs(generate);
    else if (a == "--wav") {
      if (i + 1 < argc) wav_in = utf8_to_wide(argv[++i]);
    }
  }

  std::vector<float> pcm;
  if (generate == "tone") generate_tone(pcm, rate, channels, seconds, freq, 0.5f);
  else if (generate == "silence") generate_silence(pcm, rate, channels, seconds);
  else if (generate == "impulse") generate_impulse(pcm, rate, channels);
  else if (!wav_in.empty()) {
    std::fprintf(stderr, "wav input decode not implemented in v0 analyzer; use --generate\n");
    return 2;
  } else {
    std::fprintf(stderr, "usage: mb-pcm-analyze --expect tone|silence|impulse --generate ...\n");
    return 2;
  }

  auto stats = analyze_pcm_f32(pcm.data(), pcm.size(), channels, rate);
  std::printf("peak=%.6f rms=%.6f dominant_hz=%.2f near_silence=%d\n",
              stats.peak, stats.rms, stats.dominant_hz, stats.near_silence ? 1 : 0);

  if (expect == "silence") {
    if (!stats.near_silence) {
      std::fprintf(stderr, "FAIL: expected silence\n");
      return 1;
    }
    std::printf("PASS silence\n");
    return 0;
  }

  if (expect == "impulse") {
    if (stats.peak < 0.9) {
      std::fprintf(stderr, "FAIL: impulse peak too low\n");
      return 1;
    }
    std::printf("PASS impulse\n");
    return 0;
  }

  if (expect == "tone") {
    const double err = std::fabs(stats.dominant_hz - freq);
    if (stats.near_silence || err > 20.0) {
      std::fprintf(stderr, "FAIL: expected ~%.1f Hz got %.1f Hz\n", freq, stats.dominant_hz);
      return 1;
    }
    // Amplitude check relative to expected 0.5 amp -> rms ~0.35
    const double db = 20.0 * std::log10(std::max(stats.rms, 1e-12));
    if (db < tol_db) {
      std::fprintf(stderr, "FAIL: rms too low (%.1f dBFS)\n", db);
      return 1;
    }
    std::printf("PASS tone freq=%.1f err=%.1f\n", freq, err);
    return 0;
  }

  std::fprintf(stderr, "unknown expect=%s\n", expect.c_str());
  return 2;
}
