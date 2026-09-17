#include "mixbridge/device_manager.hpp"
#include "mixbridge/wasapi_util.hpp"

#include <mmdeviceapi.h>

namespace mixbridge {

class DeviceManager::NotifyClient : public IMMNotificationClient {
public:
  explicit NotifyClient(DeviceManager* owner) : owner_(owner) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv) return E_POINTER;
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
      *ppv = static_cast<IMMNotificationClient*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG n = InterlockedDecrement(&ref_);
    if (n == 0) delete this;
    return n;
  }

  HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override {
    if (owner_ && owner_->on_invalidate_) owner_->on_invalidate_();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {
    if (owner_ && owner_->on_invalidate_) owner_->on_invalidate_();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
    if (owner_ && owner_->on_invalidate_) owner_->on_invalidate_();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow, ERole, LPCWSTR) override {
    if (owner_ && owner_->on_invalidate_) owner_->on_invalidate_();
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override {
    return S_OK;
  }

private:
  DeviceManager* owner_ = nullptr;
  LONG ref_ = 1;
};

DeviceManager::DeviceManager() = default;

DeviceManager::~DeviceManager() { shutdown(); }

bool DeviceManager::init(std::string& error) {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (SUCCEEDED(hr)) {
    com_inited_here_ = true;
  } else if (hr == RPC_E_CHANGED_MODE || hr == S_FALSE) {
    com_inited_here_ = false;
  } else {
    error = "CoInitializeEx failed " + wasapi::hr_hex(hr);
    return false;
  }

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator_));
  if (FAILED(hr) || !enumerator_) {
    error = "MMDeviceEnumerator create failed " + wasapi::hr_hex(hr);
    return false;
  }

  notify_ = new NotifyClient(this);
  enumerator_->RegisterEndpointNotificationCallback(notify_);
  return true;
}

void DeviceManager::shutdown() {
  if (enumerator_ && notify_) {
    enumerator_->UnregisterEndpointNotificationCallback(notify_);
  }
  if (notify_) {
    notify_->Release();
    notify_ = nullptr;
  }
  if (enumerator_) {
    enumerator_->Release();
    enumerator_ = nullptr;
  }
  if (com_inited_here_) {
    CoUninitialize();
    com_inited_here_ = false;
  }
}

void DeviceManager::set_invalidation_callback(InvalidationFn fn) { on_invalidate_ = std::move(fn); }

static std::vector<DeviceInfo> list_flow(IMMDeviceEnumerator* enumerator, EDataFlow flow) {
  std::vector<DeviceInfo> out;
  if (!enumerator) return out;
  IMMDeviceCollection* coll = nullptr;
  if (FAILED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &coll)) || !coll) return out;
  UINT count = 0;
  coll->GetCount(&count);
  for (UINT i = 0; i < count; ++i) {
    IMMDevice* d = nullptr;
    if (FAILED(coll->Item(i, &d)) || !d) continue;
    DeviceInfo info;
    info.id = wasapi::device_id_string(d);
    info.name = wasapi::device_friendly_name(d);
    info.capture = (flow == eCapture);
    out.push_back(std::move(info));
    d->Release();
  }
  coll->Release();
  return out;
}

std::vector<DeviceInfo> DeviceManager::list_capture() const {
  return list_flow(enumerator_, eCapture);
}

std::vector<DeviceInfo> DeviceManager::list_render() const {
  return list_flow(enumerator_, eRender);
}

IMMDevice* DeviceManager::open_default(bool capture) const {
  if (!enumerator_) return nullptr;
  IMMDevice* d = nullptr;
  enumerator_->GetDefaultAudioEndpoint(capture ? eCapture : eRender, eConsole, &d);
  return d;
}

IMMDevice* DeviceManager::open_by_id(const std::wstring& id) const {
  if (!enumerator_ || id.empty()) return nullptr;
  IMMDevice* d = nullptr;
  enumerator_->GetDevice(id.c_str(), &d);
  return d;
}

}  // namespace mixbridge
