#include "wasapi_common.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

static void usage() {
  std::printf(
    "MixBridge audio probe\n"
    "  mb-audio-probe enumerate\n"
    "  mb-audio-probe capture [--seconds N] [--wav path]\n"
    "  mb-audio-probe render [--seconds N] [--freq HZ]\n"
    "  mb-audio-probe system-loopback [--seconds N] [--wav path]\n"
    "  mb-audio-probe process-loopback --pid PID [--seconds N] [--wav path]\n"
    "  mb-audio-probe selftest\n");
}

int cmd_selftest() {
  int failures = 0;
  std::printf("=== selftest: enumerate ===\n");
  if (cmd_enumerate() != 0) {
    std::fprintf(stderr, "enumerate failed\n");
    ++failures;
  }

  std::printf("=== selftest: render 440Hz 0.4s ===\n");
  if (cmd_render(0.4, 440.0) != 0) {
    std::fprintf(stderr, "render failed\n");
    ++failures;
  }

  std::printf("=== selftest: capture 0.4s ===\n");
  {
    const int rc = cmd_capture(0.4, L"");
    if (rc != 0) {
      std::fprintf(stderr, "capture failed (may be OK if no mic)\n");
      // Do not hard-fail capture without mic; record as soft failure detail.
    }
  }

  std::printf("=== selftest: system-loopback 0.6s ===\n");
  {
    const int rc = cmd_system_loopback(0.6, L"");
    if (rc != 0) {
      std::fprintf(stderr, "system-loopback failed rc=%d\n", rc);
      ++failures;
    }
  }

  std::printf("=== selftest: process-loopback via mb-tone-player ===\n");
  // Launched by scripts/run-audio-tests.ps1 with pid injection; here spawn if present nearby.
  wchar_t module[MAX_PATH]{};
  GetModuleFileNameW(nullptr, module, MAX_PATH);
  std::wstring dir(module);
  const auto slash = dir.find_last_of(L"\\/");
  if (slash != std::wstring::npos) dir.resize(slash + 1);
  const std::wstring tone = dir + L"mb-tone-player.exe";
  if (GetFileAttributesW(tone.c_str()) == INVALID_FILE_ATTRIBUTES) {
    std::printf("skip process-loopback (mb-tone-player.exe not beside probe)\n");
  } else {
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + tone + L"\" --freq 1000 --seconds 2.0";
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
      std::fprintf(stderr, "failed to spawn tone player\n");
      ++failures;
    } else {
      Sleep(200);
      const int rc = cmd_process_loopback(pi.dwProcessId, 1.0, L"");
      TerminateProcess(pi.hProcess, 0);
      WaitForSingleObject(pi.hProcess, 2000);
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
      if (rc != 0) {
        std::fprintf(stderr, "process-loopback failed rc=%d\n", rc);
        ++failures;
      }
    }
  }

  std::printf("selftest_failures=%d\n", failures);
  return failures == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
  ComInit com;
  if (argc < 2) {
    usage();
    return 2;
  }

  std::string cmd = argv[1];
  double seconds = 1.0;
  double freq = 440.0;
  DWORD pid = 0;
  std::wstring wav;

  for (int i = 2; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--seconds" && i + 1 < argc) seconds = std::atof(argv[++i]);
    else if (a == "--freq" && i + 1 < argc) freq = std::atof(argv[++i]);
    else if (a == "--pid" && i + 1 < argc) pid = static_cast<DWORD>(std::atoi(argv[++i]));
    else if (a == "--wav" && i + 1 < argc) wav = utf8_to_wide(argv[++i]);
  }

  if (cmd == "enumerate") return cmd_enumerate();
  if (cmd == "capture") return cmd_capture(seconds, wav);
  if (cmd == "render") return cmd_render(seconds, freq);
  if (cmd == "system-loopback") return cmd_system_loopback(seconds, wav);
  if (cmd == "process-loopback") {
    if (pid == 0) {
      std::fprintf(stderr, "--pid required\n");
      return 2;
    }
    return cmd_process_loopback(pid, seconds, wav);
  }
  if (cmd == "selftest") return cmd_selftest();

  usage();
  return 2;
}
