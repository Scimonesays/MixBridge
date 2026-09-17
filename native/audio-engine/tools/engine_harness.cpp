#include "mixbridge/engine.hpp"
#include "mixbridge/analyze.hpp"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

static bool spawn_tone(const std::wstring& exe, double freq, double seconds, PROCESS_INFORMATION& pi) {
  std::wstring cmd = L"\"" + exe + L"\" --freq " + std::to_wstring(freq) + L" --seconds " +
                     std::to_wstring(seconds);
  STARTUPINFOW si{sizeof(si)};
  std::vector<wchar_t> buf(cmd.begin(), cmd.end());
  buf.push_back(L'\0');
  return CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi) != 0;
}

static std::wstring find_tone_player() {
  wchar_t module[MAX_PATH]{};
  GetModuleFileNameW(nullptr, module, MAX_PATH);
  std::wstring dir(module);
  const auto slash = dir.find_last_of(L"\\/");
  if (slash != std::wstring::npos) dir.resize(slash + 1);
  const std::wstring candidates[] = {
    dir + L"mb-tone-player.exe",
    dir + L"..\\..\\..\\probes\\build\\mb-tone-player.exe",
    L"C:\\Users\\scimo\\Documents\\CODE\\MixBridge\\native\\probes\\build\\mb-tone-player.exe",
  };
  for (const auto& c : candidates) {
    if (GetFileAttributesW(c.c_str()) != INVALID_FILE_ATTRIBUTES) return c;
  }
  return {};
}

int main(int argc, char** argv) {
  double seconds = 2.0;
  bool use_physical = true;
  bool use_process = true;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--seconds" && i + 1 < argc) seconds = std::atof(argv[++i]);
    else if (a == "--no-physical") use_physical = false;
    else if (a == "--no-process") use_process = false;
  }

  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);

  mixbridge::Engine engine;
  std::string err;
  if (!engine.init(err)) {
    std::fprintf(stderr, "init: %s\n", err.c_str());
    return 2;
  }

  // Prefer RME Speakers if present for monitor.
  for (const auto& d : engine.list_render_devices()) {
    const auto name = d.name;
    if (name.find(L"Fireface") != std::wstring::npos && name.find(L"Speakers") != std::wstring::npos) {
      engine.set_monitor_device(d.id, err);
      std::printf("monitor=Fireface Speakers\n");
      break;
    }
  }

  uint32_t physical_id = 0;
  if (use_physical) {
    mixbridge::AddPhysicalRequest req;
    // Prefer Fireface Analog 1+2 capture
    for (const auto& d : engine.list_capture_devices()) {
      if (d.name.find(L"Fireface") != std::wstring::npos && d.name.find(L"1+2") != std::wstring::npos) {
        req.device_id = d.id;
        break;
      }
    }
    physical_id = engine.add_physical_capture(req, err);
    if (!physical_id) {
      std::fprintf(stderr, "physical add failed: %s (continuing)\n", err.c_str());
    } else {
      std::printf("physical_source=%u\n", physical_id);
    }
  }

  PROCESS_INFORMATION pi{};
  uint32_t process_id = 0;
  std::wstring tone = find_tone_player();
  if (use_process) {
    if (tone.empty()) {
      std::fprintf(stderr, "mb-tone-player.exe not found\n");
      return 3;
    }
    if (!spawn_tone(tone, 1000.0, seconds + 2.0, pi)) {
      std::fprintf(stderr, "spawn tone failed\n");
      return 3;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    process_id = engine.add_process_loopback({.pid = pi.dwProcessId, .name = "tone-player"}, err);
    if (!process_id) {
      std::fprintf(stderr, "process add failed: %s\n", err.c_str());
      TerminateProcess(pi.hProcess, 1);
      return 4;
    }
    std::printf("process_source=%u pid=%lu\n", process_id, static_cast<unsigned long>(pi.dwProcessId));
  }

  // Also inject offline-audible 440 tone into mix for deterministic dual-tone when physical is quiet.
  const auto tone440 = engine.add_tone({.hz = 440.0f, .name = "ref-440"}, err);
  std::printf("ref_tone=%u\n", tone440);

  engine.enable_tap(true);
  if (!engine.start(err)) {
    std::fprintf(stderr, "start: %s\n", err.c_str());
    if (pi.hProcess) TerminateProcess(pi.hProcess, 1);
    return 5;
  }

  std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
  engine.stop();

  if (pi.hProcess) {
    TerminateProcess(pi.hProcess, 0);
    WaitForSingleObject(pi.hProcess, 2000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }

  std::vector<float> tap;
  engine.copy_tap(tap);
  const size_t frames = tap.size() / 2;
  auto stats = mixbridge::analyze_pcm(tap.data(), tap.size(), 2, 48000);
  const bool has440 = mixbridge::has_frequency(tap.data(), frames, 2, 48000, 440.0);
  const bool has1000 = mixbridge::has_frequency(tap.data(), frames, 2, 48000, 1000.0);
  const auto diag = engine.diagnostics();

  std::printf("tap_frames=%zu peak=%.4f rms=%.4f dominant=%.1f has440=%d has1000=%d\n",
              frames, stats.peak, stats.rms, stats.dominant_hz, has440 ? 1 : 0, has1000 ? 1 : 0);
  std::printf("state=%s frames_rendered=%llu xruns=%llu latency_ms=%.2f device_rate=%u buffer=%u\n",
              mixbridge::engine_state_name(diag.state),
              static_cast<unsigned long long>(diag.frames_rendered),
              static_cast<unsigned long long>(diag.xruns),
              diag.estimated_latency_ms, diag.device_rate, diag.buffer_frames);

  // Pass criteria: engine ran, tap non-silent, 440 present; 1000 present if process path used.
  int rc = 0;
  if (frames < 4800 || stats.near_silence) rc = 10;
  if (!has440) rc = 11;
  if (use_process && !has1000) rc = 12;
  std::printf("harness_result=%s\n", rc == 0 ? "PASS" : "FAIL");
  engine.shutdown();
  return rc;
}
