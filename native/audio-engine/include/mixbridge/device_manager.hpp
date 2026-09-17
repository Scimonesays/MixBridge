#pragma once

#include "mixbridge/types.hpp"

#include <functional>
#include <string>
#include <vector>

struct IMMDevice;
struct IMMDeviceEnumerator;

namespace mixbridge {

class DeviceManager {
public:
  DeviceManager();
  ~DeviceManager();

  DeviceManager(const DeviceManager&) = delete;
  DeviceManager& operator=(const DeviceManager&) = delete;

  bool init(std::string& error);
  void shutdown();

  std::vector<DeviceInfo> list_capture() const;
  std::vector<DeviceInfo> list_render() const;

  // Returns AddRef'd device or nullptr.
  IMMDevice* open_default(bool capture) const;
  IMMDevice* open_by_id(const std::wstring& id) const;

  using InvalidationFn = std::function<void()>;
  void set_invalidation_callback(InvalidationFn fn);

  IMMDeviceEnumerator* enumerator() const { return enumerator_; }

private:
  class NotifyClient;
  IMMDeviceEnumerator* enumerator_ = nullptr;
  NotifyClient* notify_ = nullptr;
  InvalidationFn on_invalidate_;
  bool com_inited_here_ = false;
};

}  // namespace mixbridge
