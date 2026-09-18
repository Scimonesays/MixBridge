#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace mixbridge {

inline constexpr uint32_t kEngineRate = 48000;
inline constexpr uint32_t kEngineChannels = 2;
inline constexpr uint32_t kMaxSources = 16;
inline constexpr uint32_t kRingFrames = 1u << 14;  // samples (floats), power of two

enum class EngineState : uint32_t {
  Stopped = 0,
  Starting = 1,
  Running = 2,
  Recovering = 3,
  DeviceMissing = 4,
  Failed = 5,
};

// Independent of EngineState. Standby keeps monitoring; Live enables broadcast sink.
enum class BroadcastState : uint32_t {
  Standby = 0,
  Live = 1,
};

inline const char* engine_state_name(EngineState s) {
  switch (s) {
    case EngineState::Stopped: return "stopped";
    case EngineState::Starting: return "starting";
    case EngineState::Running: return "running";
    case EngineState::Recovering: return "recovering";
    case EngineState::DeviceMissing: return "device_missing";
    case EngineState::Failed: return "failed";
  }
  return "unknown";
}

inline const char* broadcast_state_name(BroadcastState s) {
  switch (s) {
    case BroadcastState::Standby: return "standby";
    case BroadcastState::Live: return "live";
  }
  return "unknown";
}

enum class SourceKind : uint8_t {
  PhysicalCapture = 0,
  SystemLoopback = 1,
  ProcessLoopback = 2,
  ToneFixture = 3,
};

struct DeviceInfo {
  std::wstring id;
  std::wstring name;
  bool capture = false;
};

struct MeterSnapshot {
  float peak = 0.0f;
  float rms = 0.0f;
  bool clip = false;
};

struct EngineDiagnostics {
  EngineState state = EngineState::Stopped;
  BroadcastState broadcast = BroadcastState::Standby;
  bool live_destination_ready = false;
  uint64_t frames_rendered = 0;
  uint64_t xruns = 0;
  uint64_t underruns = 0;
  uint64_t overruns = 0;
  uint32_t buffer_frames = 0;
  uint32_t device_rate = 0;
  uint32_t engine_rate = kEngineRate;
  double estimated_latency_ms = 0.0;
};

struct SourceInfo {
  uint32_t id = 0;
  SourceKind kind = SourceKind::ToneFixture;
  std::string name;
  float gain = 1.0f;
  bool mute = false;
  bool monitor = true;
  bool broadcast = true;
  uint32_t process_id = 0;
  std::string effect_name;
  std::string effect_path;
  bool effect_bypass = false;
  bool effect_faulted = false;
  bool effect_editor_open = false;
  bool effect_dirty = false;
};

}  // namespace mixbridge
