#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mixbridge::vst3 {

// Thin realtime VST3 effect host. SDK types stay behind the pimpl boundary so
// the audio engine can consume this without importing Steinberg headers.
class Processor {
public:
  Processor();
  ~Processor();

  Processor(const Processor&) = delete;
  Processor& operator=(const Processor&) = delete;

  bool load(const std::string& module_path, std::string& error);
  void unload();

  bool prepare(double sample_rate, uint32_t max_block_frames, std::string& error);
  bool process(float* interleaved_stereo, uint32_t frames, std::string& error);
  bool save_state(std::vector<uint8_t>& component_state,
                  std::vector<uint8_t>& controller_state,
                  std::string& error);
  bool load_state(const std::vector<uint8_t>& component_state,
                  const std::vector<uint8_t>& controller_state,
                  std::string& error);
  // Realtime entry point: no error-string construction, no allocation, no locks.
  bool process_rt(float* interleaved_stereo, uint32_t frames) noexcept;

  void set_bypass(bool bypass) noexcept;
  bool bypass() const noexcept;
  bool faulted() const noexcept;
  bool loaded() const noexcept;
  bool prepared() const noexcept;
  const std::string& name() const noexcept;
  const std::string& path() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace mixbridge::vst3
