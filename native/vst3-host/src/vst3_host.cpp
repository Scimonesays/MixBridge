#include "mixbridge/vst3_host.hpp"

#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <utility>

namespace mixbridge::vst3 {
namespace {

bool ok(Steinberg::tresult r) {
  return r == Steinberg::kResultOk || r == Steinberg::kResultTrue;
}

}  // namespace

struct Processor::Impl {
  VST3::Hosting::Module::Ptr module;
  Steinberg::IPtr<Steinberg::Vst::PlugProvider> provider;
  Steinberg::IPtr<Steinberg::Vst::IComponent> component;
  Steinberg::IPtr<Steinberg::Vst::IEditController> controller;
  Steinberg::Vst::IAudioProcessor* processor = nullptr;
  Steinberg::IPtr<Steinberg::Vst::HostApplication> host_context;
  Steinberg::Vst::HostProcessData process_data;
  Steinberg::Vst::ProcessContext process_context{};

  std::string module_path;
  std::string plugin_name;
  uint32_t max_block = 0;
  double sample_rate = 0.0;
  int64_t continuous_samples = 0;
  std::atomic<bool> bypass{false};
  std::atomic<bool> faulted{false};
  bool is_prepared = false;

  void deactivate() {
    if (processor && is_prepared) {
      processor->setProcessing(false);
      if (component) component->setActive(false);
    }
    is_prepared = false;
    process_data.unprepare();
    max_block = 0;
    sample_rate = 0.0;
    continuous_samples = 0;
  }

  void reset() {
    deactivate();
    if (processor) {
      processor->release();
      processor = nullptr;
    }
    component = nullptr;
    controller = nullptr;
    provider = nullptr;
    module = nullptr;
    host_context = nullptr;
    module_path.clear();
    plugin_name.clear();
    faulted.store(false, std::memory_order_release);
    Steinberg::Vst::PluginContextFactory::instance().setPluginContext(nullptr);
  }
};

Processor::Processor() : impl_(std::make_unique<Impl>()) {}

Processor::~Processor() {
  unload();
}

bool Processor::load(const std::string& module_path, std::string& error) {
  unload();
  if (module_path.empty()) {
    error = "vst3_module_path_required";
    return false;
  }

  auto module = VST3::Hosting::Module::create(module_path, error);
  if (!module) {
    if (error.empty()) error = "vst3_module_load_failed";
    return false;
  }

  auto factory = module->getFactory();
  VST3::Hosting::ClassInfo selected;
  bool found = false;
  for (const auto& info : factory.classInfos()) {
    if (info.category() == kVstAudioEffectClass) {
      selected = info;
      found = true;
      break;
    }
  }
  if (!found) {
    error = "vst3_no_audio_effect_class";
    return false;
  }

  impl_->host_context = Steinberg::owned(new Steinberg::Vst::HostApplication());
  Steinberg::Vst::PluginContextFactory::instance().setPluginContext(impl_->host_context.get());

  auto provider = Steinberg::owned(new Steinberg::Vst::PlugProvider(factory, selected, true));
  if (!provider || !provider->initialize()) {
    error = "vst3_plugin_initialize_failed";
    Steinberg::Vst::PluginContextFactory::instance().setPluginContext(nullptr);
    impl_->host_context = nullptr;
    return false;
  }

  auto component = provider->getComponentPtr();
  if (!component) {
    error = "vst3_component_missing";
    return false;
  }

  Steinberg::Vst::IAudioProcessor* processor = nullptr;
  if (!ok(component->queryInterface(
        Steinberg::Vst::IAudioProcessor::iid,
        reinterpret_cast<void**>(&processor))) || !processor) {
    error = "vst3_audio_processor_missing";
    return false;
  }

  impl_->module = std::move(module);
  impl_->provider = provider;
  impl_->component = component;
  impl_->controller = provider->getControllerPtr();
  impl_->processor = processor;
  impl_->module_path = module_path;
  impl_->plugin_name = selected.name();
  impl_->bypass.store(false, std::memory_order_release);
  impl_->faulted.store(false, std::memory_order_release);
  return true;
}

void Processor::unload() {
  if (impl_) impl_->reset();
}

bool Processor::prepare(double sample_rate, uint32_t max_block_frames, std::string& error) {
  if (!loaded()) {
    error = "vst3_not_loaded";
    return false;
  }
  if (sample_rate <= 0.0 || max_block_frames == 0) {
    error = "vst3_invalid_process_setup";
    return false;
  }

  impl_->deactivate();

  const auto inputs = impl_->component->getBusCount(
    Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
  const auto outputs = impl_->component->getBusCount(
    Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);
  if (inputs <= 0 || outputs <= 0) {
    error = "vst3_effect_requires_audio_io";
    return false;
  }

  for (Steinberg::int32 i = 0; i < inputs; ++i) {
    impl_->component->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kInput, i, true);
  }
  for (Steinberg::int32 i = 0; i < outputs; ++i) {
    impl_->component->activateBus(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, i, true);
  }

  if (!impl_->process_data.prepare(
        *impl_->component,
        static_cast<Steinberg::int32>(max_block_frames),
        Steinberg::Vst::kSample32)) {
    error = "vst3_process_buffers_prepare_failed";
    return false;
  }

  Steinberg::Vst::ProcessSetup setup{};
  setup.processMode = Steinberg::Vst::kRealtime;
  setup.symbolicSampleSize = Steinberg::Vst::kSample32;
  setup.maxSamplesPerBlock = static_cast<Steinberg::int32>(max_block_frames);
  setup.sampleRate = sample_rate;

  if (!ok(impl_->processor->setupProcessing(setup))) {
    error = "vst3_setup_processing_failed";
    impl_->process_data.unprepare();
    return false;
  }
  if (!ok(impl_->component->setActive(true))) {
    error = "vst3_set_active_failed";
    impl_->process_data.unprepare();
    return false;
  }
  if (!ok(impl_->processor->setProcessing(true))) {
    impl_->component->setActive(false);
    impl_->process_data.unprepare();
    error = "vst3_set_processing_failed";
    return false;
  }

  impl_->process_context = {};
  impl_->process_context.sampleRate = sample_rate;
  impl_->process_context.tempo = 120.0;
  impl_->process_data.processContext = &impl_->process_context;
  impl_->max_block = max_block_frames;
  impl_->sample_rate = sample_rate;
  impl_->continuous_samples = 0;
  impl_->is_prepared = true;
  return true;
}

bool Processor::process(float* interleaved_stereo, uint32_t frames, std::string& error) {
  if (!impl_->is_prepared || !impl_->processor) {
    error = "vst3_not_prepared";
    return false;
  }
  if (!interleaved_stereo || frames == 0 || frames > impl_->max_block) {
    error = "vst3_invalid_audio_block";
    return false;
  }
  if (!process_rt(interleaved_stereo, frames)) {
    error = impl_->faulted.load(std::memory_order_acquire)
              ? "vst3_process_faulted"
              : "vst3_process_failed";
    return false;
  }
  return true;
}


bool Processor::save_state(std::vector<uint8_t>& component_state,
                           std::vector<uint8_t>& controller_state,
                           std::string& error) {
  component_state.clear();
  controller_state.clear();
  if (!loaded()) {
    error = "vst3_not_loaded";
    return false;
  }

  Steinberg::MemoryStream component_stream;
  if (!ok(impl_->component->getState(&component_stream))) {
    error = "vst3_component_get_state_failed";
    return false;
  }
  if (component_stream.getSize() > 0 && component_stream.getData()) {
    const auto* begin = reinterpret_cast<const uint8_t*>(component_stream.getData());
    component_state.assign(begin, begin + static_cast<size_t>(component_stream.getSize()));
  }

  if (impl_->controller) {
    Steinberg::MemoryStream controller_stream;
    if (ok(impl_->controller->getState(&controller_stream)) &&
        controller_stream.getSize() > 0 && controller_stream.getData()) {
      const auto* begin = reinterpret_cast<const uint8_t*>(controller_stream.getData());
      controller_state.assign(begin, begin + static_cast<size_t>(controller_stream.getSize()));
    }
  }
  return true;
}

bool Processor::load_state(const std::vector<uint8_t>& component_state,
                           const std::vector<uint8_t>& controller_state,
                           std::string& error) {
  if (!loaded()) {
    error = "vst3_not_loaded";
    return false;
  }

  const bool was_prepared = impl_->is_prepared;
  if (was_prepared) {
    impl_->processor->setProcessing(false);
    impl_->component->setActive(false);
  }

  auto reactivate = [&]() -> bool {
    if (!was_prepared) return true;
    if (!ok(impl_->component->setActive(true))) {
      error = "vst3_state_reactivate_failed";
      impl_->is_prepared = false;
      return false;
    }
    if (!ok(impl_->processor->setProcessing(true))) {
      impl_->component->setActive(false);
      error = "vst3_state_processing_resume_failed";
      impl_->is_prepared = false;
      return false;
    }
    impl_->is_prepared = true;
    return true;
  };

  if (!component_state.empty()) {
    Steinberg::MemoryStream component_stream(
      const_cast<uint8_t*>(component_state.data()),
      static_cast<Steinberg::TSize>(component_state.size()));
    if (!ok(impl_->component->setState(&component_stream))) {
      error = "vst3_component_set_state_failed";
      reactivate();
      return false;
    }

    if (impl_->controller) {
      component_stream.seek(0, Steinberg::IBStream::kIBSeekSet, nullptr);
      if (!ok(impl_->controller->setComponentState(&component_stream))) {
        error = "vst3_controller_component_state_failed";
        reactivate();
        return false;
      }
    }
  }

  if (impl_->controller && !controller_state.empty()) {
    Steinberg::MemoryStream controller_stream(
      const_cast<uint8_t*>(controller_state.data()),
      static_cast<Steinberg::TSize>(controller_state.size()));
    if (!ok(impl_->controller->setState(&controller_stream))) {
      error = "vst3_controller_set_state_failed";
      reactivate();
      return false;
    }
  }

  return reactivate();
}

bool Processor::process_rt(float* interleaved_stereo, uint32_t frames) noexcept {
  if (!impl_->is_prepared || !impl_->processor || !interleaved_stereo ||
      frames == 0 || frames > impl_->max_block) {
    return false;
  }
  if (impl_->bypass.load(std::memory_order_acquire)) return true;
  if (impl_->faulted.load(std::memory_order_acquire)) return false;

  try {
    for (Steinberg::int32 bus = 0; bus < impl_->process_data.numInputs; ++bus) {
      auto& input = impl_->process_data.inputs[bus];
      input.silenceFlags = 0;
      for (Steinberg::int32 ch = 0; ch < input.numChannels; ++ch) {
        float* dst = input.channelBuffers32[ch];
        if (!dst) continue;
        for (uint32_t i = 0; i < frames; ++i) {
          dst[i] =
            ch < 2 ? interleaved_stereo[i * 2u + static_cast<uint32_t>(ch)] : 0.0f;
        }
      }
    }

    for (Steinberg::int32 bus = 0; bus < impl_->process_data.numOutputs; ++bus) {
      auto& output = impl_->process_data.outputs[bus];
      output.silenceFlags = 0;
      for (Steinberg::int32 ch = 0; ch < output.numChannels; ++ch) {
        if (output.channelBuffers32[ch]) {
          std::memset(output.channelBuffers32[ch], 0, sizeof(float) * frames);
        }
      }
    }

    impl_->process_data.numSamples = static_cast<Steinberg::int32>(frames);
    impl_->process_context.continousTimeSamples = impl_->continuous_samples;

    if (!ok(impl_->processor->process(impl_->process_data))) {
      impl_->faulted.store(true, std::memory_order_release);
      return false;
    }
    impl_->continuous_samples += frames;

    const auto& output = impl_->process_data.outputs[0];
    if (output.numChannels <= 0 || !output.channelBuffers32 ||
        !output.channelBuffers32[0]) {
      impl_->faulted.store(true, std::memory_order_release);
      return false;
    }

    const float* left = output.channelBuffers32[0];
    const float* right =
      output.numChannels > 1 && output.channelBuffers32[1] ? output.channelBuffers32[1] : left;
    for (uint32_t i = 0; i < frames; ++i) {
      interleaved_stereo[i * 2u] = left[i];
      interleaved_stereo[i * 2u + 1u] = right[i];
    }
    return true;
  } catch (...) {
    impl_->faulted.store(true, std::memory_order_release);
    return false;
  }
}

void Processor::set_bypass(bool bypass) noexcept {
  impl_->bypass.store(bypass, std::memory_order_release);
}

bool Processor::bypass() const noexcept {
  return impl_->bypass.load(std::memory_order_acquire);
}

bool Processor::faulted() const noexcept {
  return impl_->faulted.load(std::memory_order_acquire);
}

bool Processor::loaded() const noexcept {
  return impl_ && impl_->processor != nullptr && impl_->component != nullptr;
}

bool Processor::prepared() const noexcept {
  return impl_ && impl_->is_prepared;
}

const std::string& Processor::name() const noexcept {
  return impl_->plugin_name;
}

const std::string& Processor::path() const noexcept {
  return impl_->module_path;
}

}  // namespace mixbridge::vst3
