#pragma once

#include "mixbridge_send_ids.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

namespace Steinberg {
namespace Vst {

class MixBridgeSendController : public EditController {
public:
  tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE;
  tresult PLUGIN_API setComponentState(IBStream* state) SMTG_OVERRIDE;

  static FUnknown* createInstance(void*) {
    return static_cast<IEditController*>(new MixBridgeSendController());
  }

  OBJ_METHODS(MixBridgeSendController, EditController)
  REFCOUNT_METHODS(EditController)
};

}  // namespace Vst
}  // namespace Steinberg
