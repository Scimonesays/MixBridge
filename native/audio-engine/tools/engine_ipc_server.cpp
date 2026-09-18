#include "mixbridge/engine.hpp"

#if defined(MIXBRIDGE_WITH_VST3)
#include "mixbridge/vst3_processor.hpp"
#endif

#include <windows.h>
#include <tlhelp32.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
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

#if defined(MIXBRIDGE_WITH_VST3)
struct FxChain {
  std::vector<std::unique_ptr<mixbridge::vst3::Processor>> plugs;
  std::atomic<bool> faulted{false};
};

using FxMap = std::unordered_map<uint32_t, std::unique_ptr<FxChain>>;

static int process_one_seh(mixbridge::vst3::Processor* p, float* interleaved, int32_t frames) {
  if (!p) return 0;
  __try {
    return p->process(interleaved, frames, nullptr) ? 1 : 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
}

static void fx_chain_thunk(void* ctx, float* interleaved, uint32_t frames) {
  auto* chain = static_cast<FxChain*>(ctx);
  if (!chain || chain->faulted.load(std::memory_order_relaxed)) return;
  for (auto& plug : chain->plugs) {
    const int rc = process_one_seh(plug.get(), interleaved, static_cast<int32_t>(frames));
    if (rc < 0) {
      chain->faulted.store(true, std::memory_order_release);
      return;
    }
  }
}

static std::string chain_display_name(const FxChain& chain) {
  if (chain.plugs.empty()) return {};
  std::string n = chain.plugs.front()->name();
  for (size_t i = 1; i < chain.plugs.size(); ++i) {
    n += " > ";
    n += chain.plugs[i]->name();
  }
  return n;
}

static void clear_fx(mixbridge::Engine& engine, FxMap& fx, uint32_t id) {
  engine.set_source_fx_hook(id, nullptr, nullptr, "");
  fx.erase(id);
}

static void rebind_fx(mixbridge::Engine& engine, FxMap& fx, uint32_t id) {
  auto it = fx.find(id);
  if (it == fx.end() || !it->second || it->second->plugs.empty()) {
    clear_fx(engine, fx, id);
    return;
  }
  it->second->faulted.store(false, std::memory_order_release);
  engine.set_source_fx_hook(id, &fx_chain_thunk, it->second.get(), chain_display_name(*it->second));
  engine.set_source_fx_bypass(id, false);
}

static std::filesystem::path mixbridge_config_dir() {
  const char* la = std::getenv("LOCALAPPDATA");
  std::filesystem::path p = la ? std::filesystem::path(la) : std::filesystem::path(".");
  return p / "MixBridge";
}

static void load_quarantine(std::unordered_set<std::string>& quarantine) {
  const auto path = mixbridge_config_dir() / "vst3-quarantine.txt";
  std::ifstream in(path);
  if (!in) return;
  std::string line;
  while (std::getline(in, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (!line.empty()) quarantine.insert(line);
  }
}

static void save_quarantine(const std::unordered_set<std::string>& quarantine) {
  const auto dir = mixbridge_config_dir();
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  std::ofstream out(dir / "vst3-quarantine.txt", std::ios::trunc);
  if (!out) return;
  for (const auto& p : quarantine) out << p << "\n";
}

static bool write_fx_state_file(const std::vector<uint8_t>& blob, std::string& out_path, std::string& error) {
  const auto dir = mixbridge_config_dir() / "fx-state";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const auto path = dir / ("state-" + std::to_string(GetTickCount64()) + ".bin");
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "state_write_failed";
    return false;
  }
  if (!blob.empty()) out.write(reinterpret_cast<const char*>(blob.data()), static_cast<std::streamsize>(blob.size()));
  out_path = path.string();
  return true;
}
#endif

static void list_processes(HANDLE pipe) {
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

static void handle_client(mixbridge::Engine& engine, HANDLE pipe
#if defined(MIXBRIDGE_WITH_VST3)
                          ,
                          FxMap& fx_map, std::unordered_set<std::string>& quarantine
#endif
) {
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
      char buf[480];
      std::snprintf(
        buf, sizeof(buf),
        "OK STATE %s BROADCAST %s LIVE_DEST %d FRAMES %llu XRUNS %llu UNDERRUNS %llu OVERRUNS %llu LAT_MS %.2f FEEDBACK %.3f",
        mixbridge::engine_state_name(d.state), mixbridge::broadcast_state_name(d.broadcast),
        d.live_destination_ready ? 1 : 0, static_cast<unsigned long long>(d.frames_rendered),
        static_cast<unsigned long long>(d.xruns), static_cast<unsigned long long>(d.underruns),
        static_cast<unsigned long long>(d.overruns), d.estimated_latency_ms, d.feedback_risk);
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
        std::string fx = engine.source_fx_name(s.id);
        if (fx.empty()) fx = "-";
        char buf[768];
        std::snprintf(buf, sizeof(buf),
                      "SOURCE ID %u KIND %s NAME %s DEVICE %s GAIN %.4f MUTE %d MONITOR %d BROADCAST %d FX %s",
                      s.id, source_kind_name(s.kind), s.name.c_str(),
                      s.device_id.empty() ? "-" : s.device_id.c_str(), s.gain, s.mute ? 1 : 0,
                      s.monitor ? 1 : 0, s.broadcast ? 1 : 0, fx.c_str());
        write_line(pipe, buf);
      }
      write_line(pipe, "OK END");
#if defined(MIXBRIDGE_WITH_VST3)
    } else if (cmd == "LIST_VST3") {
      auto plugins = mixbridge::vst3::list_installed();
      write_line(pipe, "OK COUNT " + std::to_string(plugins.size()));
      for (const auto& p : plugins) {
        const int q = quarantine.count(p.second) ? 1 : 0;
        write_line(pipe, "PLUGIN NAME " + p.first + " QUARANTINE " + std::to_string(q) + " PATH " +
                           p.second);
      }
      write_line(pipe, "OK END");
    } else if (cmd == "CLEAR_QUARANTINE") {
      quarantine.clear();
      save_quarantine(quarantine);
      write_line(pipe, "OK");
    } else if (cmd == "LIST_FX") {
      uint32_t id = 0;
      iss >> id;
      auto it = fx_map.find(id);
      if (it == fx_map.end() || !it->second) {
        write_line(pipe, "OK COUNT 0");
        write_line(pipe, "OK END");
      } else {
        write_line(pipe, "OK COUNT " + std::to_string(it->second->plugs.size()));
        for (size_t i = 0; i < it->second->plugs.size(); ++i) {
          write_line(pipe, "FX INDEX " + std::to_string(i) + " NAME " + it->second->plugs[i]->name() +
                             " PATH " + it->second->plugs[i]->path() + " BYPASS " +
                             (it->second->plugs[i]->bypassed() ? "1" : "0"));
        }
        write_line(pipe, "OK END");
      }
    } else if (cmd == "ADD_FX") {
      uint32_t id = 0;
      iss >> id;
      std::string path;
      std::getline(iss >> std::ws, path);
      while (!path.empty() && (path.back() == ' ' || path.back() == '\t')) path.pop_back();
      if (quarantine.count(path)) {
        write_line(pipe, "ERR quarantined");
      } else {
        auto proc = std::make_unique<mixbridge::vst3::Processor>();
        if (!proc->load(path, err)) {
          quarantine.insert(path);
          save_quarantine(quarantine);
          write_line(pipe, "ERR " + err);
        } else if (!proc->prepare(48000.0, 512, err)) {
          quarantine.insert(path);
          save_quarantine(quarantine);
          write_line(pipe, "ERR " + err);
        } else {
          proc->set_bypass(false);
          auto& chain = fx_map[id];
          if (!chain) chain = std::make_unique<FxChain>();
          chain->plugs.push_back(std::move(proc));
          rebind_fx(engine, fx_map, id);
          write_line(pipe, "OK FX " + chain_display_name(*fx_map[id]) + " INDEX " +
                             std::to_string(fx_map[id]->plugs.size() - 1));
        }
      }
    } else if (cmd == "REMOVE_FX") {
      uint32_t id = 0;
      iss >> id;
      std::string rest;
      std::getline(iss >> std::ws, rest);
      while (!rest.empty() && (rest.back() == ' ' || rest.back() == '\t')) rest.pop_back();
      if (!rest.empty()) {
        int index = 0;
        try {
          index = std::stoi(rest);
        } catch (...) {
          write_line(pipe, "ERR bad_index");
          continue;
        }
        auto it = fx_map.find(id);
        if (it == fx_map.end() || !it->second || index < 0 ||
            static_cast<size_t>(index) >= it->second->plugs.size()) {
          write_line(pipe, "ERR no_fx");
        } else {
          it->second->plugs.erase(it->second->plugs.begin() + index);
          rebind_fx(engine, fx_map, id);
          write_line(pipe, "OK");
        }
      } else {
        clear_fx(engine, fx_map, id);
        write_line(pipe, "OK");
      }
    } else if (cmd == "MOVE_FX") {
      uint32_t id = 0;
      int from = 0;
      int to = 0;
      iss >> id >> from >> to;
      auto it = fx_map.find(id);
      if (it == fx_map.end() || !it->second || from < 0 || to < 0 ||
          static_cast<size_t>(from) >= it->second->plugs.size() ||
          static_cast<size_t>(to) >= it->second->plugs.size()) {
        write_line(pipe, "ERR no_fx");
      } else {
        auto plug = std::move(it->second->plugs[static_cast<size_t>(from)]);
        it->second->plugs.erase(it->second->plugs.begin() + from);
        it->second->plugs.insert(it->second->plugs.begin() + to, std::move(plug));
        rebind_fx(engine, fx_map, id);
        write_line(pipe, "OK");
      }
    } else if (cmd == "SET_FX_BYPASS") {
      uint32_t id = 0;
      int b = 0;
      int index = -1;
      iss >> id >> b;
      if (iss >> index) {
        auto it = fx_map.find(id);
        if (it == fx_map.end() || !it->second || index < 0 ||
            static_cast<size_t>(index) >= it->second->plugs.size()) {
          write_line(pipe, "ERR no_fx");
        } else {
          it->second->plugs[static_cast<size_t>(index)]->set_bypass(b != 0);
          write_line(pipe, "OK");
        }
      } else if (auto it = fx_map.find(id); it != fx_map.end() && it->second) {
        for (auto& p : it->second->plugs) p->set_bypass(b != 0);
        engine.set_source_fx_bypass(id, b != 0);
        write_line(pipe, "OK");
      } else {
        write_line(pipe, "ERR no_fx");
      }
    } else if (cmd == "OPEN_FX_EDITOR") {
      uint32_t id = 0;
      int index = 0;
      iss >> id;
      if (!(iss >> index)) index = 0;
      auto it = fx_map.find(id);
      if (it == fx_map.end() || !it->second || index < 0 ||
          static_cast<size_t>(index) >= it->second->plugs.size()) {
        write_line(pipe, "ERR no_fx");
      } else if (it->second->plugs[static_cast<size_t>(index)]->open_editor(err)) {
        write_line(pipe, "OK");
      } else {
        write_line(pipe, "ERR " + err);
      }
    } else if (cmd == "CLOSE_FX_EDITOR") {
      uint32_t id = 0;
      int index = 0;
      iss >> id;
      if (!(iss >> index)) index = 0;
      auto it = fx_map.find(id);
      if (it == fx_map.end() || !it->second || index < 0 ||
          static_cast<size_t>(index) >= it->second->plugs.size()) {
        write_line(pipe, "ERR no_fx");
      } else {
        it->second->plugs[static_cast<size_t>(index)]->close_editor();
        write_line(pipe, "OK");
      }
    } else if (cmd == "GET_FX_STATE") {
      uint32_t id = 0;
      int index = 0;
      iss >> id;
      if (!(iss >> index)) index = 0;
      auto it = fx_map.find(id);
      if (it == fx_map.end() || !it->second || index < 0 ||
          static_cast<size_t>(index) >= it->second->plugs.size()) {
        write_line(pipe, "ERR no_fx");
      } else {
        std::vector<uint8_t> blob;
        std::string path;
        if (!it->second->plugs[static_cast<size_t>(index)]->get_state(blob, err)) {
          write_line(pipe, "ERR " + err);
        } else if (!write_fx_state_file(blob, path, err)) {
          write_line(pipe, "ERR " + err);
        } else {
          write_line(pipe, "OK BYTES " + std::to_string(blob.size()) + " PATH " + path);
        }
      }
    } else if (cmd == "SET_FX_STATE") {
      uint32_t id = 0;
      int index = 0;
      iss >> id >> index;
      std::string path;
      std::getline(iss >> std::ws, path);
      while (!path.empty() && (path.back() == ' ' || path.back() == '\t')) path.pop_back();
      auto it = fx_map.find(id);
      if (it == fx_map.end() || !it->second || index < 0 ||
          static_cast<size_t>(index) >= it->second->plugs.size()) {
        write_line(pipe, "ERR no_fx");
      } else {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
          write_line(pipe, "ERR state_read_failed");
        } else {
          std::vector<uint8_t> blob((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
          if (!it->second->plugs[static_cast<size_t>(index)]->set_state(
                blob.data(), blob.size(), err)) {
            write_line(pipe, "ERR " + err);
          } else {
            write_line(pipe, "OK");
          }
        }
      }
#endif
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
    } else if (cmd == "REGISTER_VST3_SEND") {
      // Scaffold: VST3 Send plugin registers intent + SHM mapping name.
      // Audio ingestion (ADD_SEND_BUFFER / engine ring push) is not wired yet.
      std::string name;
      std::string transport;
      std::string path;
      iss >> name >> transport;
      std::getline(iss >> std::ws, path);
      while (!path.empty() && (path.back() == ' ' || path.back() == '\t')) path.pop_back();
      if (name.empty() || transport != "SHM" || path.empty()) {
        write_line(pipe, "ERR bad_register_vst3_send");
      } else {
        write_line(pipe, "OK RESERVED NAME " + name + " TRANSPORT " + transport + " PATH " + path +
                           " NOTE send_sink_not_wired");
      }
    } else if (cmd == "REMOVE") {
      uint32_t id = 0;
      iss >> id;
#if defined(MIXBRIDGE_WITH_VST3)
      clear_fx(engine, fx_map, id);
#endif
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
#if defined(MIXBRIDGE_WITH_VST3)
      fx_map.clear();
#endif
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

#if defined(MIXBRIDGE_WITH_VST3)
  FxMap fx_map;
  std::unordered_set<std::string> quarantine;
  load_quarantine(quarantine);
#endif

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
#if defined(MIXBRIDGE_WITH_VST3)
    handle_client(engine, pipe, fx_map, quarantine);
#else
    handle_client(engine, pipe);
#endif
  }
}
