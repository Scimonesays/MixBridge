#include "mixbridge/vst3_effect_factory.hpp"

namespace mixbridge {

std::unique_ptr<RealtimeEffect> create_vst3_effect(
  const std::string&,
  std::string& error) {
  error = "vst3_host_not_built";
  return nullptr;
}

}  // namespace mixbridge
