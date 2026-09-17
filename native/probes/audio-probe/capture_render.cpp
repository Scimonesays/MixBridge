#include "wasapi_common.hpp"

#include <algorithm>

namespace {

WAVEFORMATEXTENSIBLE make_float_stereo(UINT32 rate) {
  WAVEFORMATEXTENSIBLE wfx{};
  wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  wfx.Format.nChannels = 2;
  wfx.Format.nSamplesPerSec = rate;
  wfx.Format.wBitsPerSample = 32;
  wfx.Format.nBlockAlign = 8;
  wfx.Format.nAvgBytesPerSec = rate * 8;
  wfx.Format.cbSize = 22;
  wfx.Samples.wValidBitsPerSample = 32;
  wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
  wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
  return wfx;
}

HRESULT activate_client(IMMDevice* device, IAudioClient** client) {
  return device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client));
}

}  // namespace

HRESULT capture_shared_seconds(
  IMMDevice* device,
  double seconds,
  UINT32 rate,
  std::vector<float>& interleaved,
  UINT32& out_channels,
  UINT32& out_rate) {
  IAudioClient* client = nullptr;
  HRESULT hr = activate_client(device, &client);
  if (FAILED(hr)) return hr;

  auto wfx = make_float_stereo(rate);
  WAVEFORMATEX* mix = nullptr;
  hr = client->GetMixFormat(&mix);
  if (FAILED(hr)) {
    client->Release();
    return hr;
  }

  // Prefer engine canonical format; fall back to mix format if needed.
  WAVEFORMATEX* use = &wfx.Format;
  hr = client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, use, nullptr);
  if (hr == S_FALSE || FAILED(hr)) {
    use = mix;
  }

  REFERENCE_TIME default_period = 0;
  REFERENCE_TIME min_period = 0;
  client->GetDevicePeriod(&default_period, &min_period);

  hr = client->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
    default_period,
    0,
    use,
    nullptr);
  if (FAILED(hr)) {
    CoTaskMemFree(mix);
    client->Release();
    return hr;
  }

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

  out_channels = use->nChannels;
  out_rate = use->nSamplesPerSec;
  const size_t expected = static_cast<size_t>(seconds * out_rate) * out_channels;
  interleaved.clear();
  interleaved.reserve(expected);

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
      hr = capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
      if (FAILED(hr)) break;
      const size_t samples = static_cast<size_t>(frames) * out_channels;
      if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
        interleaved.insert(interleaved.end(), samples, 0.0f);
      } else if (use->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                 (use->wFormatTag == WAVE_FORMAT_EXTENSIBLE && use->wBitsPerSample == 32)) {
        const float* f = reinterpret_cast<const float*>(data);
        interleaved.insert(interleaved.end(), f, f + samples);
      } else if (use->wBitsPerSample == 16) {
        const int16_t* s = reinterpret_cast<const int16_t*>(data);
        for (size_t i = 0; i < samples; ++i) {
          interleaved.push_back(static_cast<float>(s[i]) / 32768.0f);
        }
      } else {
        // Best-effort: treat as float
        const float* f = reinterpret_cast<const float*>(data);
        interleaved.insert(interleaved.end(), f, f + samples);
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

HRESULT render_tone_seconds(
  IMMDevice* device,
  double seconds,
  double freq_hz,
  float amplitude) {
  IAudioClient* client = nullptr;
  HRESULT hr = activate_client(device, &client);
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
    AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
    default_period,
    0,
    mix,
    nullptr);
  if (FAILED(hr)) {
    CoTaskMemFree(mix);
    client->Release();
    return hr;
  }

  HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  client->SetEventHandle(event);

  IAudioRenderClient* render = nullptr;
  hr = client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&render));
  if (FAILED(hr)) {
    CloseHandle(event);
    CoTaskMemFree(mix);
    client->Release();
    return hr;
  }

  UINT32 buffer_frames = 0;
  client->GetBufferSize(&buffer_frames);

  const UINT32 channels = mix->nChannels;
  const UINT32 rate = mix->nSamplesPerSec;
  const bool is_float = (mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                        (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mix->wBitsPerSample == 32);
  double phase = 0.0;
  const double phase_inc = 2.0 * 3.14159265358979323846 * freq_hz / static_cast<double>(rate);

  DWORD task_index = 0;
  HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);
  hr = client->Start();
  if (FAILED(hr)) {
    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    render->Release();
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
    UINT32 padding = 0;
    client->GetCurrentPadding(&padding);
    const UINT32 frames = buffer_frames - padding;
    if (frames == 0) continue;
    BYTE* data = nullptr;
    if (FAILED(render->GetBuffer(frames, &data))) continue;
    if (is_float) {
      float* f = reinterpret_cast<float*>(data);
      for (UINT32 i = 0; i < frames; ++i) {
        const float s = static_cast<float>(std::sin(phase) * amplitude);
        phase += phase_inc;
        for (UINT32 c = 0; c < channels; ++c) {
          f[i * channels + c] = s;
        }
      }
    } else {
      int16_t* s16 = reinterpret_cast<int16_t*>(data);
      for (UINT32 i = 0; i < frames; ++i) {
        const auto v = static_cast<int16_t>(std::sin(phase) * amplitude * 32767.0);
        phase += phase_inc;
        for (UINT32 c = 0; c < channels; ++c) {
          s16[i * channels + c] = v;
        }
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
  return S_OK;
}

int cmd_capture(double seconds, const std::wstring& wav_path) {
  IMMDevice* device = nullptr;
  HRESULT hr = get_default_device(eCapture, &device);
  if (FAILED(hr)) {
    print_hr("default capture device", hr);
    return 2;
  }
  std::vector<float> pcm;
  UINT32 ch = 0, rate = 0;
  hr = capture_shared_seconds(device, seconds, 48000, pcm, ch, rate);
  device->Release();
  if (FAILED(hr)) {
    print_hr("capture", hr);
    return 2;
  }
  const size_t frames = ch ? pcm.size() / ch : 0;
  auto stats = analyze_pcm_f32(pcm.data(), pcm.size(), ch, rate);
  std::printf("capture_ok=1 frames=%zu rate=%u ch=%u peak=%.6f rms=%.6f dominant_hz=%.2f\n",
              frames, rate, ch, stats.peak, stats.rms, stats.dominant_hz);
  if (!wav_path.empty()) {
    if (!write_wav_f32(wav_path, pcm.data(), frames, ch, rate)) {
      std::fprintf(stderr, "[error] failed writing wav\n");
      return 3;
    }
    std::printf("wrote=%s\n", wide_to_utf8(wav_path).c_str());
  }
  return 0;
}

int cmd_render(double seconds, double freq) {
  IMMDevice* device = nullptr;
  HRESULT hr = get_default_device(eRender, &device);
  if (FAILED(hr)) {
    print_hr("default render device", hr);
    return 2;
  }
  hr = render_tone_seconds(device, seconds, freq, 0.2f);
  device->Release();
  if (FAILED(hr)) {
    print_hr("render", hr);
    return 2;
  }
  std::printf("render_ok=1 seconds=%.3f freq=%.2f\n", seconds, freq);
  return 0;
}
