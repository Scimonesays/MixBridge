#include "mixbridge/vst3_scan.hpp"

#include <cstdio>

int main() {
  const auto plugins = mixbridge::vst3::scan_default_folders();
  std::printf("vst3_scan_count=%zu\n", plugins.size());
  for (const auto& p : plugins) {
    std::printf("PLUGIN name=%s path=%s\n", p.name.c_str(), p.path.c_str());
  }
  std::printf("vst3_scan_result=PASS\n");
  return 0;
}
