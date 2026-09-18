#include "mixbridge/vst3_host.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: mb-vst3-probe <plugin.vst3>\n";
    return 2;
  }

  mixbridge::vst3::Processor processor;
  std::string error;
  if (!processor.load(argv[1], error)) {
    std::cerr << "load_failed=" << error << "\n";
    return 3;
  }
  if (!processor.prepare(48000.0, 512, error)) {
    std::cerr << "prepare_failed=" << error << "\n";
    return 4;
  }

  std::vector<float> block(512u * 2u);
  double input_energy = 0.0;
  double output_energy = 0.0;
  double phase = 0.0;
  constexpr double two_pi = 6.2831853071795864769;
  constexpr double step = two_pi * 440.0 / 48000.0;

  for (int b = 0; b < 32; ++b) {
    for (uint32_t i = 0; i < 512; ++i) {
      const float s = static_cast<float>(std::sin(phase) * 0.1);
      phase += step;
      if (phase >= two_pi) phase -= two_pi;
      block[i * 2u] = s;
      block[i * 2u + 1u] = s;
      input_energy += static_cast<double>(s) * s * 2.0;
    }

    if (!processor.process(block.data(), 512, error)) {
      std::cerr << "process_failed=" << error << "\n";
      return 5;
    }

    for (float s : block) output_energy += static_cast<double>(s) * s;
  }

  std::cout << "plugin=" << processor.name() << "\n";
  std::cout << "input_rms=" << std::sqrt(input_energy / (32.0 * block.size())) << "\n";
  std::cout << "output_rms=" << std::sqrt(output_energy / (32.0 * block.size())) << "\n";
  std::cout << "vst3_process_result=PASS\n";
  return 0;
}
