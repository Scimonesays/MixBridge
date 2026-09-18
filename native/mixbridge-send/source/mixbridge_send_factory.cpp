#include "mixbridge_send_controller.h"
#include "mixbridge_send_ids.h"
#include "mixbridge_send_processor.h"
#include "mixbridge_send_version.h"

#include "public.sdk/source/main/pluginfactory.h"

#define stringPluginName "MixBridge Send"

using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF("MixBridge", stringCompanyWeb, stringCompanyEmail)

DEF_CLASS2(INLINE_UID_FROM_FUID(MixBridgeSendProcessorUID), PClassInfo::kManyInstances,
           kVstAudioEffectClass, stringPluginName, Vst::kDistributable, "Fx|Tools",
           FULL_VERSION_STR, kVstVersionString, MixBridgeSendProcessor::createInstance)

DEF_CLASS2(INLINE_UID_FROM_FUID(MixBridgeSendControllerUID), PClassInfo::kManyInstances,
           kVstComponentControllerClass, stringPluginName " Controller", 0, "",
           FULL_VERSION_STR, kVstVersionString, MixBridgeSendController::createInstance)

END_FACTORY
