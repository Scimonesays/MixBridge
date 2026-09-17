#include "mixbridge/engine.hpp"

#include <windows.h>

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

// Minimal line-oriented named-pipe IPC for Phase 3K.
// Pipe: \\.\pipe\mixbridge-engine
// Commands (UTF-8 lines):
//   PING
//   STATUS
//   LIST_CAPTURE
//   LIST_RENDER
//   START
//   STOP
//   ADD_TONE <hz>
//   SET_GAIN <id> <gain>
//   SET_MUTE <id> <0|1>
//   METER_MASTER
//   SHUTDOWN

static std::string wide_to_utf8(const std::wstring& ws) {
  if (ws.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
  if (n > 1) WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), n, nullptr, nullptr);
  return out;
}

static bool write_line(HANDLE pipe, const std::string& s) {
  std::string line = s;
  line.push_back('\n');
  DWORD written = 0;
  return WriteFile(pipe, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) != 0;
}

static bool read_line(HANDLE pipe, std::string& out) {
  out.clear();
  char c = 0;
  DWORD read = 0;
  while (ReadFile(pipe, &c, 1, &read, nullptr) && read == 1) {
    if (c == '\n') return true;
    if (c != '\r') out.push_back(c);
    if (out.size() > 4096) return false;
  }
  return false;
}

int main() {
  mixbridge::Engine engine;
  std::string err;
  if (!engine.init(err)) {
    std::fprintf(stderr, "init failed: %s\n", err.c_str());
    return 1;
  }

  const wchar_t* pipe_name = L"\\\\.\\pipe\\mixbridge-engine";
  std::printf("mb-engine-ipc listening on \\\\.\\pipe\\mixbridge-engine\n");
  std::fflush(stdout);

  for (;;) {
    HANDLE pipe = CreateNamedPipeW(
      pipe_name,
      PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
      1,
      8192,
      8192,
      0,
      nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
      std::fprintf(stderr, "CreateNamedPipe failed\n");
      return 2;
    }
    const BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
    if (!connected) {
      CloseHandle(pipe);
      continue;
    }

    write_line(pipe, "OK HELLO mixbridge-ipc/1");
    bool run = true;
    while (run) {
      std::string line;
      if (!read_line(pipe, line)) break;
      std::istringstream iss(line);
      std::string cmd;
      iss >> cmd;
      if (cmd == "PING") {
        write_line(pipe, "OK PONG");
      } else if (cmd == "STATUS") {
        const auto d = engine.diagnostics();
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "OK STATE %s FRAMES %llu XRUNS %llu LAT_MS %.2f",
                      mixbridge::engine_state_name(d.state),
                      static_cast<unsigned long long>(d.frames_rendered),
                      static_cast<unsigned long long>(d.xruns),
                      d.estimated_latency_ms);
        write_line(pipe, buf);
      } else if (cmd == "LIST_CAPTURE") {
        auto devices = engine.list_capture_devices();
        write_line(pipe, "OK COUNT " + std::to_string(devices.size()));
        for (const auto& d : devices) {
          write_line(pipe, "DEVICE " + wide_to_utf8(d.name));
        }
        write_line(pipe, "OK END");
      } else if (cmd == "LIST_RENDER") {
        auto devices = engine.list_render_devices();
        write_line(pipe, "OK COUNT " + std::to_string(devices.size()));
        for (const auto& d : devices) {
          write_line(pipe, "DEVICE " + wide_to_utf8(d.name));
        }
        write_line(pipe, "OK END");
      } else if (cmd == "START") {
        if (engine.start(err)) write_line(pipe, "OK STARTED");
        else write_line(pipe, "ERR " + err);
      } else if (cmd == "STOP") {
        engine.stop();
        write_line(pipe, "OK STOPPED");
      } else if (cmd == "ADD_TONE") {
        float hz = 440.0f;
        iss >> hz;
        const auto id = engine.add_tone({.hz = hz, .name = "ipc-tone"}, err);
        if (id) write_line(pipe, "OK ID " + std::to_string(id));
        else write_line(pipe, "ERR " + err);
      } else if (cmd == "SET_GAIN") {
        uint32_t id = 0;
        float g = 1.0f;
        iss >> id >> g;
        write_line(pipe, engine.set_gain(id, g) ? "OK" : "ERR");
      } else if (cmd == "SET_MUTE") {
        uint32_t id = 0;
        int m = 0;
        iss >> id >> m;
        write_line(pipe, engine.set_mute(id, m != 0) ? "OK" : "ERR");
      } else if (cmd == "METER_MASTER") {
        auto m = engine.master_meter();
        char buf[128];
        std::snprintf(buf, sizeof(buf), "OK PEAK %.6f RMS %.6f CLIP %d", m.peak, m.rms, m.clip ? 1 : 0);
        write_line(pipe, buf);
      } else if (cmd == "SHUTDOWN") {
        write_line(pipe, "OK BYE");
        engine.stop();
        engine.shutdown();
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return 0;
      } else {
        write_line(pipe, "ERR UNKNOWN_CMD");
      }
    }

    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
  }
}
