#include <audioclient.h>
#include <mmdeviceapi.h>
#include <avrt.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

struct ComInit {
  ComInit() { CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
  ~ComInit() { CoUninitialize(); }
};

int main(int argc, char** argv) {
  ComInit com;
  double seconds = 5.0;
  double freq = 440.0;
  float amp = 0.2f;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) seconds = std::atof(argv[++i]);
    else if (std::strcmp(argv[i], "--freq") == 0 && i + 1 < argc) freq = std::atof(argv[++i]);
    else if (std::strcmp(argv[i], "--amp") == 0 && i + 1 < argc) amp = static_cast<float>(std::atof(argv[++i]));
  }

  IMMDeviceEnumerator* enumerator = nullptr;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
  if (FAILED(hr)) return 2;
  IMMDevice* device = nullptr;
  hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
  enumerator->Release();
  if (FAILED(hr)) return 2;

  IAudioClient* client = nullptr;
  hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client));
  device->Release();
  if (FAILED(hr)) return 2;

  WAVEFORMATEX* mix = nullptr;
  hr = client->GetMixFormat(&mix);
  if (FAILED(hr)) {
    client->Release();
    return 2;
  }

  REFERENCE_TIME period = 0;
  client->GetDevicePeriod(&period, nullptr);
  hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period, 0, mix, nullptr);
  if (FAILED(hr)) {
    CoTaskMemFree(mix);
    client->Release();
    return 2;
  }

  HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  client->SetEventHandle(event);
  IAudioRenderClient* render = nullptr;
  hr = client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&render));
  if (FAILED(hr)) {
    CloseHandle(event);
    CoTaskMemFree(mix);
    client->Release();
    return 2;
  }

  UINT32 buffer_frames = 0;
  client->GetBufferSize(&buffer_frames);
  const UINT32 channels = mix->nChannels;
  const UINT32 rate = mix->nSamplesPerSec;
  const bool is_float = (mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                        (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mix->wBitsPerSample == 32);
  double phase = 0.0;
  const double inc = 2.0 * 3.14159265358979323846 * freq / rate;

  DWORD task = 0;
  HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task);
  client->Start();
  std::printf("mb-tone-player pid=%lu freq=%.2f seconds=%.2f\n",
              static_cast<unsigned long>(GetCurrentProcessId()), freq, seconds);

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                          std::chrono::duration<double>(seconds));
  while (std::chrono::steady_clock::now() < deadline) {
    WaitForSingleObject(event, 50);
    UINT32 padding = 0;
    client->GetCurrentPadding(&padding);
    const UINT32 frames = buffer_frames - padding;
    if (!frames) continue;
    BYTE* data = nullptr;
    if (FAILED(render->GetBuffer(frames, &data))) continue;
    if (is_float) {
      float* f = reinterpret_cast<float*>(data);
      for (UINT32 i = 0; i < frames; ++i) {
        const float s = static_cast<float>(std::sin(phase) * amp);
        phase += inc;
        for (UINT32 c = 0; c < channels; ++c) f[i * channels + c] = s;
      }
    } else {
      auto* s16 = reinterpret_cast<int16_t*>(data);
      for (UINT32 i = 0; i < frames; ++i) {
        const auto v = static_cast<int16_t>(std::sin(phase) * amp * 32767.0);
        phase += inc;
        for (UINT32 c = 0; c < channels; ++c) s16[i * channels + c] = v;
      }
    }
    render->ReleaseBuffer(frames, 0);
  }

  client->Stop();
  if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
  render->Release();
  CloseHandle(event);
  CoTaskMemFree(mix);
  client->Release();
  return 0;
}
