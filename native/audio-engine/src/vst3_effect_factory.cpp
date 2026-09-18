#include "mixbridge/vst3_effect_factory.hpp"

#include "mixbridge/types.hpp"
#include "mixbridge/vst3_host.hpp"

#include <memory>
#include <string_view>

namespace mixbridge {
namespace {

class Vst3RealtimeEffect final : public RealtimeEffect {
public:
  bool open(const std::string& path, std::string& error) {
    if (!processor_.load(path, error)) return false;
    return processor_.prepare(static_cast<double>(kEngineRate), 512, error);
  }

  bool process(float* audio, uint32_t frames) noexcept override {
    return processor_.process_rt(audio, frames);
  }
  void set_bypass(bool bypass) noexcept override { processor_.set_bypass(bypass); }
  bool bypass() const noexcept override { return processor_.bypass(); }
  bool faulted() const noexcept override { return processor_.faulted(); }
  std::string_view name() const noexcept override { return processor_.name(); }

private:
  vst3::Processor processor_;
};

}  // namespace

std::unique_ptr<RealtimeEffect> create_vst3_effect(
  const std::string& module_path,
  std::string& error) {
  auto effect = std::make_unique<Vst3RealtimeEffect>();
  if (!effect->open(module_path, error)) return nullptr;
  return effect;
}

}  // namespace mixbridge
