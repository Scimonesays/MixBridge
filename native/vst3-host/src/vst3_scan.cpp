#include "mixbridge/vst3_scan.hpp"

#include <algorithm>
#include <cstdlib>
#include <optional>

namespace mixbridge::vst3 {
namespace {

std::string stem_name(const std::filesystem::path& p) {
  auto s = p.stem().string();
  if (s.size() > 5 && s.ends_with(".vst3")) s.resize(s.size() - 5);
  return s;
}

std::optional<std::string> env_value(const char* name) {
#ifdef _WIN32
  char* value = nullptr;
  size_t len = 0;
  if (_dupenv_s(&value, &len, name) != 0 || !value) return std::nullopt;
  std::string result(value);
  std::free(value);
  return result;
#else
  if (const char* value = std::getenv(name)) return std::string(value);
  return std::nullopt;
#endif
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

  if (auto pf = env_value("ProgramFiles")) {
    scan_recursive(std::filesystem::path(*pf) / "Common Files" / "VST3", out);
  }
  if (auto pd = env_value("ProgramData")) {
    scan_recursive(std::filesystem::path(*pd) / "VST3", out);
  }
  if (auto la = env_value("LOCALAPPDATA")) {
    scan_recursive(std::filesystem::path(*la) / "VST3", out);
  }

  std::sort(out.begin(), out.end(),
            [](const PluginEntry& a, const PluginEntry& b) { return a.name < b.name; });
  out.erase(std::unique(out.begin(), out.end(),
                        [](const PluginEntry& a, const PluginEntry& b) { return a.path == b.path; }),
            out.end());
  return out;
}

}  // namespace mixbridge::vst3
