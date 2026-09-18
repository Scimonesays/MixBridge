#include "mixbridge_send_processor.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "public.sdk/source/vst/vstaudioprocessoralgo.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>

namespace Steinberg {
namespace Vst {

namespace {

uint32_t next_instance_id() {
  static std::atomic<uint32_t> counter{1};
  return counter.fetch_add(1, std::memory_order_relaxed);
}

std::string default_source_name() {
  if (const char* env = std::getenv("MIXBRIDGE_SEND_SOURCE_NAME")) {
    if (env[0]) return env;
  }
  return "MixBridge Send";
}

}  // namespace

MixBridgeSendProcessor::MixBridgeSendProcessor() { setControllerClass(MixBridgeSendControllerUID); }

tresult PLUGIN_API MixBridgeSendProcessor::initialize(FUnknown* context) {
  const tresult result = AudioEffect::initialize(context);
  if (result != kResultOk) return result;

  addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
  addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
  source_name_ = default_source_name();
  instance_id_ = next_instance_id();
  return kResultOk;
}

tresult PLUGIN_API MixBridgeSendProcessor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                                              SpeakerArrangement* outputs,
                                                              int32 numOuts) {
  if (numIns == 1 && numOuts == 1 && inputs[0] == outputs[0]) {
    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
  }
  return kResultFalse;
}

tresult PLUGIN_API MixBridgeSendProcessor::setupProcessing(ProcessSetup& setup) {
  sample_rate_ = static_cast<uint32_t>(setup.sampleRate);
  return AudioEffect::setupProcessing(setup);
}

void MixBridgeSendProcessor::startSendPath() {
#if defined(_WIN32)
  const uint32_t pid = GetCurrentProcessId();
#else
  const uint32_t pid = 0;
#endif
  std::string mapping;
  if (!shm_.open(pid, instance_id_, source_name_.c_str(), sample_rate_, mapping)) {
    return;
  }
  shm_mapping_ = mapping;
  register_thread_ = std::thread([this]() { registerAsync(); });
}

void MixBridgeSendProcessor::registerAsync() {
  std::string response;
  mixbridge_send::register_vst3_send(source_name_, shm_mapping_, response);
}

void MixBridgeSendProcessor::stopSendPath() {
  if (register_thread_.joinable()) register_thread_.join();
  shm_.close();
  shm_mapping_.clear();
}

tresult PLUGIN_API MixBridgeSendProcessor::setActive(TBool state) {
  if (state) {
    startSendPath();
    active_ = shm_.active();
  } else {
    active_ = false;
    stopSendPath();
  }
  return AudioEffect::setActive(state);
}

void MixBridgeSendProcessor::applyParameterChanges(IParameterChanges* changes) {
  if (!changes) return;
  const int32 count = changes->getParameterCount();
  for (int32 i = 0; i < count; ++i) {
    if (IParamValueQueue* queue = changes->getParameterData(i)) {
      ParamValue value = 0.0;
      int32 sampleOffset = 0;
      const int32 points = queue->getPointCount();
      if (points <= 0) continue;
      if (queue->getPoint(points - 1, sampleOffset, value) != kResultTrue) continue;
      switch (queue->getParameterId()) {
        case kSendEnableId:
          send_enabled_.store(value > 0.5, std::memory_order_relaxed);
          break;
        case kSendLevelId:
          send_level_.store(static_cast<float>(value), std::memory_order_relaxed);
          break;
        default:
          break;
      }
    }
  }
}

tresult PLUGIN_API MixBridgeSendProcessor::process(ProcessData& data) {
  applyParameterChanges(data.inputParameterChanges);

  if (data.numInputs == 0 || data.numOutputs == 0 || data.numSamples <= 0) {
    return kResultOk;
  }

  const int32 channels =
      std::min(data.inputs[0].numChannels, data.outputs[0].numChannels);
  if (channels <= 0) return kResultOk;

  const bool silent =
      data.inputs[0].silenceFlags == getChannelMask(data.inputs[0].numChannels);

  for (int32 ch = 0; ch < channels; ++ch) {
    float* in = data.inputs[0].channelBuffers32[ch];
    float* out = data.outputs[0].channelBuffers32[ch];
    if (!in || !out) continue;
    if (in != out) {
      if (silent) {
        std::memset(out, 0, static_cast<size_t>(data.numSamples) * sizeof(float));
      } else {
        std::memcpy(out, in, static_cast<size_t>(data.numSamples) * sizeof(float));
      }
    } else if (silent) {
      std::memset(out, 0, static_cast<size_t>(data.numSamples) * sizeof(float));
    }
  }
  data.outputs[0].silenceFlags = data.inputs[0].silenceFlags;

  if (!active_ || !send_enabled_.load(std::memory_order_relaxed) || silent) {
    return kResultOk;
  }

  const float level = send_level_.load(std::memory_order_relaxed);
  if (level <= 0.000001f) return kResultOk;

  const uint32_t frames = static_cast<uint32_t>(data.numSamples);
  interleave_scratch_.resize(static_cast<size_t>(frames) * 2u);
  float* left = data.inputs[0].channelBuffers32[0];
  float* right = channels > 1 ? data.inputs[0].channelBuffers32[1] : data.inputs[0].channelBuffers32[0];
  for (uint32_t i = 0; i < frames; ++i) {
    interleave_scratch_[i * 2] = left[i] * level;
    interleave_scratch_[i * 2 + 1] = right[i] * level;
  }
  shm_.write_interleaved(interleave_scratch_.data(), frames);
  return kResultOk;
}

tresult PLUGIN_API MixBridgeSendProcessor::setState(IBStream* state) {
  if (!state) return kResultFalse;
  IBStreamer streamer(state, kLittleEndian);
  int32 enabled = 1;
  float level = 1.0f;
  if (!streamer.readInt32(enabled)) return kResultFalse;
  if (!streamer.readFloat(level)) return kResultFalse;
  send_enabled_.store(enabled != 0, std::memory_order_relaxed);
  send_level_.store(level, std::memory_order_relaxed);
  return kResultOk;
}

tresult PLUGIN_API MixBridgeSendProcessor::getState(IBStream* state) {
  IBStreamer streamer(state, kLittleEndian);
  streamer.writeInt32(send_enabled_.load(std::memory_order_relaxed) ? 1 : 0);
  streamer.writeFloat(send_level_.load(std::memory_order_relaxed));
  return kResultOk;
}

}  // namespace Vst
}  // namespace Steinberg
