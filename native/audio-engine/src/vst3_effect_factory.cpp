#include "mixbridge/vst3_effect_factory.hpp"

#include "mixbridge/types.hpp"
#include "mixbridge/vst3_host.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>
#include <vector>

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

  bool save_state_file(const std::string& path, std::string& error) override {
    std::vector<uint8_t> component;
    std::vector<uint8_t> controller;
    if (!processor_.save_state(component, controller, error)) return false;

    std::ofstream out(std::filesystem::u8path(path), std::ios::binary | std::ios::trunc);
    if (!out) {
      error = "effect_state_open_write_failed";
      return false;
    }
    constexpr std::array<char, 8> magic{'M','B','F','X','1','\0','\0','\0'};
    const uint64_t component_size = static_cast<uint64_t>(component.size());
    const uint64_t controller_size = static_cast<uint64_t>(controller.size());
    out.write(magic.data(), static_cast<std::streamsize>(magic.size()));
    out.write(reinterpret_cast<const char*>(&component_size), sizeof(component_size));
    out.write(reinterpret_cast<const char*>(&controller_size), sizeof(controller_size));
    if (!component.empty()) {
      out.write(reinterpret_cast<const char*>(component.data()),
                static_cast<std::streamsize>(component.size()));
    }
    if (!controller.empty()) {
      out.write(reinterpret_cast<const char*>(controller.data()),
                static_cast<std::streamsize>(controller.size()));
    }
    if (!out.good()) {
      error = "effect_state_write_failed";
      return false;
    }
    return true;
  }

  bool load_state_file(const std::string& path, std::string& error) override {
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in) {
      error = "effect_state_open_read_failed";
      return false;
    }
    std::array<char, 8> magic{};
    uint64_t component_size = 0;
    uint64_t controller_size = 0;
    in.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    in.read(reinterpret_cast<char*>(&component_size), sizeof(component_size));
    in.read(reinterpret_cast<char*>(&controller_size), sizeof(controller_size));
    constexpr std::array<char, 8> expected{'M','B','F','X','1','\0','\0','\0'};
    constexpr uint64_t kMaxStateBytes = 64ull * 1024ull * 1024ull;
    if (!in.good() || magic != expected ||
        component_size > kMaxStateBytes || controller_size > kMaxStateBytes) {
      error = "effect_state_invalid";
      return false;
    }
    std::vector<uint8_t> component(static_cast<size_t>(component_size));
    std::vector<uint8_t> controller(static_cast<size_t>(controller_size));
    if (!component.empty()) {
      in.read(reinterpret_cast<char*>(component.data()),
              static_cast<std::streamsize>(component.size()));
    }
    if (!controller.empty()) {
      in.read(reinterpret_cast<char*>(controller.data()),
              static_cast<std::streamsize>(controller.size()));
    }
    if (!in.good() && !in.eof()) {
      error = "effect_state_read_failed";
      return false;
    }
    return processor_.load_state(component, controller, error);
  }

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
