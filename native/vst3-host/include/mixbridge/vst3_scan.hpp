#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mixbridge::vst3 {

struct PluginEntry {
  std::string name;
  std::string path;
  std::string vendor;
};

// Filesystem scan only — does not load plugins (no SDK required).
std::vector<PluginEntry> scan_default_folders();
std::vector<PluginEntry> scan_folder(const std::filesystem::path& root);

}  // namespace mixbridge::vst3
