#include "mixbridge/vst3_processor.hpp"
#include "mixbridge/vst3_scan.hpp"

#include "base/source/fobject.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "pluginterfaces/vst/vsttypes.h"

#include <windows.h>

#include <cmath>
#include <cstring>
#include <memory>

using namespace Steinberg;
using namespace Steinberg::Vst;
using VST3::Hosting::Module;
using VST3::Hosting::ClassInfo;

namespace mixbridge::vst3 {
namespace {

HostApplication g_host;

float peak_of(const float* x, int32_t n) {
  float p = 0.0f;
  for (int32_t i = 0; i < n; ++i) p = std::max(p, std::fabs(x[i]));
  return p;
}

float rms_of(const float* x, int32_t n) {
  if (n <= 0) return 0.0f;
  double s = 0.0;
  for (int32_t i = 0; i < n; ++i) s += static_cast<double>(x[i]) * x[i];
  return static_cast<float>(std::sqrt(s / n));
}

}  // namespace

struct Processor::Impl {
  Module::Ptr module;
  IPtr<PlugProvider> provider;
  IPtr<IComponent> component;
  IPtr<IEditController> controller;
  IPtr<IAudioProcessor> processor;
  HostProcessData process_data;
  double sample_rate = 48000.0;
  int32_t max_block = 512;
  IPtr<IPlugView> view;
  HWND editor_hwnd = nullptr;
};

Processor::Processor() { impl_ = new Impl(); }
Processor::~Processor() {
  unload();
  delete impl_;
  impl_ = nullptr;
}

bool Processor::load(const std::string& path, std::string& error) {
  std::lock_guard<std::mutex> lock(control_mu_);
  unload();

  PluginContextFactory::instance().setPluginContext(&g_host);

  std::string err;
  auto module = Module::create(path, err);
  if (!module) {
    error = err.empty() ? "module create failed" : err;
    return false;
  }

  const auto& factory = module->getFactory();
  IPtr<PlugProvider> provider;
  for (auto& ci : factory.classInfos()) {
    if (ci.category() == kVstAudioEffectClass) {
      provider = owned(NEW PlugProvider(factory, ci, true));
      if (provider && provider->initialize()) break;
      provider = nullptr;
    }
  }
  if (!provider) {
    error = "no audio effect class";
    return false;
  }

  IComponent* component = provider->getComponent();
  IEditController* controller = provider->getController();
  if (!component) {
    error = "missing component";
    return false;
  }

  IAudioProcessor* proc = nullptr;
  if (component->queryInterface(IAudioProcessor::iid, (void**)&proc) != kResultOk || !proc) {
    error = "missing IAudioProcessor";
    return false;
  }

  impl_->module = module;
  impl_->provider = provider;
  impl_->component = component;
  impl_->controller = controller;
  impl_->processor = proc;
  path_ = path;
  name_ = module->getName();
  // Prefer a short UI name; Module::getName() sometimes returns a filesystem path.
  {
    const auto slash = path.find_last_of("\\/");
    std::string stem = (slash == std::string::npos) ? path : path.substr(slash + 1);
    if (stem.size() > 5 && stem.compare(stem.size() - 5, 5, ".vst3") == 0) stem.resize(stem.size() - 5);
    if (name_.empty() || name_.find('\\') != std::string::npos || name_.find('/') != std::string::npos ||
        name_.size() > 64) {
      name_ = stem;
    }
  }
  loaded_ = true;
  prepared_ = false;
  bypass_ = false;
  return true;
}

void Processor::unload() {
  close_editor();
  if (!impl_) return;
  if (prepared_ && impl_->processor) {
    impl_->processor->setProcessing(false);
  }
  if (impl_->component && prepared_) {
    impl_->component->setActive(false);
  }
  impl_->process_data.unprepare();
  impl_->processor = nullptr;
  impl_->controller = nullptr;
  impl_->component = nullptr;
  impl_->provider = nullptr;
  impl_->module.reset();
  loaded_ = false;
  prepared_ = false;
}

bool Processor::set_bypass(bool bypass) {
  bypass_ = bypass;
  return true;
}

bool Processor::prepare(double sample_rate, int32_t max_block, std::string& error) {
  std::lock_guard<std::mutex> lock(control_mu_);
  if (!loaded_ || !impl_->component || !impl_->processor) {
    error = "not loaded";
    return false;
  }

  if (prepared_) {
    impl_->processor->setProcessing(false);
    impl_->component->setActive(false);
    impl_->process_data.unprepare();
    prepared_ = false;
  }

  impl_->sample_rate = sample_rate;
  impl_->max_block = max_block;

  SpeakerArrangement inArr = SpeakerArr::kStereo;
  SpeakerArrangement outArr = SpeakerArr::kStereo;
  impl_->processor->setBusArrangements(&inArr, 1, &outArr, 1);

  // Activate first audio buses.
  int32_t in_buses = impl_->component->getBusCount(kAudio, kInput);
  int32_t out_buses = impl_->component->getBusCount(kAudio, kOutput);
  for (int32_t i = 0; i < in_buses; ++i) impl_->component->activateBus(kAudio, kInput, i, true);
  for (int32_t i = 0; i < out_buses; ++i) impl_->component->activateBus(kAudio, kOutput, i, true);

  if (impl_->processor->canProcessSampleSize(kSample32) != kResultTrue) {
    error = "plugin does not support 32-bit float";
    return false;
  }

  ProcessSetup setup{};
  setup.processMode = kRealtime;
  setup.symbolicSampleSize = kSample32;
  setup.maxSamplesPerBlock = max_block;
  setup.sampleRate = sample_rate;
  if (impl_->processor->setupProcessing(setup) != kResultOk) {
    error = "setupProcessing failed";
    return false;
  }

  if (!impl_->process_data.prepare(*impl_->component, max_block, kSample32)) {
    error = "HostProcessData prepare failed";
    return false;
  }

  if (impl_->component->setActive(true) != kResultOk) {
    error = "setActive failed";
    return false;
  }
  if (impl_->processor->setProcessing(true) != kResultOk) {
    error = "setProcessing failed";
    return false;
  }

  prepared_ = true;
  return true;
}

bool Processor::process(float* interleaved_io, int32_t frames, ProcessStats* stats) {
  if (!loaded_ || !prepared_ || !interleaved_io || frames <= 0) return false;
  if (frames > impl_->max_block) return false;

  if (bypass_) {
    if (stats) {
      stats->in_peak = peak_of(interleaved_io, frames * 2);
      stats->out_peak = stats->in_peak;
      stats->in_rms = rms_of(interleaved_io, frames * 2);
      stats->out_rms = stats->in_rms;
    }
    return true;
  }

  // Deinterleave into host buffers (assume stereo in/out bus 0).
  if (impl_->process_data.numInputs < 1 || impl_->process_data.numOutputs < 1) return false;
  auto& in_bus = impl_->process_data.inputs[0];
  auto& out_bus = impl_->process_data.outputs[0];
  if (!in_bus.channelBuffers32 || !out_bus.channelBuffers32) return false;
  const int32_t in_ch = in_bus.numChannels;
  const int32_t out_ch = out_bus.numChannels;
  if (in_ch < 1 || out_ch < 1) return false;

  float* inL = in_bus.channelBuffers32[0];
  float* inR = (in_ch > 1) ? in_bus.channelBuffers32[1] : in_bus.channelBuffers32[0];
  for (int32_t i = 0; i < frames; ++i) {
    inL[i] = interleaved_io[i * 2];
    inR[i] = interleaved_io[i * 2 + 1];
  }
  for (int32_t c = 2; c < in_ch; ++c) {
    std::memset(in_bus.channelBuffers32[c], 0, sizeof(float) * static_cast<size_t>(frames));
  }

  if (stats) {
    stats->in_peak = peak_of(interleaved_io, frames * 2);
    stats->in_rms = rms_of(interleaved_io, frames * 2);
  }

  impl_->process_data.numSamples = frames;
  const tresult pr = impl_->processor->process(impl_->process_data);
  if (pr != kResultOk) return false;

  float* outL = out_bus.channelBuffers32[0];
  float* outR = (out_ch > 1) ? out_bus.channelBuffers32[1] : out_bus.channelBuffers32[0];
  for (int32_t i = 0; i < frames; ++i) {
    interleaved_io[i * 2] = outL[i];
    interleaved_io[i * 2 + 1] = outR[i];
  }

  if (stats) {
    stats->out_peak = peak_of(interleaved_io, frames * 2);
    stats->out_rms = rms_of(interleaved_io, frames * 2);
  }
  return true;
}

bool Processor::get_state(std::vector<uint8_t>& out, std::string& error) {
  std::lock_guard<std::mutex> lock(control_mu_);
  if (!impl_->component) {
    error = "not loaded";
    return false;
  }
  MemoryStream stream;
  if (impl_->component->getState(&stream) != kResultOk) {
    error = "getState failed";
    return false;
  }
  out.assign(reinterpret_cast<const uint8_t*>(stream.getData()),
             reinterpret_cast<const uint8_t*>(stream.getData()) + stream.getSize());
  return true;
}

bool Processor::set_state(const uint8_t* data, size_t size, std::string& error) {
  std::lock_guard<std::mutex> lock(control_mu_);
  if (!impl_->component) {
    error = "not loaded";
    return false;
  }
  MemoryStream stream;
  if (size > 0) {
    stream.write(const_cast<uint8_t*>(data), static_cast<int32>(size), nullptr);
    stream.seek(0, IBStream::kIBSeekSet, nullptr);
  }
  if (impl_->component->setState(&stream) != kResultOk) {
    error = "setState failed";
    return false;
  }
  if (impl_->controller) {
    stream.seek(0, IBStream::kIBSeekSet, nullptr);
    impl_->controller->setComponentState(&stream);
  }
  return true;
}

bool Processor::open_editor(std::string& error) {
  std::lock_guard<std::mutex> lock(control_mu_);
  close_editor();
  if (!impl_->controller) {
    error = "no edit controller";
    return false;
  }
  IPlugView* view = impl_->controller->createView(ViewType::kEditor);
  if (!view) {
    error = "createView failed";
    return false;
  }
  impl_->view = view;

  ViewRect vr{};
  view->getSize(&vr);
  const int w = std::max(200, vr.right - vr.left);
  const int h = std::max(100, vr.bottom - vr.top);

  WNDCLASSW wc{};
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"MixBridgeVstEditor";
  RegisterClassW(&wc);

  HWND hwnd = CreateWindowExW(0, L"MixBridgeVstEditor", L"MixBridge FX",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT,
                              CW_USEDEFAULT, w + 16, h + 39, nullptr, nullptr, wc.hInstance, nullptr);
  if (!hwnd) {
    error = "CreateWindow failed";
    impl_->view = nullptr;
    return false;
  }
  impl_->editor_hwnd = hwnd;
  if (view->isPlatformTypeSupported(kPlatformTypeHWND) != kResultTrue) {
    error = "HWND platform unsupported";
    DestroyWindow(hwnd);
    impl_->editor_hwnd = nullptr;
    impl_->view = nullptr;
    return false;
  }
  if (view->attached(hwnd, kPlatformTypeHWND) != kResultOk) {
    error = "view attached failed";
    DestroyWindow(hwnd);
    impl_->editor_hwnd = nullptr;
    impl_->view = nullptr;
    return false;
  }
  return true;
}

void Processor::close_editor() {
  if (!impl_) return;
  if (impl_->view) {
    impl_->view->removed();
    impl_->view = nullptr;
  }
  if (impl_->editor_hwnd) {
    DestroyWindow(impl_->editor_hwnd);
    impl_->editor_hwnd = nullptr;
  }
}

std::vector<std::pair<std::string, std::string>> list_installed() {
  std::vector<std::pair<std::string, std::string>> out;
  for (const auto& e : scan_default_folders()) {
    out.emplace_back(e.name, e.path);
  }
  return out;
}

}  // namespace mixbridge::vst3
