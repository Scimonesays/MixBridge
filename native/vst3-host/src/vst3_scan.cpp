#include "mixbridge/vst3_scan.hpp"

#include <algorithm>

namespace mixbridge::vst3 {
namespace {

std::string stem_name(const std::filesystem::path& p) {
  auto s = p.stem().string();
  const std::string suffix = ".vst3";
  if (s.size() > suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0) {
    s.resize(s.size() - suffix.size());
  }
  return s;
}

void scan_recursive(const std::filesystem::path& root, std::vector<PluginEntry>& out) {
  std::error_code ec;
  if (!std::filesystem::exists(root, ec)) return;
  for (auto it = std::filesystem::recursive_directory_iterator(
         root, std::filesystem::directory_options::skip_permission_denied, ec);
       it != std::filesystem::recursive_directory_iterator(); ++it) {
    if (ec) {
      ec.clear();
      continue;
    }
    const auto& p = it->path();
    if (p.extension() == ".vst3") {
      PluginEntry e;
      e.path = p.string();
      e.name = stem_name(p);
      out.push_back(std::move(e));
      if (it->is_directory()) it.disable_recursion_pending();
    }
  }
}

}  // namespace

std::vector<PluginEntry> scan_folder(const std::filesystem::path& root) {
  std::vector<PluginEntry> out;
  scan_recursive(root, out);
  std::sort(out.begin(), out.end(),
            [](const PluginEntry& a, const PluginEntry& b) { return a.name < b.name; });
  out.erase(std::unique(out.begin(), out.end(),
                        [](const PluginEntry& a, const PluginEntry& b) { return a.path == b.path; }),
            out.end());
  return out;
}

std::vector<PluginEntry> scan_default_folders() {
  std::vector<PluginEntry> out;
  const char* locals[] = {
    "C:\\Program Files\\Common Files\\VST3",
    "C:\\Program Files\\VST3",
  };
  for (const char* p : locals) scan_recursive(p, out);

  if (const char* pf = std::getenv("ProgramFiles")) {
    scan_recursive(std::filesystem::path(pf) / "Common Files" / "VST3", out);
  }
  if (const char* pd = std::getenv("ProgramData")) {
    scan_recursive(std::filesystem::path(pd) / "VST3", out);
  }
  if (const char* la = std::getenv("LOCALAPPDATA")) {
    scan_recursive(std::filesystem::path(la) / "VST3", out);
  }

  std::sort(out.begin(), out.end(),
            [](const PluginEntry& a, const PluginEntry& b) { return a.name < b.name; });
  out.erase(std::unique(out.begin(), out.end(),
                        [](const PluginEntry& a, const PluginEntry& b) { return a.path == b.path; }),
            out.end());
  return out;
}

}  // namespace mixbridge::vst3
