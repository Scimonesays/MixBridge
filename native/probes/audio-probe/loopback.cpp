#include "wasapi_common.hpp"

#include <atomic>
#include <thread>

#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/module.h>

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::FtmBase;
using Microsoft::WRL::Make;

namespace {

class ActivateHandler
  : public RuntimeClass<RuntimeClassFlags<ClassicCom>, FtmBase, IActivateAudioInterfaceCompletionHandler> {
public:
  ActivateHandler() {
    done_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  }
  ~ActivateHandler() override {
    if (done_event_) CloseHandle(done_event_);
  }

  STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation) override {
    HRESULT activate_hr = E_FAIL;
    ComPtr<IUnknown> unk;
    HRESULT hr = operation->GetActivateResult(&activate_hr, &unk);
    if (SUCCEEDED(hr)) hr = activate_hr;
    if (SUCCEEDED(hr) && unk) {
      hr = unk.As(&client_);
    }
    result_ = hr;
    SetEvent(done_event_);
    return S_OK;
  }

  HRESULT Wait(IAudioClient** out, DWORD timeout_ms = 10000) {
    const DWORD w = WaitForSingleObject(done_event_, timeout_ms);
    if (w != WAIT_OBJECT_0) return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    if (FAILED(result_)) return result_;
    if (!client_) return E_FAIL;
    *out = client_.Detach();
    return S_OK;
  }

private:
  HANDLE done_event_ = nullptr;
  HRESULT result_ = E_FAIL;
  ComPtr<IAudioClient> client_;
};

}  // namespace

HRESULT capture_system_loopback_seconds(
  IMMDevice* render_device,
  double seconds,
  UINT32 /*rate*/,
  std::vector<float>& interleaved,
  UINT32& out_channels,
  UINT32& out_rate) {
  IAudioClient* client = nullptr;
  HRESULT hr = render_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                       reinterpret_cast<void**>(&client));
  if (FAILED(hr)) return hr;

  WAVEFORMATEX* mix = nullptr;
  hr = client->GetMixFormat(&mix);
  if (FAILED(hr)) {
    client->Release();
    return hr;
  }

  REFERENCE_TIME default_period = 0;
  client->GetDevicePeriod(&default_period, nullptr);
  hr = client->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
    default_period,
    0,
    mix,
    nullptr);
  if (FAILED(hr)) {
    CoTaskMemFree(mix);
    client->Release();
    return hr;
  }

  // Re-create path through helper by duplicating capture loop with LOOPBACK already initialized.
  // Simpler: manually capture here.
  HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  client->SetEventHandle(event);
  IAudioCaptureClient* capture = nullptr;
  hr = client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&capture));
  if (FAILED(hr)) {
    CloseHandle(event);
    CoTaskMemFree(mix);
    client->Release();
    return hr;
  }

  out_channels = mix->nChannels;
  out_rate = mix->nSamplesPerSec;
  interleaved.clear();
  const bool is_float = (mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                        (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mix->wBitsPerSample == 32);

  DWORD task_index = 0;
  HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
  hr = client->Start();
  if (FAILED(hr)) {
    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    capture->Release();
    CloseHandle(event);
    CoTaskMemFree(mix);
    client->Release();
    return hr;
  }

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                          std::chrono::duration<double>(seconds));
  while (std::chrono::steady_clock::now() < deadline) {
    WaitForSingleObject(event, 50);
    UINT32 packet = 0;
    while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0) {
      BYTE* data = nullptr;
      UINT32 frames = 0;
      DWORD flags = 0;
      if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
      const size_t samples = static_cast<size_t>(frames) * out_channels;
      if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
        interleaved.insert(interleaved.end(), samples, 0.0f);
      } else if (is_float) {
        const float* f = reinterpret_cast<const float*>(data);
        interleaved.insert(interleaved.end(), f, f + samples);
      } else if (mix->wBitsPerSample == 16) {
        const int16_t* s = reinterpret_cast<const int16_t*>(data);
        for (size_t i = 0; i < samples; ++i) {
          interleaved.push_back(static_cast<float>(s[i]) / 32768.0f);
        }
      }
      capture->ReleaseBuffer(frames);
    }
  }

  client->Stop();
  if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
  capture->Release();
  CloseHandle(event);
  CoTaskMemFree(mix);
  client->Release();
  return S_OK;
}

HRESULT capture_process_loopback_seconds(
  DWORD pid,
  bool include_tree,
  double seconds,
  UINT32 rate,
  std::vector<float>& interleaved,
  UINT32& out_channels,
  UINT32& out_rate) {
  AUDIOCLIENT_ACTIVATION_PARAMS params{};
  params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
  params.ProcessLoopbackParams.TargetProcessId = pid;
  params.ProcessLoopbackParams.ProcessLoopbackMode = include_tree
    ? PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE
    : PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE;

  PROPVARIANT activate_params{};
  PropVariantInit(&activate_params);
  activate_params.vt = VT_BLOB;
  activate_params.blob.cbSize = sizeof(params);
  activate_params.blob.pBlobData = reinterpret_cast<BYTE*>(&params);

  ComPtr<ActivateHandler> handler = Make<ActivateHandler>();
  if (!handler) return E_OUTOFMEMORY;

  ComPtr<IActivateAudioInterfaceAsyncOperation> async_op;
  HRESULT hr = ActivateAudioInterfaceAsync(
    VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
    __uuidof(IAudioClient),
    &activate_params,
    handler.Get(),
    &async_op);
  if (FAILED(hr)) {
    std::fprintf(stderr, "[error] ActivateAudioInterfaceAsync hr=0x%08lX\n",
                 static_cast<unsigned long>(hr));
    return hr;
  }

  IAudioClient* client = nullptr;
  hr = handler->Wait(&client);
  if (FAILED(hr)) {
    std::fprintf(stderr, "[error] process loopback activate wait hr=0x%08lX\n",
                 static_cast<unsigned long>(hr));
    return hr;
  }

  // Match Microsoft ApplicationLoopback sample: LOOPBACK + EVENTCALLBACK + AUTOCONVERTPCM
  // with an explicit PCM format (GetMixFormat is optional for process loopback).
  WAVEFORMATEX capture_format{};
  capture_format.wFormatTag = WAVE_FORMAT_PCM;
  capture_format.nChannels = 2;
  capture_format.nSamplesPerSec = rate ? rate : 48000;
  capture_format.wBitsPerSample = 16;
  capture_format.nBlockAlign =
    static_cast<WORD>(capture_format.nChannels * capture_format.wBitsPerSample / 8);
  capture_format.nAvgBytesPerSec =
    capture_format.nSamplesPerSec * capture_format.nBlockAlign;
  capture_format.cbSize = 0;

  hr = client->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
      AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
    0,
    0,
    &capture_format,
    nullptr);
  if (FAILED(hr)) {
    std::fprintf(stderr, "[error] process loopback Initialize hr=0x%08lX\n",
                 static_cast<unsigned long>(hr));
    client->Release();
    return hr;
  }

  HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  client->SetEventHandle(event);
  IAudioCaptureClient* capture = nullptr;
  hr = client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&capture));
  if (FAILED(hr)) {
    CloseHandle(event);
    client->Release();
    return hr;
  }

  out_channels = capture_format.nChannels;
  out_rate = capture_format.nSamplesPerSec;
  interleaved.clear();
  const bool is_float = false;

  DWORD task_index = 0;
  HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
  hr = client->Start();
  if (FAILED(hr)) {
    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    capture->Release();
    CloseHandle(event);
    client->Release();
    return hr;
  }

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                          std::chrono::duration<double>(seconds));
  while (std::chrono::steady_clock::now() < deadline) {
    WaitForSingleObject(event, 50);
    UINT32 packet = 0;
    while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0) {
      BYTE* data = nullptr;
      UINT32 frames = 0;
      DWORD flags = 0;
      if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
      const size_t samples = static_cast<size_t>(frames) * out_channels;
      if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
        interleaved.insert(interleaved.end(), samples, 0.0f);
      } else if (is_float) {
        const float* f = reinterpret_cast<const float*>(data);
        interleaved.insert(interleaved.end(), f, f + samples);
      } else {
        const int16_t* s = reinterpret_cast<const int16_t*>(data);
        for (size_t i = 0; i < samples; ++i) {
          interleaved.push_back(static_cast<float>(s[i]) / 32768.0f);
        }
      }
      capture->ReleaseBuffer(frames);
    }
  }

  client->Stop();
  if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
  capture->Release();
  CloseHandle(event);
  client->Release();
  return S_OK;
}

int cmd_system_loopback(double seconds, const std::wstring& wav_path) {
  IMMDevice* device = nullptr;
  HRESULT hr = get_default_device(eRender, &device);
  if (FAILED(hr)) {
    print_hr("default render for loopback", hr);
    return 2;
  }

  // Play a known tone concurrently so loopback is not silence on quiet systems.
  std::atomic<bool> render_done{false};
  HRESULT render_hr = S_OK;
  std::thread tone([&]() {
    render_hr = render_tone_seconds(device, seconds, 1000.0, 0.25f);
    render_done = true;
  });

  std::vector<float> pcm;
  UINT32 ch = 0, rate = 0;
  // Small delay so render starts first.
  Sleep(50);
  hr = capture_system_loopback_seconds(device, seconds, 48000, pcm, ch, rate);
  tone.join();
  device->Release();

  if (FAILED(hr)) {
    print_hr("system loopback", hr);
    return 2;
  }
  if (FAILED(render_hr)) {
    print_hr("concurrent render", render_hr);
    return 2;
  }

  const size_t frames = ch ? pcm.size() / ch : 0;
  auto stats = analyze_pcm_f32(pcm.data(), pcm.size(), ch, rate);
  std::printf("system_loopback_ok=1 frames=%zu rate=%u ch=%u peak=%.6f rms=%.6f dominant_hz=%.2f\n",
              frames, rate, ch, stats.peak, stats.rms, stats.dominant_hz);
  if (!wav_path.empty()) {
    write_wav_f32(wav_path, pcm.data(), frames, ch, rate);
    std::printf("wrote=%s\n", wide_to_utf8(wav_path).c_str());
  }
  // Expect energy near 1000 Hz when speakers/headphones path is active.
  if (stats.rms < 1e-4) {
    std::fprintf(stderr, "[warn] system loopback captured near-silence (device may be muted)\n");
    return 4;
  }
  return 0;
}

int cmd_process_loopback(DWORD pid, double seconds, const std::wstring& wav_path) {
  std::vector<float> pcm;
  UINT32 ch = 0, rate = 0;
  HRESULT hr = capture_process_loopback_seconds(pid, true, seconds, 48000, pcm, ch, rate);
  if (FAILED(hr)) {
    print_hr("process loopback", hr);
    return 2;
  }
  const size_t frames = ch ? pcm.size() / ch : 0;
  auto stats = analyze_pcm_f32(pcm.data(), pcm.size(), ch, rate);
  std::printf("process_loopback_ok=1 pid=%lu frames=%zu rate=%u ch=%u peak=%.6f rms=%.6f dominant_hz=%.2f\n",
              static_cast<unsigned long>(pid), frames, rate, ch, stats.peak, stats.rms, stats.dominant_hz);
  if (!wav_path.empty()) {
    write_wav_f32(wav_path, pcm.data(), frames, ch, rate);
    std::printf("wrote=%s\n", wide_to_utf8(wav_path).c_str());
  }
  return 0;
}
