#pragma once

#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <avrt.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

struct ComInit {
  ComInit() { CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
  ~ComInit() { CoUninitialize(); }
};

struct ProbeResult {
  bool ok = false;
  std::string name;
  std::string detail;
  double peak = 0.0;
  double rms = 0.0;
  double dominant_hz = 0.0;
  uint64_t frames = 0;
};

inline void print_hr(const char* what, HRESULT hr) {
  std::fprintf(stderr, "[error] %s hr=0x%08lX\n", what, static_cast<unsigned long>(hr));
}

std::string wide_to_utf8(std::wstring_view ws);
std::wstring utf8_to_wide(std::string_view s);

HRESULT get_default_device(EDataFlow flow, IMMDevice** device);
HRESULT enumerate_endpoints(EDataFlow flow, std::vector<std::pair<std::wstring, std::wstring>>& out);

// Capture shared-mode float32 (converted) into interleaved buffer.
HRESULT capture_shared_seconds(
  IMMDevice* device,
  double seconds,
  UINT32 rate,
  std::vector<float>& interleaved,
  UINT32& out_channels,
  UINT32& out_rate);

// Render a stereo sine for `seconds` on device.
HRESULT render_tone_seconds(
  IMMDevice* device,
  double seconds,
  double freq_hz,
  float amplitude);

// System loopback on a render endpoint (AUDCLNT_STREAMFLAGS_LOOPBACK).
HRESULT capture_system_loopback_seconds(
  IMMDevice* render_device,
  double seconds,
  UINT32 rate,
  std::vector<float>& interleaved,
  UINT32& out_channels,
  UINT32& out_rate);

// Process-tree loopback (Win10 20348+). Captures audio rendered by pid (+ children).
HRESULT capture_process_loopback_seconds(
  DWORD pid,
  bool include_tree,
  double seconds,
  UINT32 rate,
  std::vector<float>& interleaved,
  UINT32& out_channels,
  UINT32& out_rate);

struct AnalyzeStats {
  double peak = 0.0;
  double rms = 0.0;
  double dominant_hz = 0.0;
  bool near_silence = true;
};

AnalyzeStats analyze_pcm_f32(const float* samples, size_t count, UINT32 channels, UINT32 rate);
bool write_wav_f32(const std::wstring& path, const float* samples, size_t frames, UINT32 channels, UINT32 rate);

int cmd_enumerate();
int cmd_capture(double seconds, const std::wstring& wav_path);
int cmd_render(double seconds, double freq);
int cmd_system_loopback(double seconds, const std::wstring& wav_path);
int cmd_process_loopback(DWORD pid, double seconds, const std::wstring& wav_path);
int cmd_selftest();
