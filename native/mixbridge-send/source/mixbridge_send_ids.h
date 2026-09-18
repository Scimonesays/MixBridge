#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace Steinberg {
namespace Vst {

enum MixBridgeSendParamIds : ParamID {
  kSendEnableId = 100,
  kSendLevelId = 101,
};

static const FUID MixBridgeSendProcessorUID(0x8F3A2B1C, 0x4D694200, 0x536E6450, 0x00010001);
static const FUID MixBridgeSendControllerUID(0x8F3A2B1C, 0x4D694200, 0x536E6443, 0x00010001);

}  // namespace Vst
}  // namespace Steinberg
