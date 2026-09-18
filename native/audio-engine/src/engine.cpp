#include "mixbridge/engine.hpp"
#include "mixbridge/wasapi_util.hpp"

#include <windows.h>

#include <algorithm>
#include <cstring>

namespace mixbridge {

Engine::Engine() = default;

Engine::~Engine() { shutdown(); }

bool Engine::init(std::string& error) {
  if (!devices_.init(error)) return false;
  devices_.set_invalidation_callback([this]() { on_device_invalidated(); });
  scratch_frames_cap_ = 4096;
  scratch_monitor_.assign(static_cast<size_t>(scratch_frames_cap_) * kEngineChannels, 0.0f);
  scratch_broadcast_.assign(static_cast<size_t>(scratch_frames_cap_) * kEngineChannels, 0.0f);
  scratch_source_.assign(static_cast<size_t>(scratch_frames_cap_) * kEngineChannels, 0.0f);
  set_state(EngineState::Stopped);
  return true;
}

void Engine::shutdown() {
  stop();
  stop_live_thread();
  for (uint32_t i = 0; i < kMaxSources; ++i) {
    captures_[i].stop();
    slots_[i].active.store(false);
  }
  devices_.shutdown();
}

void Engine::set_state(EngineState s) {
  state_.store(static_cast<uint32_t>(s), std::memory_order_release);
}

void Engine::on_device_invalidated() {
  const auto s = state();
  if (s == EngineState::Running || s == EngineState::Starting) {
    set_state(EngineState::Recovering);
    // Soft-stop render; control layer may restart.
    stop_render_.store(true, std::memory_order_release);
  } else if (s != EngineState::Stopped) {
    set_state(EngineState::DeviceMissing);
  }
}

EngineDiagnostics Engine::diagnostics() const {
  EngineDiagnostics d;
  d.state = state();
  d.broadcast = broadcast_state();
  d.live_destination_ready = live_destination_ready();
  d.frames_rendered = frames_rendered_.load(std::memory_order_relaxed);
  d.xruns = xruns_.load(std::memory_order_relaxed);
  d.underruns = underruns_.load(std::memory_order_relaxed);
  d.overruns = overruns_.load(std::memory_order_relaxed);
  d.buffer_frames = monitor_sink_.buffer_frames();
  d.device_rate = monitor_sink_.device_rate();
  d.engine_rate = kEngineRate;
  if (d.device_rate > 0 && d.buffer_frames > 0) {
    d.estimated_latency_ms =
      1000.0 * static_cast<double>(d.buffer_frames) / static_cast<double>(d.device_rate);
  }
  return d;
}

std::vector<DeviceInfo> Engine::list_capture_devices() const { return devices_.list_capture(); }
std::vector<DeviceInfo> Engine::list_render_devices() const { return devices_.list_render(); }

std::vector<SourceInfo> Engine::list_sources() const {
  std::vector<SourceInfo> out;
  for (uint32_t i = 0; i < kMaxSources; ++i) {
    if (!slots_[i].active.load(std::memory_order_relaxed)) continue;
    SourceInfo info;
    info.id = slots_[i].id.load(std::memory_order_relaxed);
    info.kind = static_cast<SourceKind>(slots_[i].kind.load(std::memory_order_relaxed));
    info.name = slots_[i].name;
    info.gain = slots_[i].gain.load(std::memory_order_relaxed);
    info.mute = slots_[i].mute.load(std::memory_order_relaxed);
    info.monitor = slots_[i].monitor.load(std::memory_order_relaxed);
    info.broadcast = slots_[i].broadcast.load(std::memory_order_relaxed);
    info.process_id = slots_[i].process_id.load(std::memory_order_relaxed);
    out.push_back(std::move(info));
  }
  return out;
}

bool Engine::set_monitor_device(const std::wstring& device_id, std::string& error) {
  std::lock_guard<std::mutex> lock(control_mu_);
  if (state() == EngineState::Running || state() == EngineState::Starting) {
    error = "cannot change monitor device while running";
    return false;
  }
  monitor_device_id_ = device_id;
  return true;
}

bool Engine::set_live_device(const std::wstring& device_id, std::string& error) {
  if (device_id.empty()) {
    stop_live_thread();
    std::lock_guard<std::mutex> lock(control_mu_);
    live_device_id_.clear();
    live_destination_ready_.store(false, std::memory_order_release);
    broadcast_state_.store(static_cast<uint32_t>(BroadcastState::Standby), std::memory_order_release);
    return true;
  }
  std::lock_guard<std::mutex> lock(control_mu_);
  IMMDevice* device = devices_.open_by_id(device_id);
  if (!device) {
    error = "live render device not found";
    return false;
  }
  device->Release();
  live_device_id_ = device_id;
  live_destination_ready_.store(true, std::memory_order_release);
  return true;
}

std::wstring Engine::live_device_id() const { return live_device_id_; }

int Engine::alloc_slot() {
  for (int i = 0; i < static_cast<int>(kMaxSources); ++i) {
    if (!slots_[i].active.load(std::memory_order_relaxed)) return i;
  }
  return -1;
}

void Engine::free_slot(int index) {
  if (index < 0 || index >= static_cast<int>(kMaxSources)) return;
  captures_[index].stop();
  slots_[index].active.store(false, std::memory_order_release);
  slots_[index].effect.store(nullptr, std::memory_order_release);
  slots_[index].ring.clear();
  slots_[index].meter.reset();
  slots_[index].name.clear();
  slots_[index].device_id.clear();
}

SourceSlot* Engine::slot_by_id(uint32_t id) {
  for (uint32_t i = 0; i < kMaxSources; ++i) {
    if (slots_[i].active.load(std::memory_order_relaxed) &&
        slots_[i].id.load(std::memory_order_relaxed) == id) {
      return &slots_[i];
    }
  }
  return nullptr;
}

uint32_t Engine::add_tone(const AddToneRequest& req, std::string& error) {
  std::lock_guard<std::mutex> lock(control_mu_);
  const int idx = alloc_slot();
  if (idx < 0) {
    error = "no free source slots";
    return 0;
  }
  auto& slot = slots_[idx];
  const uint32_t id = next_id_.fetch_add(1);
  slot.id.store(id);
  slot.kind.store(static_cast<uint32_t>(SourceKind::ToneFixture));
  slot.tone_hz.store(req.hz);
  slot.effect.store(nullptr, std::memory_order_relaxed);
  slot.gain.store(1.0f);
  slot.pan.store(0.0f);
  slot.mute.store(false);
  slot.solo.store(false);
  slot.monitor.store(true);
  slot.broadcast.store(true);
  slot.tone_phase = 0.0;
  slot.name = req.name.empty() ? ("tone-" + std::to_string(static_cast<int>(req.hz))) : req.name;
  slot.ring.clear();
  slot.active.store(true, std::memory_order_release);
  return id;
}

uint32_t Engine::add_physical_capture(const AddPhysicalRequest& req, std::string& error) {
  std::lock_guard<std::mutex> lock(control_mu_);
  const int idx = alloc_slot();
  if (idx < 0) {
    error = "no free source slots";
    return 0;
  }

  IMMDevice* device = nullptr;
  if (req.device_id.empty()) {
    device = devices_.open_default(true);
  } else {
    device = devices_.open_by_id(req.device_id);
  }
  if (!device) {
    error = "capture device not found";
    return 0;
  }

  auto& slot = slots_[idx];
  const uint32_t id = next_id_.fetch_add(1);
  slot.id.store(id);
  slot.kind.store(static_cast<uint32_t>(SourceKind::PhysicalCapture));
  slot.effect.store(nullptr, std::memory_order_relaxed);
  slot.device_id = wasapi::device_id_string(device);
  if (!req.name.empty()) {
    slot.name = req.name;
  } else {
    const auto wname = wasapi::device_friendly_name(device);
    int n = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);
    slot.name.assign(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
    if (n > 1) {
      WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, slot.name.data(), n, nullptr, nullptr);
    }
  }
  slot.gain.store(1.0f);
  slot.mute.store(false);
  slot.solo.store(false);
  slot.monitor.store(true);
  slot.broadcast.store(true);
  slot.ring.clear();
  slot.active.store(true, std::memory_order_release);

  if (!captures_[idx].start_physical(device, &slot, error)) {
    slot.active.store(false);
    device->Release();
    return 0;
  }
  device->Release();
  return id;
}

uint32_t Engine::add_process_loopback(const AddProcessRequest& req, std::string& error) {
  std::lock_guard<std::mutex> lock(control_mu_);
  if (req.pid == 0) {
    error = "pid required";
    return 0;
  }
  const int idx = alloc_slot();
  if (idx < 0) {
    error = "no free source slots";
    return 0;
  }
  auto& slot = slots_[idx];
  const uint32_t id = next_id_.fetch_add(1);
  slot.id.store(id);
  slot.kind.store(static_cast<uint32_t>(SourceKind::ProcessLoopback));
  slot.process_id.store(req.pid);
  slot.effect.store(nullptr, std::memory_order_relaxed);
  slot.name = req.name.empty() ? ("pid-" + std::to_string(req.pid)) : req.name;
  slot.gain.store(1.0f);
  slot.mute.store(false);
  slot.solo.store(false);
  slot.monitor.store(true);
  slot.broadcast.store(true);
  slot.ring.clear();
  slot.active.store(true, std::memory_order_release);
  if (!captures_[idx].start_process_loopback(req.pid, &slot, error)) {
    slot.active.store(false);
    return 0;
  }
  return id;
}

bool Engine::remove_source(uint32_t id, std::string& error) {
  std::lock_guard<std::mutex> lock(control_mu_);
  for (int i = 0; i < static_cast<int>(kMaxSources); ++i) {
    if (slots_[i].active.load() && slots_[i].id.load() == id) {
      free_slot(i);
      return true;
    }
  }
  error = "source not found";
  return false;
}

bool Engine::set_gain(uint32_t id, float gain) {
  if (auto* s = slot_by_id(id)) {
    s->gain.store(std::clamp(gain, 0.0f, 4.0f), std::memory_order_relaxed);
    return true;
  }
  return false;
}
bool Engine::set_mute(uint32_t id, bool mute) {
  if (auto* s = slot_by_id(id)) {
    s->mute.store(mute);
    return true;
  }
  return false;
}
bool Engine::set_solo(uint32_t id, bool solo) {
  if (auto* s = slot_by_id(id)) {
    s->solo.store(solo);
    return true;
  }
  return false;
}
bool Engine::set_monitor(uint32_t id, bool enabled) {
  if (auto* s = slot_by_id(id)) {
    s->monitor.store(enabled);
    return true;
  }
  return false;
}
bool Engine::set_broadcast(uint32_t id, bool enabled) {
  if (auto* s = slot_by_id(id)) {
    s->broadcast.store(enabled);
    return true;
  }
  return false;
}
bool Engine::set_pan(uint32_t id, float pan) {
  if (auto* s = slot_by_id(id)) {
    s->pan.store(std::clamp(pan, -1.0f, 1.0f));
    return true;
  }
  return false;
}
bool Engine::set_master_gain(float gain) {
  master_gain_.store(std::clamp(gain, 0.0f, 4.0f));
  return true;
}

MeterSnapshot Engine::source_meter(uint32_t id) {
  if (auto* s = slot_by_id(id)) return s->meter.snapshot_and_reset();
  return {};
}
MeterSnapshot Engine::master_meter() { return master_meter_.snapshot_and_reset(); }
MeterSnapshot Engine::broadcast_meter() { return broadcast_meter_.snapshot_and_reset(); }

void Engine::set_live_destination_ready(bool ready) {
  live_destination_ready_.store(ready, std::memory_order_release);
  if (!ready) {
    stop_live_thread();
    broadcast_state_.store(static_cast<uint32_t>(BroadcastState::Standby), std::memory_order_release);
  }
}

void Engine::stop_live_thread() {
  stop_live_.store(true, std::memory_order_release);
  if (live_thread_.joinable()) live_thread_.join();
  live_sink_.close();
  live_ring_.clear();
}

bool Engine::live_fill_thunk(void* user, float* dst, uint32_t frames) {
  return static_cast<Engine*>(user)->live_fill(dst, frames);
}

bool Engine::live_fill(float* dst, uint32_t frames) {
  const std::size_t want = static_cast<std::size_t>(frames) * kEngineChannels;
  const std::size_t got = live_ring_.read(dst, want);
  for (std::size_t i = got; i < want; ++i) dst[i] = 0.0f;
  if (got < want) underruns_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

bool Engine::enable_broadcast(std::string& error) {
  if (!live_destination_ready()) {
    error = "no_live_destination";
    broadcast_state_.store(static_cast<uint32_t>(BroadcastState::Standby), std::memory_order_release);
    return false;
  }
  const auto st = state();
  if (st != EngineState::Running && st != EngineState::Starting) {
    error = "engine_not_running";
    return false;
  }

  stop_live_thread();

  std::wstring device_id;
  {
    std::lock_guard<std::mutex> lock(control_mu_);
    if (broadcast_state() == BroadcastState::Live) return true;
    device_id = live_device_id_;
    if (device_id.empty()) {
      // Test helper: ready flag without sink.
      broadcast_state_.store(static_cast<uint32_t>(BroadcastState::Live), std::memory_order_release);
      return true;
    }
  }

  IMMDevice* device = devices_.open_by_id(device_id);
  if (!device) {
    error = "live render device missing";
    return false;
  }
  if (!live_sink_.open(device, error)) {
    device->Release();
    return false;
  }
  device->Release();

  live_ring_.clear();
  stop_live_.store(false, std::memory_order_release);
  live_thread_ = std::thread([this]() {
    std::string err;
    live_sink_.run_loop(&Engine::live_fill_thunk, this, stop_live_, err);
    live_sink_.close();
  });
  broadcast_state_.store(static_cast<uint32_t>(BroadcastState::Live), std::memory_order_release);
  return true;
}

void Engine::disable_broadcast() {
  broadcast_state_.store(static_cast<uint32_t>(BroadcastState::Standby), std::memory_order_release);
  stop_live_thread();
}

bool Engine::render_fill_thunk(void* user, float* dst, uint32_t frames) {
  return static_cast<Engine*>(user)->render_fill(dst, frames);
}

bool Engine::render_fill(float* dst, uint32_t frames) {
  if (frames > scratch_frames_cap_) {
    xruns_.fetch_add(1, std::memory_order_relaxed);
    for (uint32_t i = 0; i < frames * 2; ++i) dst[i] = 0.0f;
    return true;
  }

  float* mon = scratch_monitor_.data();
  float* bc = scratch_broadcast_.data();
  graph_.process(slots_, frames, mon, bc, master_gain_.load(std::memory_order_relaxed), true);
  master_meter_.accumulate(mon, frames, kEngineChannels);
  broadcast_meter_.accumulate(bc, frames, kEngineChannels);
  frames_rendered_.fetch_add(frames, std::memory_order_relaxed);

  if (broadcast_state() == BroadcastState::Live) {
    const auto written =
      live_ring_.write(bc, static_cast<std::size_t>(frames) * kEngineChannels);
    if (written < static_cast<std::size_t>(frames) * kEngineChannels) {
      overruns_.fetch_add(1, std::memory_order_relaxed);
    }
  }

  // Count underruns roughly: if any active capture ring was empty-ish — skipped for now.

  std::memcpy(dst, mon, sizeof(float) * frames * kEngineChannels);

  if (tap_enabled_.load(std::memory_order_relaxed)) {
    tap_ring_.write(mon, static_cast<std::size_t>(frames) * kEngineChannels);
  }
  return true;
}

bool Engine::start(std::string& error) {
  std::lock_guard<std::mutex> lock(control_mu_);
  if (state() == EngineState::Running || state() == EngineState::Starting) {
    return true;  // idempotent — Go Live must not own this path
  }
  set_state(EngineState::Starting);
  stop_render_.store(false);

  IMMDevice* device = nullptr;
  if (monitor_device_id_.empty()) {
    device = devices_.open_default(false);
  } else {
    device = devices_.open_by_id(monitor_device_id_);
  }
  if (!device) {
    error = "monitor render device missing";
    set_state(EngineState::DeviceMissing);
    return false;
  }

  if (!monitor_sink_.open(device, error)) {
    device->Release();
    set_state(EngineState::Failed);
    return false;
  }
  device->Release();

  // Ensure scratch covers buffer period.
  const uint32_t need = std::max(monitor_sink_.buffer_frames(), 512u);
  if (need > scratch_frames_cap_) {
    scratch_frames_cap_ = need;
    scratch_monitor_.assign(static_cast<size_t>(need) * kEngineChannels, 0.0f);
    scratch_broadcast_.assign(static_cast<size_t>(need) * kEngineChannels, 0.0f);
    scratch_source_.assign(static_cast<size_t>(need) * kEngineChannels, 0.0f);
  }

  render_thread_ = std::thread([this]() {
    std::string err;
    set_state(EngineState::Running);
    const bool ok = monitor_sink_.run_loop(&Engine::render_fill_thunk, this, stop_render_, err);
    monitor_sink_.close();
    if (!ok && state() == EngineState::Running) {
      last_error_ = err;
      set_state(EngineState::Failed);
    } else if (state() == EngineState::Recovering) {
      set_state(EngineState::DeviceMissing);
    } else if (state() != EngineState::Failed && state() != EngineState::DeviceMissing) {
      set_state(EngineState::Stopped);
    }
  });

  return true;
}

void Engine::stop() {
  disable_broadcast();
  stop_render_.store(true, std::memory_order_release);
  if (render_thread_.joinable()) render_thread_.join();
  monitor_sink_.close();
  if (state() != EngineState::Failed && state() != EngineState::DeviceMissing) {
    set_state(EngineState::Stopped);
  }
}

void Engine::render_offline(uint32_t frames, float* monitor, float* broadcast) {
  graph_.process(slots_, frames, monitor, broadcast, master_gain_.load(), true);
  frames_rendered_.fetch_add(frames, std::memory_order_relaxed);
}

void Engine::enable_tap(bool on) {
  tap_enabled_.store(on, std::memory_order_release);
  if (on) tap_ring_.clear();
}

bool Engine::copy_tap(std::vector<float>& out_interleaved) const {
  out_interleaved.clear();
  float tmp[4096];
  for (;;) {
    const auto n = const_cast<SpscFloatRing&>(tap_ring_).read(tmp, 4096);
    if (n == 0) break;
    out_interleaved.insert(out_interleaved.end(), tmp, tmp + n);
  }
  return !out_interleaved.empty();
}

}  // namespace mixbridge
