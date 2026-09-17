#pragma once

#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <avrt.h>
#include <windows.h>

#include <cstdint>
#include <string>

namespace mixbridge::wasapi {

struct ComScope {
  HRESULT hr = E_FAIL;
  explicit ComScope(DWORD apt = COINIT_MULTITHREADED) {
    hr = CoInitializeEx(nullptr, apt);
  }
  ~ComScope() {
    if (SUCCEEDED(hr) || hr == S_FALSE) CoUninitialize();
  }
  bool ok() const { return SUCCEEDED(hr) || hr == S_FALSE; }
};

inline WAVEFORMATEXTENSIBLE make_float_stereo(UINT32 rate) {
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

inline WAVEFORMATEX make_pcm16_stereo(UINT32 rate) {
  WAVEFORMATEX wfx{};
  wfx.wFormatTag = WAVE_FORMAT_PCM;
  wfx.nChannels = 2;
  wfx.nSamplesPerSec = rate;
  wfx.wBitsPerSample = 16;
  wfx.nBlockAlign = 4;
  wfx.nAvgBytesPerSec = rate * 4;
  wfx.cbSize = 0;
  return wfx;
}

std::string hr_hex(HRESULT hr);
std::wstring device_friendly_name(IMMDevice* device);
std::wstring device_id_string(IMMDevice* device);

}  // namespace mixbridge::wasapi
