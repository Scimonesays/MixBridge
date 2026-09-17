#include "wasapi_common.hpp"

#include <windows.h>

std::string wide_to_utf8(std::wstring_view ws) {
  if (ws.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
  std::string out(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), out.data(), n, nullptr, nullptr);
  return out;
}

std::wstring utf8_to_wide(std::string_view s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring out(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
  return out;
}

HRESULT get_default_device(EDataFlow flow, IMMDevice** device) {
  IMMDeviceEnumerator* enumerator = nullptr;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
  if (FAILED(hr)) return hr;
  hr = enumerator->GetDefaultAudioEndpoint(flow, eConsole, device);
  enumerator->Release();
  return hr;
}

static HRESULT device_name(IMMDevice* device, std::wstring& id, std::wstring& name) {
  LPWSTR eid = nullptr;
  HRESULT hr = device->GetId(&eid);
  if (FAILED(hr)) return hr;
  id = eid;
  CoTaskMemFree(eid);

  IPropertyStore* props = nullptr;
  hr = device->OpenPropertyStore(STGM_READ, &props);
  if (FAILED(hr)) return hr;
  PROPVARIANT var;
  PropVariantInit(&var);
  hr = props->GetValue(PKEY_Device_FriendlyName, &var);
  if (SUCCEEDED(hr) && var.vt == VT_LPWSTR && var.pwszVal) {
    name = var.pwszVal;
  } else {
    name = id;
  }
  PropVariantClear(&var);
  props->Release();
  return S_OK;
}

HRESULT enumerate_endpoints(EDataFlow flow, std::vector<std::pair<std::wstring, std::wstring>>& out) {
  IMMDeviceEnumerator* enumerator = nullptr;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
  if (FAILED(hr)) return hr;

  IMMDeviceCollection* coll = nullptr;
  hr = enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &coll);
  enumerator->Release();
  if (FAILED(hr)) return hr;

  UINT count = 0;
  coll->GetCount(&count);
  for (UINT i = 0; i < count; ++i) {
    IMMDevice* device = nullptr;
    if (FAILED(coll->Item(i, &device))) continue;
    std::wstring id, name;
    if (SUCCEEDED(device_name(device, id, name))) {
      out.emplace_back(std::move(id), std::move(name));
    }
    device->Release();
  }
  coll->Release();
  return S_OK;
}
