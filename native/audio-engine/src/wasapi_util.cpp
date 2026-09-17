#include "mixbridge/wasapi_util.hpp"

#include <cstdio>
#include <sstream>

namespace mixbridge::wasapi {

std::string hr_hex(HRESULT hr) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "0x%08lX", static_cast<unsigned long>(hr));
  return buf;
}

std::wstring device_id_string(IMMDevice* device) {
  if (!device) return {};
  LPWSTR id = nullptr;
  if (FAILED(device->GetId(&id)) || !id) return {};
  std::wstring out = id;
  CoTaskMemFree(id);
  return out;
}

std::wstring device_friendly_name(IMMDevice* device) {
  if (!device) return L"";
  IPropertyStore* props = nullptr;
  if (FAILED(device->OpenPropertyStore(STGM_READ, &props)) || !props) {
    return device_id_string(device);
  }
  PROPVARIANT var;
  PropVariantInit(&var);
  std::wstring name;
  if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var)) && var.vt == VT_LPWSTR && var.pwszVal) {
    name = var.pwszVal;
  } else {
    name = device_id_string(device);
  }
  PropVariantClear(&var);
  props->Release();
  return name;
}

}  // namespace mixbridge::wasapi
