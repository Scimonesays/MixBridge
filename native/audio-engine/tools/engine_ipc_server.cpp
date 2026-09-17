#include "mixbridge/engine.hpp"

#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

// Line-oriented named-pipe IPC — mixbridge-ipc/1
// Pipe: \\.\pipe\mixbridge-engine

static std::string wide_to_utf8(const std::wstring& ws) {
  if (ws.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
  if (n > 1) WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), n, nullptr, nullptr);
  return out;
}

static std::wstring utf8_to_wide(const std::string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<size_t>(n > 0 ? n - 1 : 0), L'\0');
  if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), n);
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
    if (out.size() > 8192) return false;
  }
  return false;
}

static const char* source_kind_name(mixbridge::SourceKind k) {
  switch (k) {
    case mixbridge::SourceKind::PhysicalCapture: return "physical";
    case mixbridge::SourceKind::SystemLoopback: return "system";
    case mixbridge::SourceKind::ProcessLoopback: return "process";
    case mixbridge::SourceKind::ToneFixture: return "tone";
  }
  return "unknown";
}

static void list_processes(HANDLE pipe) {
  // Visible-window process list for application capture picker (no PIDs in UI).
  std::unordered_set<DWORD> pids;
  EnumWindows(
    [](HWND hwnd, LPARAM lp) -> BOOL {
      if (!IsWindowVisible(hwnd)) return TRUE;
      if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
      wchar_t title[4];
      if (GetWindowTextW(hwnd, title, 4) <= 0) return TRUE;
      DWORD pid = 0;
      GetWindowThreadProcessId(hwnd, &pid);
      if (pid) static_cast<std::unordered_set<DWORD>*>(reinterpret_cast<void*>(lp))->insert(pid);
      return TRUE;
    },
    reinterpret_cast<LPARAM>(&pids));

  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) {
    write_line(pipe, "ERR process_snapshot");
    return;
  }

  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  std::vector<std::pair<DWORD, std::string>> rows;
  if (Process32FirstW(snap, &pe)) {
    do {
      if (!pids.count(pe.th32ProcessID)) continue;
      if (pe.th32ProcessID == GetCurrentProcessId()) continue;
      std::string name = wide_to_utf8(pe.szExeFile);
      if (name.empty()) continue;
      // Skip obvious system shells
      if (_stricmp(name.c_str(), "explorer.exe") == 0) continue;
      if (_stricmp(name.c_str(), "ApplicationFrameHost.exe") == 0) continue;
      if (_stricmp(name.c_str(), "SearchHost.exe") == 0) continue;
      if (_stricmp(name.c_str(), "ShellExperienceHost.exe") == 0) continue;
      if (_stricmp(name.c_str(), "TextInputHost.exe") == 0) continue;
      rows.emplace_back(pe.th32ProcessID, std::move(name));
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);

  write_line(pipe, "OK COUNT " + std::to_string(rows.size()));
  for (const auto& row : rows) {
    write_line(pipe, "PROCESS PID " + std::to_string(row.first) + " NAME " + row.second);
  }
  write_line(pipe, "OK END");
}

static void handle_client(mixbridge::Engine& engine, HANDLE pipe) {
  std::string err;
  write_line(pipe, "OK HELLO mixbridge-ipc/1");
  while (true) {
    std::string line;
    if (!read_line(pipe, line)) break;
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    if (cmd == "PING") {
      write_line(pipe, "OK PONG");
    } else if (cmd == "STATUS") {
      const auto d = engine.diagnostics();
      char buf[420];
      std::snprintf(
        buf, sizeof(buf),
        "OK STATE %s BROADCAST %s LIVE_DEST %d FRAMES %llu XRUNS %llu UNDERRUNS %llu OVERRUNS %llu LAT_MS %.2f",
        mixbridge::engine_state_name(d.state), mixbridge::broadcast_state_name(d.broadcast),
        d.live_destination_ready ? 1 : 0, static_cast<unsigned long long>(d.frames_rendered),
        static_cast<unsigned long long>(d.xruns), static_cast<unsigned long long>(d.underruns),
        static_cast<unsigned long long>(d.overruns), d.estimated_latency_ms);
      write_line(pipe, buf);
    } else if (cmd == "LIST_CAPTURE") {
      auto devices = engine.list_capture_devices();
      write_line(pipe, "OK COUNT " + std::to_string(devices.size()));
      for (const auto& d : devices) {
        write_line(pipe, "DEVICE ID " + wide_to_utf8(d.id) + " NAME " + wide_to_utf8(d.name));
      }
      write_line(pipe, "OK END");
    } else if (cmd == "LIST_RENDER") {
      auto devices = engine.list_render_devices();
      write_line(pipe, "OK COUNT " + std::to_string(devices.size()));
      for (const auto& d : devices) {
        write_line(pipe, "DEVICE ID " + wide_to_utf8(d.id) + " NAME " + wide_to_utf8(d.name));
      }
      write_line(pipe, "OK END");
    } else if (cmd == "LIST_PROCESSES") {
      list_processes(pipe);
    } else if (cmd == "LIST_SOURCES") {
      auto sources = engine.list_sources();
      write_line(pipe, "OK COUNT " + std::to_string(sources.size()));
      for (const auto& s : sources) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
                      "SOURCE ID %u KIND %s NAME %s GAIN %.4f MUTE %d MONITOR %d BROADCAST %d", s.id,
                      source_kind_name(s.kind), s.name.c_str(), s.gain, s.mute ? 1 : 0,
                      s.monitor ? 1 : 0, s.broadcast ? 1 : 0);
        write_line(pipe, buf);
      }
      write_line(pipe, "OK END");
    } else if (cmd == "START") {
      if (engine.start(err)) write_line(pipe, "OK STARTED");
      else write_line(pipe, "ERR " + err);
    } else if (cmd == "STOP") {
      engine.stop();
      write_line(pipe, "OK STOPPED");
    } else if (cmd == "RESTART") {
      engine.stop();
      if (engine.start(err)) write_line(pipe, "OK RESTARTED");
      else write_line(pipe, "ERR " + err);
    } else if (cmd == "BROADCAST_ENABLE") {
      if (engine.enable_broadcast(err)) write_line(pipe, "OK LIVE");
      else write_line(pipe, "ERR " + err);
    } else if (cmd == "BROADCAST_DISABLE") {
      engine.disable_broadcast();
      write_line(pipe, "OK STANDBY");
    } else if (cmd == "SET_LIVE_DEVICE") {
      std::string id_utf8;
      std::getline(iss >> std::ws, id_utf8);
      while (!id_utf8.empty() && (id_utf8.back() == ' ' || id_utf8.back() == '\t')) id_utf8.pop_back();
      if (engine.set_live_device(utf8_to_wide(id_utf8), err)) write_line(pipe, "OK LIVE_DEST");
      else write_line(pipe, "ERR " + err);
    } else if (cmd == "ADD_TONE") {
      float hz = 440.0f;
      iss >> hz;
      const auto id = engine.add_tone({.hz = hz, .name = "ipc-tone"}, err);
      if (id) write_line(pipe, "OK ID " + std::to_string(id));
      else write_line(pipe, "ERR " + err);
    } else if (cmd == "ADD_PHYSICAL") {
      std::string id_utf8;
      std::getline(iss >> std::ws, id_utf8);
      // Trim trailing spaces
      while (!id_utf8.empty() && (id_utf8.back() == ' ' || id_utf8.back() == '\t')) id_utf8.pop_back();
      mixbridge::AddPhysicalRequest req;
      req.device_id = utf8_to_wide(id_utf8);
      const auto id = engine.add_physical_capture(req, err);
      if (id) write_line(pipe, "OK ID " + std::to_string(id));
      else write_line(pipe, "ERR " + err);
    } else if (cmd == "ADD_PROCESS") {
      uint32_t pid = 0;
      iss >> pid;
      std::string name;
      std::getline(iss >> std::ws, name);
      mixbridge::AddProcessRequest req{.pid = pid, .name = name};
      const auto id = engine.add_process_loopback(req, err);
      if (id) write_line(pipe, "OK ID " + std::to_string(id));
      else write_line(pipe, "ERR " + err);
    } else if (cmd == "REMOVE") {
      uint32_t id = 0;
      iss >> id;
      if (engine.remove_source(id, err)) write_line(pipe, "OK REMOVED");
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
    } else if (cmd == "SET_MONITOR") {
      uint32_t id = 0;
      int m = 0;
      iss >> id >> m;
      write_line(pipe, engine.set_monitor(id, m != 0) ? "OK" : "ERR");
    } else if (cmd == "SET_BROADCAST") {
      uint32_t id = 0;
      int m = 0;
      iss >> id >> m;
      write_line(pipe, engine.set_broadcast(id, m != 0) ? "OK" : "ERR");
    } else if (cmd == "METER_MASTER") {
      auto m = engine.master_meter();
      char buf[128];
      std::snprintf(buf, sizeof(buf), "OK PEAK %.6f RMS %.6f CLIP %d", m.peak, m.rms, m.clip ? 1 : 0);
      write_line(pipe, buf);
    } else if (cmd == "METER_BROADCAST") {
      auto m = engine.broadcast_meter();
      char buf[128];
      std::snprintf(buf, sizeof(buf), "OK PEAK %.6f RMS %.6f CLIP %d", m.peak, m.rms, m.clip ? 1 : 0);
      write_line(pipe, buf);
    } else if (cmd == "METER_SOURCE") {
      uint32_t id = 0;
      iss >> id;
      auto m = engine.source_meter(id);
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
      ExitProcess(0);
    } else {
      write_line(pipe, "ERR UNKNOWN_CMD");
    }
  }
  FlushFileBuffers(pipe);
  DisconnectNamedPipe(pipe);
  CloseHandle(pipe);
}

int main() {
  mixbridge::Engine engine;
  std::string err;
  if (!engine.init(err)) {
    std::fprintf(stderr, "init failed: %s\n", err.c_str());
    return 1;
  }

  // Developer-only live sink flag — never a production destination.
  wchar_t env[8]{};
  if (GetEnvironmentVariableW(L"MIXBRIDGE_DEV_LIVE_SINK", env, 8) > 0 && env[0] == L'1') {
    engine.set_live_destination_ready(true);
  }

  const wchar_t* pipe_name = L"\\\\.\\pipe\\mixbridge-engine";
  std::printf("mb-engine-ipc listening on \\\\.\\pipe\\mixbridge-engine\n");
  std::fflush(stdout);

  for (;;) {
    HANDLE pipe = CreateNamedPipeW(
      pipe_name, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES, 8192, 8192, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
      std::fprintf(stderr, "CreateNamedPipe failed (%lu)\n", GetLastError());
      Sleep(200);
      continue;
    }
    const BOOL connected =
      ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
    if (!connected) {
      CloseHandle(pipe);
      continue;
    }
    handle_client(engine, pipe);
  }
}
