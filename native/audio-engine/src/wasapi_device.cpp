#include "mixbridge/engine.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace mixbridge {

struct DeviceInfo {
  std::wstring id;
  std::wstring name;
  bool capture = false;
};

// Placeholder — full WASAPI enumeration lives in probes today and will move here.
std::vector<DeviceInfo> list_devices_placeholder() {
  return {};
}

}  // namespace mixbridge

// Keep a translation unit for future WASAPI device control without linking unused COM yet.
void mixbridge_wasapi_device_tu_anchor() {}
