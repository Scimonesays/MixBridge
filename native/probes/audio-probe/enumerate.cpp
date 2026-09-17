#include "wasapi_common.hpp"

int cmd_enumerate() {
  std::vector<std::pair<std::wstring, std::wstring>> capture;
  std::vector<std::pair<std::wstring, std::wstring>> render;
  HRESULT hr = enumerate_endpoints(eCapture, capture);
  if (FAILED(hr)) {
    print_hr("enumerate capture", hr);
    return 2;
  }
  hr = enumerate_endpoints(eRender, render);
  if (FAILED(hr)) {
    print_hr("enumerate render", hr);
    return 2;
  }

  std::printf("capture_devices=%zu\n", capture.size());
  for (const auto& [id, name] : capture) {
    std::printf("  CAP  %s\n      id=%s\n", wide_to_utf8(name).c_str(), wide_to_utf8(id).c_str());
  }
  std::printf("render_devices=%zu\n", render.size());
  for (const auto& [id, name] : render) {
    std::printf("  REN  %s\n      id=%s\n", wide_to_utf8(name).c_str(), wide_to_utf8(id).c_str());
  }
  return (capture.empty() && render.empty()) ? 1 : 0;
}
