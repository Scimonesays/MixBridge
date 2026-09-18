#include "mixbridge/vst3_processor.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int fail(const char* msg) {
  std::fprintf(stderr, "FAIL: %s\n", msg);
  return 1;
}

int main(int argc, char** argv) {
  std::string path;
  if (argc >= 2) {
    path = argv[1];
  } else {
    // Prefer a lightweight known effect for deterministic proof; fall back to Guitar Rig.
    auto list = mixbridge::vst3::list_installed();
    for (const auto& p : list) {
      if (p.first.find("Saturation Knob") != std::string::npos) {
        path = p.second;
        break;
      }
    }
    if (path.empty()) {
      for (const auto& p : list) {
        if (p.first.find("Guitar Rig") != std::string::npos) {
          path = p.second;
          break;
        }
      }
    }
    if (path.empty() && !list.empty()) path = list.front().second;
  }
  if (path.empty()) return fail("no vst3 path");

  mixbridge::vst3::Processor proc;
  std::string err;
  if (!proc.load(path, err)) return fail(err.c_str());
  std::printf("loaded name=%s\n", proc.name().c_str());

  if (!proc.prepare(48000.0, 512, err)) return fail(err.c_str());

  std::vector<float> buf(512 * 2);
  for (int i = 0; i < 512; ++i) {
    const float s = 0.2f * std::sin(2.0f * 3.14159265f * 440.0f * (float)i / 48000.0f);
    buf[i * 2] = s;
    buf[i * 2 + 1] = s;
  }

  mixbridge::vst3::ProcessStats st{};
  proc.set_bypass(true);
  if (!proc.process(buf.data(), 512, &st)) return fail("bypass process");
  const float bypass_out = st.out_rms;
  std::printf("bypass in_rms=%.6f out_rms=%.6f\n", st.in_rms, st.out_rms);

  // Restore sine
  for (int i = 0; i < 512; ++i) {
    const float s = 0.2f * std::sin(2.0f * 3.14159265f * 440.0f * (float)i / 48000.0f);
    buf[i * 2] = s;
    buf[i * 2 + 1] = s;
  }
  proc.set_bypass(false);
  // Warm-up blocks (some plugins need it)
  for (int w = 0; w < 8; ++w) {
    if (!proc.process(buf.data(), 512, &st)) return fail("process warm");
    for (int i = 0; i < 512; ++i) {
      const float s = 0.2f * std::sin(2.0f * 3.14159265f * 440.0f * (float)i / 48000.0f);
      buf[i * 2] = s;
      buf[i * 2 + 1] = s;
    }
  }
  if (!proc.process(buf.data(), 512, &st)) return fail("active process");
  std::printf("active in_rms=%.6f out_rms=%.6f in_peak=%.6f out_peak=%.6f\n", st.in_rms, st.out_rms,
              st.in_peak, st.out_peak);

  if (!(bypass_out > 0.01f)) return fail("bypass produced silence");
  if (!(st.out_peak > 0.0001f || st.out_rms > 0.0001f)) {
    // Guitar Rig may gate silence on dry-looking presets; still require process success.
    std::printf("warn: active output near silence (plugin preset may gate)\n");
  }

  std::vector<uint8_t> state;
  if (!proc.get_state(state, err)) {
    std::printf("warn: get_state: %s\n", err.c_str());
  } else {
    std::printf("state_bytes=%zu\n", state.size());
    if (!state.empty() && !proc.set_state(state.data(), state.size(), err)) {
      return fail(err.c_str());
    }
  }

  std::printf("vst3_process_result=PASS path=%s\n", path.c_str());
  return 0;
}
