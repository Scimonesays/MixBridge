#pragma once

#include "mixbridge/source_slot.hpp"
#include "mixbridge/types.hpp"

#include <audioclient.h>
#include <mmdeviceapi.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace mixbridge {

// Captures into SourceSlot::ring as interleaved float stereo @ kEngineRate (via AUTOCONVERTPCM).
class WasapiCaptureSource {
public:
  WasapiCaptureSource() = default;
  ~WasapiCaptureSource();

  WasapiCaptureSource(const WasapiCaptureSource&) = delete;
  WasapiCaptureSource& operator=(const WasapiCaptureSource&) = delete;

  bool start_physical(IMMDevice* device, SourceSlot* slot, std::string& error);
  bool start_system_loopback(IMMDevice* render_device, SourceSlot* slot, std::string& error);
  bool start_process_loopback(uint32_t pid, SourceSlot* slot, std::string& error);
  void stop();

  bool running() const { return running_.load(std::memory_order_acquire); }

private:
  enum class Mode { Physical, SystemLoopback, ProcessLoopback };

  void thread_main(Mode mode, IMMDevice* device, uint32_t pid);
  bool run_endpoint_capture(IMMDevice* device, bool loopback, std::string& error);
  bool run_process_capture(uint32_t pid, std::string& error);

  SourceSlot* slot_ = nullptr;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> running_{false};
  std::string last_error_;
};

class WasapiRenderSink {
public:
  WasapiRenderSink() = default;
  ~WasapiRenderSink();

  WasapiRenderSink(const WasapiRenderSink&) = delete;
  WasapiRenderSink& operator=(const WasapiRenderSink&) = delete;

  bool open(IMMDevice* device, std::string& error);
  void close();

  // Blocking event-driven render of interleaved float stereo @ engine rate (AUTOCONVERT).
  // `fill` writes `frames` stereo frames into `dst` (frames*2 floats). Return false to stop.
  using FillFn = bool (*)(void* user, float* dst, uint32_t frames);
  bool run_loop(FillFn fill, void* user, std::atomic<bool>& stop_flag, std::string& error);

  uint32_t buffer_frames() const { return buffer_frames_; }
  uint32_t device_rate() const { return device_rate_; }
  uint32_t channels() const { return channels_; }

private:
  IMMDevice* device_ = nullptr;  // not owned
  IAudioClient* client_ = nullptr;
  IAudioRenderClient* render_ = nullptr;
  HANDLE event_ = nullptr;
  WAVEFORMATEX* mix_ = nullptr;
  uint32_t buffer_frames_ = 0;
  uint32_t device_rate_ = 0;
  uint32_t channels_ = 0;
  bool write_float_ = false;
  bool opened_ = false;
};

}  // namespace mixbridge
