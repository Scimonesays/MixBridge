#pragma once

#include "mixbridge_send_ids.h"
#include "send_pipe_client.h"
#include "send_shm.h"

#include "public.sdk/source/vst/vstaudioeffect.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace Steinberg {
namespace Vst {

class MixBridgeSendProcessor : public AudioEffect {
public:
  MixBridgeSendProcessor();

  tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE;
  tresult PLUGIN_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
                                        SpeakerArrangement* outputs, int32 numOuts) SMTG_OVERRIDE;
  tresult PLUGIN_API setupProcessing(ProcessSetup& setup) SMTG_OVERRIDE;
  tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE;
  tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE;
  tresult PLUGIN_API setState(IBStream* state) SMTG_OVERRIDE;
  tresult PLUGIN_API getState(IBStream* state) SMTG_OVERRIDE;

  static FUnknown* createInstance(void*) {
    return static_cast<IAudioProcessor*>(new MixBridgeSendProcessor());
  }

private:
  void applyParameterChanges(IParameterChanges* changes);
  void startSendPath();
  void stopSendPath();
  void registerAsync();

  mixbridge_send::SendShmWriter shm_;
  std::string source_name_{"MixBridge Send"};
  std::string shm_mapping_;
  std::vector<float> interleave_scratch_;
  std::atomic<bool> send_enabled_{true};
  std::atomic<float> send_level_{1.0f};
  uint32_t sample_rate_ = 48000;
  uint32_t instance_id_ = 1;
  bool active_ = false;
  std::thread register_thread_;
};

}  // namespace Vst
}  // namespace Steinberg
