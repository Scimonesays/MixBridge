#include "mixbridge_send_controller.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"

namespace Steinberg {
namespace Vst {

tresult PLUGIN_API MixBridgeSendController::initialize(FUnknown* context) {
  const tresult result = EditController::initialize(context);
  if (result != kResultOk) return result;

  parameters.addParameter(STR16("Send Enable"), nullptr, 1, 1.0,
                          ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass, kSendEnableId);
  parameters.addParameter(STR16("Send Level"), nullptr, 0, 1.0, ParameterInfo::kCanAutomate,
                          kSendLevelId);
  return kResultOk;
}

tresult PLUGIN_API MixBridgeSendController::setComponentState(IBStream* state) {
  if (!state) return kResultFalse;
  IBStreamer streamer(state, kLittleEndian);
  int32 enabled = 1;
  float level = 1.0f;
  if (!streamer.readInt32(enabled)) return kResultFalse;
  if (!streamer.readFloat(level)) return kResultFalse;
  setParamNormalized(kSendEnableId, enabled ? 1.0 : 0.0);
  setParamNormalized(kSendLevelId, level);
  return kResultOk;
}

}  // namespace Vst
}  // namespace Steinberg
