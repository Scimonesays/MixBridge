#include "mixbridge/engine.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <windows.h>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

int main(int argc, char** argv) {
  double minutes = 30.0;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--minutes" && i + 1 < argc) minutes = std::atof(argv[++i]);
  }

  mixbridge::Engine engine;
  std::string err;
  if (!engine.init(err)) {
    std::fprintf(stderr, "init: %s\n", err.c_str());
    return 2;
  }
  engine.add_tone({.hz = 440.0f, .name = "soak-a"}, err);
  engine.add_tone({.hz = 1000.0f, .name = "soak-b"}, err);
  if (!engine.start(err)) {
    std::fprintf(stderr, "start: %s\n", err.c_str());
    return 3;
  }

  PROCESS_MEMORY_COUNTERS_EX mem0{};
  GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem0), sizeof(mem0));
  const auto t0 = std::chrono::steady_clock::now();
  const auto deadline = t0 + std::chrono::duration<double>(minutes * 60.0);

  uint64_t last_frames = 0;
  size_t samples = 0;
  SIZE_T peak_working = mem0.WorkingSetSize;

  while (std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    const auto d = engine.diagnostics();
    auto meter = engine.master_meter();
    PROCESS_MEMORY_COUNTERS_EX mem{};
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem), sizeof(mem));
    if (mem.WorkingSetSize > peak_working) peak_working = mem.WorkingSetSize;

    if (d.state != mixbridge::EngineState::Running) {
      std::fprintf(stderr, "soak FAIL state=%s\n", mixbridge::engine_state_name(d.state));
      engine.stop();
      return 4;
    }
    if (d.frames_rendered <= last_frames) {
      std::fprintf(stderr, "soak FAIL frames stalled\n");
      engine.stop();
      return 5;
    }
    last_frames = d.frames_rendered;
    ++samples;
    std::printf("soak_tick=%zu frames=%llu xruns=%llu meter_peak=%.4f ws_mb=%.1f\n",
                samples,
                static_cast<unsigned long long>(d.frames_rendered),
                static_cast<unsigned long long>(d.xruns),
                meter.peak,
                mem.WorkingSetSize / (1024.0 * 1024.0));
    std::fflush(stdout);
  }

  engine.stop();
  PROCESS_MEMORY_COUNTERS_EX mem1{};
  GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem1), sizeof(mem1));
  const double growth_mb =
    (static_cast<double>(mem1.WorkingSetSize) - static_cast<double>(mem0.WorkingSetSize)) / (1024.0 * 1024.0);
  const auto d = engine.diagnostics();
  std::printf("soak_done minutes=%.2f xruns=%llu growth_mb=%.2f peak_ws_mb=%.1f\n",
              minutes,
              static_cast<unsigned long long>(d.xruns),
              growth_mb,
              peak_working / (1024.0 * 1024.0));

  // Allow modest working-set churn; fail on large continuous growth heuristic.
  if (growth_mb > 150.0) {
    std::fprintf(stderr, "soak FAIL memory growth\n");
    return 6;
  }
  std::printf("soak_result=PASS\n");
  return 0;
}
