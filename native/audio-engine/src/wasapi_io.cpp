#include "mixbridge/wasapi_io.hpp"
#include "mixbridge/wasapi_util.hpp"

#include <wrl/client.h>
#include <wrl/implements.h>

#include <ksmedia.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::FtmBase;
using Microsoft::WRL::Make;

namespace mixbridge {
namespace {

// Skip Windows audio enhancements. Those APOs sit outside the reported
// buffer size and are a common reason a guitar round trip feels late.
void prefer_raw_stream(IAudioClient* client) {
  ComPtr<IAudioClient2> client2;
  if (FAILED(client->QueryInterface(IID_PPV_ARGS(&client2))) || !client2) return;
  AudioClientProperties props{};
  props.cbSize = sizeof(props);
  props.bIsOffload = FALSE;
  props.eCategory = AudioCategory_Media;
  props.Options = AUDCLNT_STREAMOPTIONS_RAW;
  client2->SetClientProperties(&props);
}

// Windows 10+ low-latency shared-mode path. IAudioClient3 lets a client request
// the device/audio-engine's minimum supported shared period (often far smaller
// than the legacy default period). If a driver or endpoint rejects it, callers
// transparently fall back to IAudioClient::Initialize.
// A failed Initialize retires the client; the caller must Activate a new one.
HRESULT initialize_shared_stream_low_latency(
  IAudioClient* client,
  DWORD flags,
  const WAVEFORMATEX* format,
  REFERENCE_TIME fallback_period,
  UINT32* selected_period_frames = nullptr,
  bool raw = true) {
  if (selected_period_frames) *selected_period_frames = 0;
  if (!client || !format) return E_POINTER;
  if (raw) prefer_raw_stream(client);

  ComPtr<IAudioClient3> client3;
  HRESULT hr3 = client->QueryInterface(IID_PPV_ARGS(&client3));
  if (SUCCEEDED(hr3) && client3) {
    UINT32 default_frames = 0;
    UINT32 fundamental_frames = 0;
    UINT32 min_frames = 0;
    UINT32 max_frames = 0;
    hr3 = client3->GetSharedModeEnginePeriod(
      format,
      &default_frames,
      &fundamental_frames,
      &min_frames,
      &max_frames);
    if (SUCCEEDED(hr3) && min_frames > 0) {
      hr3 = client3->InitializeSharedAudioStream(flags, min_frames, format, nullptr);
      if (SUCCEEDED(hr3)) {
        if (selected_period_frames) *selected_period_frames = min_frames;
        return hr3;
      }
    }
  }

  return client->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    flags,
    fallback_period,
    0,
    format,
    nullptr);
}

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

// Streaming linear resampler. Holds one frame of history so a 44.1 kHz
// endpoint can feed the 48 kHz graph without the Windows quality converter,
// which adds a long delay around the guitar.
uint32_t resample_stereo(
  double& pos,
  float& hist_l,
  float& hist_r,
  float* hold,
  uint32_t& hold_frames,
  const float* in,
  uint32_t in_frames,
  float* out,
  uint32_t out_cap,
  double in_per_out) {
  uint32_t total = hold_frames + in_frames;
  if (total > 4128) total = 4128;
  float joined[4128 * 2];
  if (hold_frames) {
    std::memcpy(joined, hold, static_cast<std::size_t>(hold_frames) * 2u * sizeof(float));
  }
  const uint32_t copy_in = total > hold_frames ? total - hold_frames : 0;
  if (copy_in && in) {
    std::memcpy(joined + static_cast<std::size_t>(hold_frames) * 2u, in,
                static_cast<std::size_t>(copy_in) * 2u * sizeof(float));
  }
  const float* src = joined;
  uint32_t n = 0;
  while (n < out_cap && total > 0) {
    const int i0 = static_cast<int>(std::floor(pos));
    if (i0 + 1 >= static_cast<int>(total)) break;
    const float frac = static_cast<float>(pos - static_cast<double>(i0));
    const float a = 1.0f - frac;
    auto at = [&](int index, int ch) -> float {
      if (index < 0) return ch ? hist_r : hist_l;
      return src[static_cast<uint32_t>(index) * 2u + static_cast<uint32_t>(ch)];
    };
    out[n * 2] = at(i0, 0) * a + at(i0 + 1, 0) * frac;
    out[n * 2 + 1] = at(i0, 1) * a + at(i0 + 1, 1) * frac;
    ++n;
    pos += in_per_out;
  }
  int consumed = static_cast<int>(std::floor(pos));
  if (consumed < 0) consumed = 0;
  if (consumed > static_cast<int>(total)) consumed = static_cast<int>(total);
  if (consumed > 0) {
    hist_l = src[(static_cast<uint32_t>(consumed) - 1u) * 2u];
    hist_r = src[(static_cast<uint32_t>(consumed) - 1u) * 2u + 1u];
  }
  pos -= static_cast<double>(consumed);
  uint32_t left = total - static_cast<uint32_t>(consumed);
  if (left > 32) left = 32;
  hold_frames = left;
  if (left) {
    std::memcpy(hold, src + static_cast<std::size_t>(consumed) * 2u,
                static_cast<std::size_t>(left) * 2u * sizeof(float));
  }
  return n;
}

void push_packet_to_ring(
  SourceSlot* slot,
  const BYTE* data,
  UINT32 frames,
  UINT32 channels,
  int32_t selected_channel,
  DWORD flags,
  bool is_float,
  UINT32 bits,
  uint32_t device_rate = 0,
  double* rate_pos = nullptr,
  float* hist_l = nullptr,
  float* hist_r = nullptr,
  float* hold = nullptr,
  uint32_t* hold_frames = nullptr) {
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
        if (selected_channel >= 0 && static_cast<UINT32>(selected_channel) < channels) {
          const float mono = f[i * channels + static_cast<UINT32>(selected_channel)];
          tmp[i * 2] = mono;
          tmp[i * 2 + 1] = mono;
        } else if (selected_channel >= 0) {
          tmp[i * 2] = 0.0f;
          tmp[i * 2 + 1] = 0.0f;
        } else {
          const float l = f[i * channels];
          const float r = (channels > 1) ? f[i * channels + 1] : l;
          tmp[i * 2] = l;
          tmp[i * 2 + 1] = r;
        }
      }
    } else if (bits == 32 && channels >= 1) {
      const int32_t* s = reinterpret_cast<const int32_t*>(data) + offset * channels;
      for (UINT32 i = 0; i < chunk; ++i) {
        auto sample = [&](UINT32 ch) -> float {
          return static_cast<float>(s[i * channels + ch]) / 2147483648.0f;
        };
        if (selected_channel >= 0 && static_cast<UINT32>(selected_channel) < channels) {
          const float mono = sample(static_cast<UINT32>(selected_channel));
          tmp[i * 2] = mono;
          tmp[i * 2 + 1] = mono;
        } else if (selected_channel >= 0) {
          tmp[i * 2] = 0.0f;
          tmp[i * 2 + 1] = 0.0f;
        } else {
          const float l = sample(0);
          const float r = (channels > 1) ? sample(1) : l;
          tmp[i * 2] = l;
          tmp[i * 2 + 1] = r;
        }
      }
    } else if (bits == 16 && channels >= 1) {
      const int16_t* s = reinterpret_cast<const int16_t*>(data) + offset * channels;
      for (UINT32 i = 0; i < chunk; ++i) {
        if (selected_channel >= 0 && static_cast<UINT32>(selected_channel) < channels) {
          const float mono =
            static_cast<float>(s[i * channels + static_cast<UINT32>(selected_channel)]) / 32768.0f;
          tmp[i * 2] = mono;
          tmp[i * 2 + 1] = mono;
        } else if (selected_channel >= 0) {
          tmp[i * 2] = 0.0f;
          tmp[i * 2 + 1] = 0.0f;
        } else {
          const float l = static_cast<float>(s[i * channels]) / 32768.0f;
          const float r =
            (channels > 1) ? static_cast<float>(s[i * channels + 1]) / 32768.0f : l;
          tmp[i * 2] = l;
          tmp[i * 2 + 1] = r;
        }
      }
    } else {
      for (UINT32 i = 0; i < samples; ++i) tmp[i] = 0.0f;
    }
    const bool convert = device_rate != 0 && device_rate != kEngineRate && rate_pos && hist_l &&
                         hist_r && hold && hold_frames;
    if (convert) {
      alignas(64) float eng[8000 * 2];
      const double in_per_out =
        static_cast<double>(device_rate) / static_cast<double>(kEngineRate);
      const uint32_t got = resample_stereo(
        *rate_pos, *hist_l, *hist_r, hold, *hold_frames, tmp, chunk, eng, 8000, in_per_out);
      if (got > 0) slot->ring.write(eng, static_cast<std::size_t>(got) * 2u);
    } else {
      slot->ring.write(tmp, samples);
    }
    offset += chunk;
    remaining -= chunk;
  }
}

}  // namespace

WasapiCaptureSource::~WasapiCaptureSource() { stop(); }

bool WasapiCaptureSource::start_physical(
  IMMDevice* device,
  SourceSlot* slot,
  int32_t input_channel,
  std::string& error) {
  if (!device || !slot) {
    error = "null device/slot";
    return false;
  }
  stop();
  slot_ = slot;
  input_channel_ = input_channel;
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
  input_channel_ = -1;
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
  input_channel_ = -1;
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

bool is_default_console_capture(IMMDevice* device) {
  if (!device) return false;
  ComPtr<IMMDeviceEnumerator> enumerator;
  if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              IID_PPV_ARGS(&enumerator)))) {
    return false;
  }
  ComPtr<IMMDevice> def;
  if (FAILED(enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &def)) || !def) return false;
  LPWSTR a = nullptr;
  LPWSTR b = nullptr;
  device->GetId(&a);
  def->GetId(&b);
  const bool same = a && b && CompareStringOrdinal(a, -1, b, -1, TRUE) == CSTR_EQUAL;
  if (a) CoTaskMemFree(a);
  if (b) CoTaskMemFree(b);
  return same;
}

// 24-bit samples in a 32-bit container. Shared mode on this Fireface stays at
// 22 ms. Exclusive mode can go down to 3 ms, but that block is not an even
// 48 kHz count and Guitar Rig cannot finish inside it, so the note breaks up.
// 10 ms is 441 frames at 44.1 kHz, which is exactly 480 frames at 48 kHz.
bool open_exclusive_client(IMMDevice* device, IAudioClient** out, WAVEFORMATEXTENSIBLE* fmt) {
  *out = nullptr;
  const REFERENCE_TIME kSteadyPeriod = 100000;  // 10 ms
  REFERENCE_TIME durations[2] = {kSteadyPeriod, 30000};
  for (REFERENCE_TIME dur : durations) {
    IAudioClient* client = nullptr;
    HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(&client));
    if (FAILED(hr) || !client) return false;
    WAVEFORMATEX* mix = nullptr;
    client->GetMixFormat(&mix);
    const DWORD rate = (mix && mix->nSamplesPerSec) ? mix->nSamplesPerSec : 44100;
    if (mix) CoTaskMemFree(mix);
    if (dur < kSteadyPeriod) {
      REFERENCE_TIME shared_period = 0;
      REFERENCE_TIME exclusive_period = 0;
      client->GetDevicePeriod(&shared_period, &exclusive_period);
      if (exclusive_period >= 10000) dur = exclusive_period;
    }

    *fmt = {};
    fmt->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    fmt->Format.nChannels = 2;
    fmt->Format.nSamplesPerSec = rate;
    fmt->Format.wBitsPerSample = 32;
    fmt->Format.nBlockAlign = 8;
    fmt->Format.nAvgBytesPerSec = rate * 8;
    fmt->Format.cbSize = static_cast<WORD>(sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX));
    fmt->Samples.wValidBitsPerSample = 24;
    fmt->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    fmt->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                            dur, dur, &fmt->Format, nullptr);
    if (SUCCEEDED(hr)) {
      *out = client;
      return true;
    }
    client->Release();
  }
  return false;
}

bool WasapiCaptureSource::run_endpoint_capture(IMMDevice* device, bool loopback, std::string& error) {
  WAVEFORMATEXTENSIBLE exclusive_fmt{};
  IAudioClient* client = nullptr;
  WAVEFORMATEX* mix = nullptr;
  WAVEFORMATEX* use = nullptr;
  const bool exclusive = !loopback && !is_default_console_capture(device) &&
                         open_exclusive_client(device, &client, &exclusive_fmt);
  HRESULT hr = S_OK;
  if (exclusive) {
    use = &exclusive_fmt.Format;
  } else {
  hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                        reinterpret_cast<void**>(&client));
  if (FAILED(hr) || !client) {
    error = "Activate IAudioClient " + wasapi::hr_hex(hr);
    return false;
  }

  auto want = wasapi::make_float_stereo(kEngineRate);
  client->GetMixFormat(&mix);
  // Open the mix format. IAudioClient3's 10 ms period is only offered there.
  // A 48 kHz request falls back to the ~22 ms buffer and, with the Windows
  // converter, adds a long delay around the guitar. We resample at the edge.
  REFERENCE_TIME period = 0;
  client->GetDevicePeriod(&period, nullptr);
  DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  if (loopback) flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;

  UINT32 selected_period_frames = 0;
  use = mix ? mix : &want.Format;
  hr = initialize_shared_stream_low_latency(
    client, flags, use, period, &selected_period_frames, true);
  if (FAILED(hr)) {
    client->Release();
    client = nullptr;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(&client));
    if (FAILED(hr) || !client) {
      error = "capture reactivate " + wasapi::hr_hex(hr);
      if (mix) CoTaskMemFree(mix);
      return false;
    }
    hr = initialize_shared_stream_low_latency(
      client, flags, use, period, &selected_period_frames, false);
  }
  if (FAILED(hr)) {
    client->Release();
    client = nullptr;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(&client));
    if (FAILED(hr) || !client) {
      error = "capture reactivate " + wasapi::hr_hex(hr);
      if (mix) CoTaskMemFree(mix);
      return false;
    }
    use = &want.Format;
    flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
    hr = initialize_shared_stream_low_latency(
      client, flags, use, period, &selected_period_frames, true);
  }
  if (FAILED(hr)) {
    error = "capture Initialize " + wasapi::hr_hex(hr);
    if (mix) CoTaskMemFree(mix);
    client->Release();
    return false;
  }

  // RAW or a 48 kHz request can succeed and still land on the ~22 ms buffer.
  // If that happened on the mix format, reopen once without RAW.
  UINT32 sized = 0;
  client->GetBufferSize(&sized);
  const uint32_t opened_rate = use->nSamplesPerSec ? use->nSamplesPerSec : kEngineRate;
  if (mix && use == mix && sized * 1000u > opened_rate * 15u) {
    client->Release();
    client = nullptr;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(&client));
    if (FAILED(hr) || !client) {
      error = "capture short-period reactivate " + wasapi::hr_hex(hr);
      CoTaskMemFree(mix);
      return false;
    }
    flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (loopback) flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
    use = mix;
    hr = initialize_shared_stream_low_latency(
      client, flags, use, period, &selected_period_frames, false);
    if (FAILED(hr)) {
      error = "capture short-period Initialize " + wasapi::hr_hex(hr);
      CoTaskMemFree(mix);
      client->Release();
      return false;
    }
  }
  }  // shared-mode fallback

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
  // Exclusive is 24-bit PCM packed in 32-bit words. The mix format is float,
  // and that one also reports extensible + 32 bits, so the tag alone is not enough.
  const bool is_float = !exclusive && ((use->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                                       (use->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                                        use->wBitsPerSample == 32));
  const UINT32 bits = use->wBitsPerSample;
  capture_rate_ = use->nSamplesPerSec;
  rate_pos_ = 0.0;
  rate_hist_l_ = 0.0f;
  rate_hist_r_ = 0.0f;
  rate_hold_frames_ = 0;

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
      push_packet_to_ring(
        slot_,
        data,
        frames,
        channels,
        loopback ? -1 : input_channel_,
        flags_pkt,
        is_float,
        bits,
        capture_rate_,
        &rate_pos_,
        &rate_hist_l_,
        &rate_hist_r_,
        rate_hold_,
        &rate_hold_frames_);
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
  // No SRC_DEFAULT_QUALITY: that converter is what makes app capture late.
  hr = initialize_shared_stream_low_latency(
    client,
    AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
      AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
    &format,
    0,
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
      push_packet_to_ring(slot_, data, frames, 2, -1, flags_pkt, false, 16);
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
  exclusive_ = false;
  write_pcm32_ = false;

  WAVEFORMATEXTENSIBLE exclusive_fmt{};
  IAudioClient* exclusive_client = nullptr;
  if (open_exclusive_client(device_, &exclusive_client, &exclusive_fmt)) {
    client_ = exclusive_client;
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HRESULT exclusive_hr = client_->SetEventHandle(event_);
    if (SUCCEEDED(exclusive_hr)) {
      exclusive_hr = client_->GetService(__uuidof(IAudioRenderClient),
                                         reinterpret_cast<void**>(&render_));
    }
    if (FAILED(exclusive_hr) || !render_) {
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
    } else {
      client_->GetBufferSize(&buffer_frames_);
      channels_ = exclusive_fmt.Format.nChannels;
      device_rate_ = exclusive_fmt.Format.nSamplesPerSec;
      write_float_ = false;
      write_pcm32_ = true;
      exclusive_ = true;
      rate_pos_ = 0.0;
      rate_hist_l_ = 0.0f;
      rate_hist_r_ = 0.0f;
      rate_hold_frames_ = 0;
      opened_ = true;
      return true;
    }
  }

  HRESULT hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**>(&client_));
  if (FAILED(hr) || !client_) {
    error = "render Activate " + wasapi::hr_hex(hr);
    close();
    return false;
  }

  auto want = wasapi::make_float_stereo(kEngineRate);
  client_->GetMixFormat(&mix_);
  // Mix format first so the shared stream can take the 10 ms period.
  REFERENCE_TIME period = 0;
  client_->GetDevicePeriod(&period, nullptr);
  DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  WAVEFORMATEX* use = mix_ ? mix_ : &want.Format;
  bool using_float_want = false;
  UINT32 selected_period_frames = 0;
  hr = initialize_shared_stream_low_latency(
    client_, flags, use, period, &selected_period_frames, true);
  if (FAILED(hr)) {
    client_->Release();
    client_ = nullptr;
    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                           reinterpret_cast<void**>(&client_));
    if (FAILED(hr) || !client_) {
      error = "render reactivate " + wasapi::hr_hex(hr);
      close();
      return false;
    }
    hr = initialize_shared_stream_low_latency(
      client_, flags, use, period, &selected_period_frames, false);
  }
  if (FAILED(hr)) {
    client_->Release();
    client_ = nullptr;
    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                           reinterpret_cast<void**>(&client_));
    if (FAILED(hr) || !client_) {
      error = "render reactivate " + wasapi::hr_hex(hr);
      close();
      return false;
    }
    use = &want.Format;
    using_float_want = true;
    flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
    hr = initialize_shared_stream_low_latency(
      client_, flags, use, period, &selected_period_frames, true);
  }
  if (FAILED(hr)) {
    error = "render Initialize " + wasapi::hr_hex(hr);
    close();
    return false;
  }

  UINT32 sized = 0;
  client_->GetBufferSize(&sized);
  const uint32_t opened_rate = use->nSamplesPerSec ? use->nSamplesPerSec : kEngineRate;
  if (mix_ && use == mix_ && sized * 1000u > opened_rate * 15u) {
    client_->Release();
    client_ = nullptr;
    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                           reinterpret_cast<void**>(&client_));
    if (FAILED(hr) || !client_) {
      error = "render short-period reactivate " + wasapi::hr_hex(hr);
      close();
      return false;
    }
    flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    use = mix_;
    using_float_want = false;
    hr = initialize_shared_stream_low_latency(
      client_, flags, use, period, &selected_period_frames, false);
    if (FAILED(hr)) {
      error = "render short-period Initialize " + wasapi::hr_hex(hr);
      close();
      return false;
    }
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
  rate_pos_ = 0.0;
  rate_hist_l_ = 0.0f;
  rate_hist_r_ = 0.0f;
  rate_hold_frames_ = 0;
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
  const bool write_pcm32 = write_pcm32_;
  const bool exclusive = exclusive_;

  const bool convert = device_rate_ != 0 && device_rate_ != kEngineRate;
  const double engine_per_device = convert
    ? static_cast<double>(kEngineRate) / static_cast<double>(device_rate_)
    : 1.0;
  std::vector<float> engine_buf(static_cast<size_t>(buffer_frames_) * 4u + 64u);
  std::vector<float> device_buf(static_cast<size_t>(buffer_frames_) * 2u + 16u);

  auto write_block = [&](UINT32 frames) -> bool {
    uint32_t engine_frames = frames;
    if (convert) {
      const double last_pos =
        rate_pos_ + static_cast<double>(frames > 0 ? frames - 1 : 0) * engine_per_device;
      const int need_total = static_cast<int>(std::floor(last_pos)) + 2;
      int ask = need_total - static_cast<int>(rate_hold_frames_);
      if (ask < 1) ask = 1;
      engine_frames = static_cast<uint32_t>(ask);
    }
    if (engine_frames * 2u > engine_buf.size()) {
      engine_frames = static_cast<uint32_t>(engine_buf.size() / 2u);
    }
    if (!fill(user, engine_buf.data(), engine_frames)) return false;

    float* play = engine_buf.data();
    if (convert) {
      const uint32_t got = resample_stereo(
        rate_pos_, rate_hist_l_, rate_hist_r_, rate_hold_, rate_hold_frames_,
        engine_buf.data(), engine_frames, device_buf.data(), frames, engine_per_device);
      if (got < frames) {
        const float hold_l = got > 0 ? device_buf[(got - 1) * 2] : 0.0f;
        const float hold_r = got > 0 ? device_buf[(got - 1) * 2 + 1] : 0.0f;
        for (uint32_t i = got; i < frames; ++i) {
          device_buf[i * 2] = hold_l;
          device_buf[i * 2 + 1] = hold_r;
        }
      }
      play = device_buf.data();
    }

    BYTE* data = nullptr;
    if (FAILED(render_->GetBuffer(frames, &data))) return true;

    if (write_pcm32 && channels_ >= 1) {
      auto* s32 = reinterpret_cast<int32_t*>(data);
      for (UINT32 i = 0; i < frames; ++i) {
        const float l = std::clamp(play[i * 2], -1.0f, 1.0f);
        const float r = std::clamp(play[i * 2 + 1], -1.0f, 1.0f);
        if (channels_ >= 2) {
          s32[i * channels_] = static_cast<int32_t>(l * 2147483647.0f);
          s32[i * channels_ + 1] = static_cast<int32_t>(r * 2147483647.0f);
          for (UINT32 c = 2; c < channels_; ++c) s32[i * channels_ + c] = 0;
        } else {
          s32[i] = static_cast<int32_t>(0.5f * (l + r) * 2147483647.0f);
        }
      }
    } else if (write_float && channels_ >= 2) {
      float* f = reinterpret_cast<float*>(data);
      for (UINT32 i = 0; i < frames; ++i) {
        f[i * channels_] = play[i * 2];
        f[i * channels_ + 1] = play[i * 2 + 1];
        for (UINT32 c = 2; c < channels_; ++c) f[i * channels_ + c] = 0.0f;
      }
    } else if (write_float && channels_ == 1) {
      float* f = reinterpret_cast<float*>(data);
      for (UINT32 i = 0; i < frames; ++i) {
        f[i] = 0.5f * (play[i * 2] + play[i * 2 + 1]);
      }
    } else {
      auto* s16 = reinterpret_cast<int16_t*>(data);
      for (UINT32 i = 0; i < frames; ++i) {
        const float l = std::clamp(play[i * 2], -1.0f, 1.0f);
        const float r = std::clamp(play[i * 2 + 1], -1.0f, 1.0f);
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
    return true;
  };

  DWORD task = 0;
  HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task);
  // Exclusive mode plays the buffer we hand it before Start. One period of
  // silence (or the current mix) keeps the first callback from underrunning.
  if (exclusive) {
    if (!write_block(buffer_frames_)) {
      if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
      return false;
    }
  }
  HRESULT hr = client_->Start();
  if (FAILED(hr)) {
    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    error = "render Start " + wasapi::hr_hex(hr);
    return false;
  }

  while (!stop_flag.load(std::memory_order_acquire)) {
    UINT32 frames = 0;
    if (exclusive) {
      if (WaitForSingleObject(event_, 50) != WAIT_OBJECT_0) continue;
      frames = buffer_frames_;
    } else {
      WaitForSingleObject(event_, 50);
      UINT32 padding = 0;
      client_->GetCurrentPadding(&padding);
      frames = buffer_frames_ - padding;
      if (frames == 0) continue;
    }
    if (!write_block(frames)) break;
  }

  client_->Stop();
  if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
  return true;
}

}  // namespace mixbridge
