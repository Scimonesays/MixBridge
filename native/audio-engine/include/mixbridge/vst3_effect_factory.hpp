#pragma once

#include "mixbridge/effect_processor.hpp"

#include <memory>
#include <string>

namespace mixbridge {

std::unique_ptr<RealtimeEffect> create_vst3_effect(
  const std::string& module_path,
  std::string& error);

}  // namespace mixbridge
