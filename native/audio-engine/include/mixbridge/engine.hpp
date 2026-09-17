#pragma once

#include "mixbridge/device_manager.hpp"
#include "mixbridge/meters.hpp"
#include "mixbridge/mix_graph.hpp"
#include "mixbridge/source_slot.hpp"
#include "mixbridge/types.hpp"
#include "mixbridge/wasapi_io.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mixbridge {

struct AddPhysicalRequest {
  std::wstring device_id;  // empty = default capture
  std::string name;
};

struct AddProcessRequest {
  uint32_t pid = 0;
  std::string name;
};

struct AddToneRequest {
  float hz = 440.0f;
  std::string name;
};

class Engine {
public:
  Engine();
  ~Engine();

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  bool init(std::string& error);
  void shutdown();

  EngineState state() const {
    return static_cast<EngineState>(state_.load(std::memory_order_acquire));
  }

  EngineDiagnostics diagnostics() const;
  std::vector<DeviceInfo> list_capture_devices() const;
  std::vector<DeviceInfo> list_render_devices() const;

  // Select monitor render device (empty = default). Call before start.
  bool set_monitor_device(const std::wstring& device_id, std::string& error);

  // Graph control (non-realtime). Returns source id or 0 on failure.
  uint32_t add_physical_capture(const AddPhysicalRequest& req, std::string& error);
  uint32_t add_process_loopback(const AddProcessRequest& req, std::string& error);
  uint32_t add_tone(const AddToneRequest& req, std::string& error);
  bool remove_source(uint32_t id, std::string& error);

  bool set_gain(uint32_t id, float gain);
  bool set_mute(uint32_t id, bool mute);
  bool set_solo(uint32_t id, bool solo);
  bool set_monitor(uint32_t id, bool enabled);
  bool set_broadcast(uint32_t id, bool enabled);
  bool set_pan(uint32_t id, float pan);
  bool set_master_gain(float gain);

  MeterSnapshot source_meter(uint32_t id);
  MeterSnapshot master_meter();

  // Start/stop realtime monitor mix.
  bool start(std::string& error);
  void stop();

  // Offline/test: mix tone/fixture rings into buffers without WASAPI (unit tests).
  void render_offline(uint32_t frames, float* monitor, float* broadcast);

  // Capture last N seconds of monitor mix into float buffer (for analysis harness).
  void enable_tap(bool on);
  bool copy_tap(std::vector<float>& out_interleaved) const;

  SourceSlot* slot_by_id(uint32_t id);
  const SourceSlot* slots() const { return slots_; }

private:
  int alloc_slot();
  void free_slot(int index);
  static bool render_fill_thunk(void* user, float* dst, uint32_t frames);
  bool render_fill(float* dst, uint32_t frames);
  void on_device_invalidated();
  void set_state(EngineState s);

  DeviceManager devices_;
  SourceSlot slots_[kMaxSources];
  WasapiCaptureSource captures_[kMaxSources];
  WasapiRenderSink monitor_sink_;

  MixGraph graph_;
  AtomicMeter master_meter_;

  std::atomic<uint32_t> state_{static_cast<uint32_t>(EngineState::Stopped)};
  std::atomic<uint64_t> frames_rendered_{0};
  std::atomic<uint64_t> xruns_{0};
  std::atomic<uint64_t> underruns_{0};
  std::atomic<uint64_t> overruns_{0};
  std::atomic<float> master_gain_{1.0f};
  std::atomic<uint32_t> next_id_{1};

  std::wstring monitor_device_id_;
  std::thread render_thread_;
  std::atomic<bool> stop_render_{false};
  std::string last_error_;

  // Preallocated realtime scratch (constructed at init size).
  std::vector<float> scratch_monitor_;
  std::vector<float> scratch_broadcast_;
  std::vector<float> scratch_source_;  // max period * 2
  uint32_t scratch_frames_cap_ = 0;

  // Tap for analysis harness: lock-free SPSC; control thread drains.
  std::atomic<bool> tap_enabled_{false};
  SpscFloatRing tap_ring_{1u << 18};  // ~2.7s stereo @ 48k

  std::mutex control_mu_;
};

}  // namespace mixbridge
