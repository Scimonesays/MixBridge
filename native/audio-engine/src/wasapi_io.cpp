#include "mixbridge/wasapi_io.hpp"
#include "mixbridge/wasapi_util.hpp"

#include <wrl/client.h>
#include <wrl/implements.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::FtmBase;
using Microsoft::WRL::Make;

namespace mixbridge {
namespace {

class ActivateHandler
  : public RuntimeClass<RuntimeClassFlags<ClassicCom>, FtmBase, IActivateAudioInterfaceCompletionHandler> {
public:
  ActivateHandler() { done_ = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
  ~ActivateHandler() override {
    if (done_) CloseHandle(done_);
  }

  STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation) override {
    HRESULT activate_hr = E_FAIL;
    ComPtr<IUnknown> unk;
    HRESULT hr = operation->GetActivateResult(&activate_hr, &unk);
    if (SUCCEEDED(hr)) hr = activate_hr;
    if (SUCCEEDED(hr) && unk) hr = unk.As(&client_);
    result_ = hr;
    SetEvent(done_);
    return S_OK;
  }

  HRESULT Wait(IAudioClient** out, DWORD timeout_ms = 10000) {
    if (WaitForSingleObject(done_, timeout_ms) != WAIT_OBJECT_0) {
      return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    }
    if (FAILED(result_) || !client_) return FAILED(result_) ? result_ : E_FAIL;
    *out = client_.Detach();
    return S_OK;
  }

private:
  HANDLE done_ = nullptr;
  HRESULT result_ = E_FAIL;
  ComPtr<IAudioClient> client_;
};

void push_packet_to_ring(
  SourceSlot* slot,
  const BYTE* data,
  UINT32 frames,
  UINT32 channels,
  DWORD flags,
  bool is_float,
  UINT32 bits) {
  if (!slot || frames == 0) return;
  alignas(64) float tmp[4096 * 2];
  UINT32 remaining = frames;
  UINT32 offset = 0;
  while (remaining > 0) {
    const UINT32 chunk = (remaining > 4096) ? 4096 : remaining;
    const UINT32 samples = chunk * 2;  // always write stereo to ring
    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
      for (UINT32 i = 0; i < samples; ++i) tmp[i] = 0.0f;
    } else if (is_float && channels >= 1) {
      const float* f = reinterpret_cast<const float*>(data) + offset * channels;
      for (UINT32 i = 0; i < chunk; ++i) {
        const float l = f[i * channels];
        const float r = (channels > 1) ? f[i * channels + 1] : l;
        tmp[i * 2] = l;
        tmp[i * 2 + 1] = r;
      }
    } else if (bits == 16 && channels >= 1) {
      const int16_t* s = reinterpret_cast<const int16_t*>(data) + offset * channels;
      for (UINT32 i = 0; i < chunk; ++i) {
        const float l = static_cast<float>(s[i * channels]) / 32768.0f;
        const float r = (channels > 1) ? static_cast<float>(s[i * channels + 1]) / 32768.0f : l;
        tmp[i * 2] = l;
        tmp[i * 2 + 1] = r;
      }
    } else {
      for (UINT32 i = 0; i < samples; ++i) tmp[i] = 0.0f;
    }
    const auto written = slot->ring.write(tmp, samples);
    if (written < samples) {
      // overrun — drop remainder of this chunk
    }
    offset += chunk;
    remaining -= chunk;
  }
}

}  // namespace

WasapiCaptureSource::~WasapiCaptureSource() { stop(); }

bool WasapiCaptureSource::start_physical(IMMDevice* device, SourceSlot* slot, std::string& error) {
  if (!device || !slot) {
    error = "null device/slot";
    return false;
  }
  stop();
  slot_ = slot;
  stop_.store(false);
  device->AddRef();
  thread_ = std::thread([this, device]() {
    thread_main(Mode::Physical, device, 0);
    device->Release();
  });
  return true;
}

bool WasapiCaptureSource::start_system_loopback(IMMDevice* render_device, SourceSlot* slot, std::string& error) {
  if (!render_device || !slot) {
    error = "null device/slot";
    return false;
  }
  stop();
  slot_ = slot;
  stop_.store(false);
  render_device->AddRef();
  thread_ = std::thread([this, render_device]() {
    thread_main(Mode::SystemLoopback, render_device, 0);
    render_device->Release();
  });
  return true;
}

bool WasapiCaptureSource::start_process_loopback(uint32_t pid, SourceSlot* slot, std::string& error) {
  if (!slot || pid == 0) {
    error = "invalid process loopback request";
    return false;
  }
  stop();
  slot_ = slot;
  stop_.store(false);
  thread_ = std::thread([this, pid]() { thread_main(Mode::ProcessLoopback, nullptr, pid); });
  return true;
}

void WasapiCaptureSource::stop() {
  stop_.store(true);
  if (thread_.joinable()) thread_.join();
  running_.store(false);
  slot_ = nullptr;
}

void WasapiCaptureSource::thread_main(Mode mode, IMMDevice* device, uint32_t pid) {
  wasapi::ComScope com;
  running_.store(true);
  std::string error;
  bool ok = false;
  if (mode == Mode::ProcessLoopback) {
    ok = run_process_capture(pid, error);
  } else {
    ok = run_endpoint_capture(device, mode == Mode::SystemLoopback, error);
  }
  if (!ok) last_error_ = error;
  running_.store(false);
}

bool WasapiCaptureSource::run_endpoint_capture(IMMDevice* device, bool loopback, std::string& error) {
  IAudioClient* client = nullptr;
  HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void**>(&client));
  if (FAILED(hr) || !client) {
    error = "Activate IAudioClient " + wasapi::hr_hex(hr);
    return false;
  }

  auto want = wasapi::make_float_stereo(kEngineRate);
  WAVEFORMATEX* mix = nullptr;
  client->GetMixFormat(&mix);
  WAVEFORMATEX* use = &want.Format;
  HRESULT fmt = client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, use, nullptr);
  if (fmt == S_FALSE || FAILED(fmt)) use = mix;

  REFERENCE_TIME period = 0;
  client->GetDevicePeriod(&period, nullptr);
  DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  if (loopback) flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
  // Prefer engine format conversion when requesting non-mix format.
  if (use == &want.Format) {
    flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
  }

  hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, period, 0, use, nullptr);
  if (FAILED(hr)) {
    error = "capture Initialize " + wasapi::hr_hex(hr);
    if (mix) CoTaskMemFree(mix);
    client->Release();
    return false;
  }

  HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  client->SetEventHandle(event);
  IAudioCaptureClient* capture = nullptr;
  hr = client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&capture));
  if (FAILED(hr)) {
    error = "GetService capture " + wasapi::hr_hex(hr);
    CloseHandle(event);
    if (mix) CoTaskMemFree(mix);
    client->Release();
    return false;
  }

  const UINT32 channels = use->nChannels;
  const bool is_float = (use->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                        (use->wFormatTag == WAVE_FORMAT_EXTENSIBLE && use->wBitsPerSample == 32);
  const UINT32 bits = use->wBitsPerSample;

  DWORD task = 0;
  HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Audio", &task);
  client->Start();

  while (!stop_.load(std::memory_order_acquire)) {
    WaitForSingleObject(event, 50);
    UINT32 packet = 0;
    while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0) {
      BYTE* data = nullptr;
      UINT32 frames = 0;
      DWORD flags_pkt = 0;
      if (FAILED(capture->GetBuffer(&data, &frames, &flags_pkt, nullptr, nullptr))) break;
      push_packet_to_ring(slot_, data, frames, channels, flags_pkt, is_float, bits);
      capture->ReleaseBuffer(frames);
    }
  }

  client->Stop();
  if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
  capture->Release();
  CloseHandle(event);
  if (mix) CoTaskMemFree(mix);
  client->Release();
  return true;
}

bool WasapiCaptureSource::run_process_capture(uint32_t pid, std::string& error) {
  AUDIOCLIENT_ACTIVATION_PARAMS params{};
  params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
  params.ProcessLoopbackParams.TargetProcessId = pid;
  params.ProcessLoopbackParams.ProcessLoopbackMode =
    PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

  PROPVARIANT activate_params{};
  PropVariantInit(&activate_params);
  activate_params.vt = VT_BLOB;
  activate_params.blob.cbSize = sizeof(params);
  activate_params.blob.pBlobData = reinterpret_cast<BYTE*>(&params);

  ComPtr<ActivateHandler> handler = Make<ActivateHandler>();
  ComPtr<IActivateAudioInterfaceAsyncOperation> async_op;
  HRESULT hr = ActivateAudioInterfaceAsync(
    VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
    __uuidof(IAudioClient),
    &activate_params,
    handler.Get(),
    &async_op);
  if (FAILED(hr)) {
    error = "ActivateAudioInterfaceAsync " + wasapi::hr_hex(hr);
    return false;
  }

  IAudioClient* client = nullptr;
  hr = handler->Wait(&client);
  if (FAILED(hr) || !client) {
    error = "process activate wait " + wasapi::hr_hex(hr);
    return false;
  }

  auto format = wasapi::make_pcm16_stereo(kEngineRate);
  hr = client->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
      AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
    0,
    0,
    &format,
    nullptr);
  if (FAILED(hr)) {
    error = "process Initialize " + wasapi::hr_hex(hr);
    client->Release();
    return false;
  }

  HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  client->SetEventHandle(event);
  IAudioCaptureClient* capture = nullptr;
  hr = client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&capture));
  if (FAILED(hr)) {
    error = "process GetService " + wasapi::hr_hex(hr);
    CloseHandle(event);
    client->Release();
    return false;
  }

  DWORD task = 0;
  HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Audio", &task);
  client->Start();

  while (!stop_.load(std::memory_order_acquire)) {
    WaitForSingleObject(event, 50);
    UINT32 packet = 0;
    while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0) {
      BYTE* data = nullptr;
      UINT32 frames = 0;
      DWORD flags_pkt = 0;
      if (FAILED(capture->GetBuffer(&data, &frames, &flags_pkt, nullptr, nullptr))) break;
      push_packet_to_ring(slot_, data, frames, 2, flags_pkt, false, 16);
      capture->ReleaseBuffer(frames);
    }
  }

  client->Stop();
  if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
  capture->Release();
  CloseHandle(event);
  client->Release();
  return true;
}

WasapiRenderSink::~WasapiRenderSink() { close(); }

bool WasapiRenderSink::open(IMMDevice* device, std::string& error) {
  close();
  if (!device) {
    error = "null render device";
    return false;
  }
  device_ = device;
  device_->AddRef();

  HRESULT hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**>(&client_));
  if (FAILED(hr) || !client_) {
    error = "render Activate " + wasapi::hr_hex(hr);
    close();
    return false;
  }

  auto want = wasapi::make_float_stereo(kEngineRate);
  client_->GetMixFormat(&mix_);
  WAVEFORMATEX* use = &want.Format;
  HRESULT fmt = client_->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, use, nullptr);
  DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  bool using_float_want = true;
  if (fmt == S_FALSE || FAILED(fmt)) {
    use = mix_;
    using_float_want = false;
  } else {
    flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
  }

  REFERENCE_TIME period = 0;
  client_->GetDevicePeriod(&period, nullptr);
  hr = client_->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, period, 0, use, nullptr);
  if (FAILED(hr)) {
    error = "render Initialize " + wasapi::hr_hex(hr);
    close();
    return false;
  }

  event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  client_->SetEventHandle(event_);
  hr = client_->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&render_));
  if (FAILED(hr)) {
    error = "render GetService " + wasapi::hr_hex(hr);
    close();
    return false;
  }

  client_->GetBufferSize(&buffer_frames_);
  channels_ = use->nChannels;
  device_rate_ = use->nSamplesPerSec;
  write_float_ = using_float_want ||
                 (use->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                 (use->wFormatTag == WAVE_FORMAT_EXTENSIBLE && use->wBitsPerSample == 32);
  opened_ = true;
  return true;
}

void WasapiRenderSink::close() {
  if (render_) {
    render_->Release();
    render_ = nullptr;
  }
  if (client_) {
    client_->Release();
    client_ = nullptr;
  }
  if (event_) {
    CloseHandle(event_);
    event_ = nullptr;
  }
  if (mix_) {
    CoTaskMemFree(mix_);
    mix_ = nullptr;
  }
  if (device_) {
    device_->Release();
    device_ = nullptr;
  }
  opened_ = false;
}

bool WasapiRenderSink::run_loop(FillFn fill, void* user, std::atomic<bool>& stop_flag, std::string& error) {
  if (!opened_ || !client_ || !render_ || !fill) {
    error = "render sink not open";
    return false;
  }

  const bool write_float = write_float_;

  DWORD task = 0;
  HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task);
  HRESULT hr = client_->Start();
  if (FAILED(hr)) {
    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    error = "render Start " + wasapi::hr_hex(hr);
    return false;
  }

  std::vector<float> engine_buf(static_cast<size_t>(buffer_frames_) * 2u + 64u);

  while (!stop_flag.load(std::memory_order_acquire)) {
    WaitForSingleObject(event_, 50);
    UINT32 padding = 0;
    client_->GetCurrentPadding(&padding);
    const UINT32 frames = buffer_frames_ - padding;
    if (frames == 0) continue;

    if (!fill(user, engine_buf.data(), frames)) break;

    BYTE* data = nullptr;
    if (FAILED(render_->GetBuffer(frames, &data))) continue;

    if (write_float && channels_ >= 2) {
      float* f = reinterpret_cast<float*>(data);
      for (UINT32 i = 0; i < frames; ++i) {
        f[i * channels_] = engine_buf[i * 2];
        f[i * channels_ + 1] = engine_buf[i * 2 + 1];
        for (UINT32 c = 2; c < channels_; ++c) f[i * channels_ + c] = 0.0f;
      }
    } else if (write_float && channels_ == 1) {
      float* f = reinterpret_cast<float*>(data);
      for (UINT32 i = 0; i < frames; ++i) {
        f[i] = 0.5f * (engine_buf[i * 2] + engine_buf[i * 2 + 1]);
      }
    } else {
      auto* s16 = reinterpret_cast<int16_t*>(data);
      for (UINT32 i = 0; i < frames; ++i) {
        const float l = std::clamp(engine_buf[i * 2], -1.0f, 1.0f);
        const float r = std::clamp(engine_buf[i * 2 + 1], -1.0f, 1.0f);
        if (channels_ >= 2) {
          s16[i * channels_] = static_cast<int16_t>(l * 32767.0f);
          s16[i * channels_ + 1] = static_cast<int16_t>(r * 32767.0f);
          for (UINT32 c = 2; c < channels_; ++c) s16[i * channels_ + c] = 0;
        } else {
          const float m = 0.5f * (l + r);
          s16[i] = static_cast<int16_t>(m * 32767.0f);
        }
      }
    }
    render_->ReleaseBuffer(frames, 0);
  }

  client_->Stop();
  if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
  return true;
}

}  // namespace mixbridge
