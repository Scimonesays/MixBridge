#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace mixbridge::vst3 {

struct ProcessStats {
  float in_peak = 0.0f;
  float out_peak = 0.0f;
  float in_rms = 0.0f;
  float out_rms = 0.0f;
};

// Hosts one VST3 audio effect. Control-thread load; process() is RT-callable once active.
class Processor {
public:
  Processor();
  ~Processor();

  Processor(const Processor&) = delete;
  Processor& operator=(const Processor&) = delete;

  bool load(const std::string& path, std::string& error);
  void unload();

  bool loaded() const { return loaded_; }
  const std::string& path() const { return path_; }
  const std::string& name() const { return name_; }

  bool set_bypass(bool bypass);
  bool bypassed() const { return bypass_; }

  // Sample rate / max block; call after load, before process.
  bool prepare(double sample_rate, int32_t max_block, std::string& error);

  // Interleaved stereo float in/out (may alias). frames <= max_block.
  bool process(float* interleaved_io, int32_t frames, ProcessStats* stats = nullptr);

  // Component state blob (getState/setState).
  bool get_state(std::vector<uint8_t>& out, std::string& error);
  bool set_state(const uint8_t* data, size_t size, std::string& error);

  // Best-effort editor open on Windows HWND (non-modal helper window).
  bool open_editor(std::string& error);
  void close_editor();

private:
  struct Impl;
  Impl* impl_ = nullptr;
  bool loaded_ = false;
  bool bypass_ = false;
  bool prepared_ = false;
  std::string path_;
  std::string name_;
  std::mutex control_mu_;
};

// Scan helpers (filesystem).
std::vector<std::pair<std::string, std::string>> list_installed();  // name, path

}  // namespace mixbridge::vst3
