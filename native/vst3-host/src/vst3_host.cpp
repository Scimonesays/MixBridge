#include "mixbridge/vst3_host.hpp"

#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/gui/iplugview.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#endif

namespace mixbridge::vst3 {
namespace {

bool ok(Steinberg::tresult r) {
  return r == Steinberg::kResultOk || r == Steinberg::kResultTrue;
}

bool processing_transition_ok(Steinberg::tresult r) {
  // VST3 AudioEffect's default setProcessing implementation returns
  // kNotImplemented. Steinberg's own validator treats that as non-fatal.
  return ok(r) || r == Steinberg::kNotImplemented;
}

class ComponentHandler final : public Steinberg::Vst::IComponentHandler {
public:
  ComponentHandler(Steinberg::Vst::ParameterChangeTransfer& transfer,
                   std::atomic<bool>& dirty)
      : transfer_(transfer), dirty_(dirty) {}

  Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID) override {
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API performEdit(
      Steinberg::Vst::ParamID id,
      Steinberg::Vst::ParamValue value) override {
    transfer_.addChange(id, value, 0);
    dirty_.store(true, std::memory_order_release);
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID) override {
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32) override {
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override {
    if (!obj) return Steinberg::kInvalidArgument;
    *obj = nullptr;
    if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::Vst::IComponentHandler::iid) ||
        Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid)) {
      *obj = static_cast<Steinberg::Vst::IComponentHandler*>(this);
      return Steinberg::kResultTrue;
    }
    return Steinberg::kNoInterface;
  }
  Steinberg::uint32 PLUGIN_API addRef() override { return 1000; }
  Steinberg::uint32 PLUGIN_API release() override { return 1000; }

private:
  Steinberg::Vst::ParameterChangeTransfer& transfer_;
  std::atomic<bool>& dirty_;
};

#ifdef _WIN32
std::wstring editor_title_wide(const std::string& value) {
  if (value.empty()) return L"MixBridge VST3";
  const int count = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (count <= 0) return L"MixBridge VST3";
  std::wstring out(static_cast<size_t>(count), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), count);
  return out;
}

class EditorFrame final : public Steinberg::IPlugFrame {
public:
  Steinberg::IPtr<Steinberg::IPlugView> view;
  HWND hwnd = nullptr;
  std::atomic<bool>* open_flag = nullptr;

  Steinberg::tresult PLUGIN_API resizeView(
      Steinberg::IPlugView* candidate, Steinberg::ViewRect* size) override {
    if (!hwnd || !candidate || candidate != view.get() || !size)
      return Steinberg::kInvalidArgument;
    RECT wr{0, 0, size->right - size->left, size->bottom - size->top};
    const auto style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    const auto exstyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    AdjustWindowRectEx(&wr, style, FALSE, exstyle);
    SetWindowPos(hwnd, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    candidate->onSize(size);
    return Steinberg::kResultTrue;
  }
  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override {
    if (!obj) return Steinberg::kInvalidArgument;
    *obj = nullptr;
    if (Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::IPlugFrame::iid) ||
        Steinberg::FUnknownPrivate::iidEqual(iid, Steinberg::FUnknown::iid)) {
      *obj = static_cast<Steinberg::IPlugFrame*>(this);
      return Steinberg::kResultTrue;
    }
    return Steinberg::kNoInterface;
  }
  Steinberg::uint32 PLUGIN_API addRef() override { return 1000; }
  Steinberg::uint32 PLUGIN_API release() override { return 1000; }
};

LRESULT CALLBACK mixbridge_editor_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  auto* frame = reinterpret_cast<EditorFrame*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    frame = static_cast<EditorFrame*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(frame));
    if (frame) frame->hwnd = hwnd;
  }
  if (!frame) return DefWindowProcW(hwnd, message, wParam, lParam);
  switch (message) {
    case WM_ERASEBKGND: return 1;
    case WM_SIZE:
      if (frame->view) {
        Steinberg::ViewRect size{};
        size.right = LOWORD(lParam);
        size.bottom = HIWORD(lParam);
        frame->view->onSize(&size);
      }
      return 0;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      if (frame->view) {
        frame->view->setFrame(nullptr);
        frame->view->removed();
        frame->view = nullptr;
      }
      if (frame->open_flag) frame->open_flag->store(false, std::memory_order_release);
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(hwnd, message, wParam, lParam);
  }
}

bool register_mixbridge_editor_class(HINSTANCE instance) {
  static std::once_flag once;
  static bool result = false;
  std::call_once(once, [&] {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = mixbridge_editor_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"MixBridgeVST3Editor";
    result = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  });
  return result;
}
#endif

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
  Steinberg::Vst::ParameterChanges input_changes{0};
  Steinberg::Vst::ParameterChangeTransfer ui_changes{0};
  std::atomic<bool> editor_state_dirty{false};
  ComponentHandler component_handler{ui_changes, editor_state_dirty};
  std::thread editor_thread;
  std::atomic<bool> editor_is_open{false};
#ifdef _WIN32
  std::atomic<HWND> editor_hwnd{nullptr};
#endif

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

  void close_editor() noexcept {
#ifdef _WIN32
    if (auto hwnd = editor_hwnd.load(std::memory_order_acquire))
      PostMessageW(hwnd, WM_CLOSE, 0, 0);
#endif
    if (editor_thread.joinable()) editor_thread.join();
#ifdef _WIN32
    editor_hwnd.store(nullptr, std::memory_order_release);
#endif
    editor_is_open.store(false, std::memory_order_release);
  }

  void reset() {
    close_editor();
    deactivate();
    if (processor) {
      processor->release();
      processor = nullptr;
    }
    component = nullptr;
    if (controller) controller->setComponentHandler(nullptr);
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
  if (impl_->controller) {
    const auto parameter_count =
      std::max<Steinberg::int32>(16, impl_->controller->getParameterCount());
    impl_->input_changes.setMaxParameters(parameter_count);
    impl_->ui_changes.setMaxParameters(parameter_count);
    impl_->controller->setComponentHandler(&impl_->component_handler);
  }
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
  if (!processing_transition_ok(impl_->processor->setProcessing(true))) {
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
  impl_->editor_state_dirty.store(false, std::memory_order_release);
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
    if (!processing_transition_ok(impl_->processor->setProcessing(true))) {
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

bool Processor::open_editor(std::string& error) {
#ifndef _WIN32
  error = "vst3_editor_windows_only";
  return false;
#else
  if (!impl_->controller) {
    error = "vst3_editor_controller_missing";
    return false;
  }
  if (impl_->editor_is_open.load(std::memory_order_acquire)) {
    if (auto hwnd = impl_->editor_hwnd.load(std::memory_order_acquire)) {
      ShowWindow(hwnd, SW_RESTORE);
      SetForegroundWindow(hwnd);
    }
    return true;
  }
  if (impl_->editor_thread.joinable()) impl_->editor_thread.join();

  auto* controller = impl_->controller.get();
  controller->addRef();
  const auto title = impl_->plugin_name.empty()
    ? std::string("MixBridge VST3")
    : impl_->plugin_name + " — MixBridge";
  impl_->editor_is_open.store(true, std::memory_order_release);

  impl_->editor_thread = std::thread([this, controller, title] {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    auto view = Steinberg::owned(controller->createView(Steinberg::Vst::ViewType::kEditor));
    if (!view ||
        view->isPlatformTypeSupported(Steinberg::kPlatformTypeHWND) != Steinberg::kResultTrue) {
      impl_->editor_is_open.store(false, std::memory_order_release);
      controller->release();
      CoUninitialize();
      return;
    }
    Steinberg::ViewRect vr{};
    if (view->getSize(&vr) != Steinberg::kResultTrue) {
      impl_->editor_is_open.store(false, std::memory_order_release);
      controller->release();
      CoUninitialize();
      return;
    }

    const auto instance = GetModuleHandleW(nullptr);
    if (!register_mixbridge_editor_class(instance)) {
      impl_->editor_is_open.store(false, std::memory_order_release);
      controller->release();
      CoUninitialize();
      return;
    }

    EditorFrame frame;
    frame.view = view;
    frame.open_flag = &impl_->editor_is_open;

    DWORD style = WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    if (view->canResize() == Steinberg::kResultTrue) style |= WS_SIZEBOX | WS_MAXIMIZEBOX;
    RECT wr{0, 0, vr.right - vr.left, vr.bottom - vr.top};
    AdjustWindowRectEx(&wr, style, FALSE, WS_EX_APPWINDOW);
    const auto wtitle = editor_title_wide(title);
    const auto hwnd = CreateWindowExW(
      WS_EX_APPWINDOW, L"MixBridgeVST3Editor", wtitle.c_str(), style,
      CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
      nullptr, nullptr, instance, &frame);
    if (!hwnd) {
      impl_->editor_is_open.store(false, std::memory_order_release);
      controller->release();
      CoUninitialize();
      return;
    }
    impl_->editor_hwnd.store(hwnd, std::memory_order_release);
    view->setFrame(&frame);
    if (view->attached(hwnd, Steinberg::kPlatformTypeHWND) != Steinberg::kResultTrue) {
      DestroyWindow(hwnd);
      impl_->editor_hwnd.store(nullptr, std::memory_order_release);
      impl_->editor_is_open.store(false, std::memory_order_release);
      controller->release();
      CoUninitialize();
      return;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    impl_->editor_hwnd.store(nullptr, std::memory_order_release);
    impl_->editor_is_open.store(false, std::memory_order_release);
    controller->release();
    CoUninitialize();
  });
  return true;
#endif
}

void Processor::close_editor() noexcept {
  impl_->close_editor();
}

bool Processor::editor_open() const noexcept {
  return impl_->editor_is_open.load(std::memory_order_acquire);
}

bool Processor::editor_dirty() const noexcept {
  return impl_->editor_state_dirty.load(std::memory_order_acquire);
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
    impl_->input_changes.clearQueue();
    impl_->ui_changes.transferChangesTo(impl_->input_changes);
    impl_->process_data.inputParameterChanges =
      impl_->input_changes.getParameterCount() > 0 ? &impl_->input_changes : nullptr;

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
