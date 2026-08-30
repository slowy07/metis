#include "metis/infrastructure/vcpkg_dependency_provider.hpp"

#include <fmt/format.h>

#include <regex>
#include <string>

namespace metis::infrastructure {

VcpkgDependencyParser::VcpkgDependencyParser(domain::ports::IFileSystem* fs)
  : fs_(fs) {}

bool VcpkgDependencyParser::can_parse(const std::filesystem::path& repo_root) const {
  return fs_->exists(repo_root / "vcpkg.json");
}

std::vector<domain::Dependency> VcpkgDependencyParser::parse(
    const std::filesystem::path& repo_root) const {
  std::vector<domain::Dependency> out;
  auto path = repo_root / "vcpkg.json";
  if (!fs_->exists(path)) {
    return out;
  }

  std::string content = fs_->read_file(path);
  auto dep_pos = content.find("\"dependencies\"");
  if (content.empty() || dep_pos == std::string::npos) {
    return out;
  }

  auto arr_start = content.find('[', dep_pos);
  auto arr_end = content.find(']', arr_start);
  if (arr_start == std::string::npos || arr_end == std::string::npos) {
    return out;
  }

  const std::string slice = content.substr(arr_start, arr_end - arr_start + 1);

  static const std::regex obj_re{R"re(\{[^{}]*\})re"};
  for (std::sregex_iterator it(slice.begin(), slice.end(), obj_re); it != std::sregex_iterator{};
       ++it) {
    static const std::regex name_re{R"re("name"\s*:\s*"([^"]+)")re"};
    static const std::regex ver_re{R"re("version>=?"?\s*:\s*"([^"]+)")re"};
    std::smatch match;
    std::string entry = it->str();
    if (!std::regex_search(entry, match, name_re)) {
      continue;
    }

    domain::Dependency dep;
    dep.name = match[1].str();
    dep.source = "vcpkg";
    if (std::regex_search(entry, match, ver_re)) {
      dep.version = match[1].str();
    }
    out.push_back(std::move(dep));
  }

  if (out.empty()) {
    static const std::regex str_re{R"re("([^"]+)")re"};
    for (std::sregex_iterator it(slice.begin(), slice.end(), str_re); it != std::sregex_iterator{};
         ++it) {
      domain::Dependency dep;
      dep.name = (*it)[1].str();
      dep.source = "vcpkg";
      out.push_back(std::move(dep));
    }
  }

  return out;
}

std::string VcpkgDependencyParser::source_name() const { return "vcpkg"; }

VcpkgVersionChecker::VcpkgVersionChecker(domain::ports::IShellExecutor* shell)
  : shell_(shell) {}

std::optional<std::string> VcpkgVersionChecker::latest_version(
    const std::string& package_name) const {
  if (!shell_->command_exists("vcpkg")) {
    return std::nullopt;
  }
  try {
    auto cmd = fmt::format("vcpkg search {} 2>/dev/null | grep \"^{}\" | head -n 1", package_name,
                           package_name);
    auto res = shell_->exec_captured(cmd);
    if (res.exit_code_ != 0 || res.output_.empty()) {
      return std::nullopt;
    }
    auto space = res.output_.find(' ');
    if (space != std::string::npos) {
      std::string ver = res.output_.substr(space + 1);
      ver.erase(ver.find_last_not_of(" \n\r\t") + 1);
      if (auto hash = ver.find('#'); hash != std::string::npos) {
        ver = ver.substr(0, hash);
      }
      return ver;
    }
  } catch (...) {
    return std::nullopt;
  }
  return std::nullopt;
}

std::string VcpkgVersionChecker::source_name() const { return "vcpkg"; }

VcpkgManifestEditor::VcpkgManifestEditor(domain::ports::IFileSystem* fs)
  : fs_(fs) {}

bool VcpkgManifestEditor::can_edit(const std::filesystem::path& repo_root) const {
  return fs_->exists(repo_root / "vcpkg.json");
}

domain::ports::ManifestEditResult VcpkgManifestEditor::add_dependency(
    const std::filesystem::path& repo_root, const std::string& name, const std::string& version) const {
  domain::ports::ManifestEditResult result;
  auto path = repo_root / "vcpkg.json";
  std::string content = fs_->read_file(path);

  const std::string entry_payload =
      version.empty() ? fmt::format("\"{}\"", name)
                      : fmt::format(R"({{"name": "{}", "version>=": "{}"}})", name, version);
  std::string deps_section = fmt::format(R"(,
  "dependencies": [
    {0}
  ])",
                                         entry_payload);

  auto dep_pos = content.find("\"dependencies\"");
  if (dep_pos == std::string::npos) {
    auto close_brace = content.find_last_of('}');
    if (close_brace == std::string::npos) {
      result.success = false;
      result.message = "Invalid vcpkg.json format";
      return result;
    }
    content.insert(close_brace, deps_section);
  } else {
    auto arr_start = content.find('[', dep_pos);
    auto arr_end = content.find(']', arr_start);
    if (arr_start == std::string::npos || arr_end == std::string::npos) {
      result.success = false;
      result.message = "Invalid dependencies array in vcpkg.json";
      return result;
    }
    bool array_empty = content.substr(arr_start + 1, arr_end - arr_start - 1).find_first_not_of(" \t\r\n") ==
                       std::string::npos;
    std::string entry = array_empty ? fmt::format("\n    {0}", entry_payload)
                                    : fmt::format("\n    {0},", entry_payload);
    content.insert(arr_start + 1, entry);
  }

  if (!fs_->write_file(path, content)) {
    result.success = false;
    result.message = "Failed to write vcpkg.json";
    return result;
  }

  result.success = true;
  result.message = fmt::format("Added {} to vcpkg.json", name);
  return result;
}

domain::ports::ManifestEditResult VcpkgManifestEditor::remove_dependency(
    const std::filesystem::path& repo_root, const std::string& name) const {
  domain::ports::ManifestEditResult result;
  auto path = repo_root / "vcpkg.json";
  std::string content = fs_->read_file(path);

  std::string obj_pat = "\\s*\\{\\s*\"name\"\\s*:\\s*\"" + name + "\"[^}]*\\}\\s*,?\\n?";
  std::string replaced = std::regex_replace(content, std::regex(obj_pat), "");

  if (replaced == content) {
    std::string str_pat = "\\s*\"" + name + "\"\\s*,?\\n?";
    replaced = std::regex_replace(content, std::regex(str_pat), "");
  }

  if (replaced == content) {
    result.success = false;
    result.message = fmt::format("{} not found in vcpkg.json", name);
    return result;
  }
  std::regex dangling_comma{R"((,)(\s*[\]}]))"};
  replaced = std::regex_replace(replaced, dangling_comma, "$2");

  if (!fs_->write_file(path, replaced)) {
    result.success = false;
    result.message = "Failed to write vcpkg.json";
    return result;
  }

  result.success = true;
  result.message = fmt::format("Removed {} from vcpkg.json", name);
  return result;
}

domain::ports::ManifestEditResult VcpkgManifestEditor::update_dependency(
    const std::filesystem::path& repo_root, const std::string& name,
    const std::string& new_version) const {
  domain::ports::ManifestEditResult result;
  auto path = repo_root / "vcpkg.json";
  std::string content = fs_->read_file(path);

  std::regex ver_re{
      std::string("(\\\"name\\\"\\s*:\\s*\\\"") + name +
      "\\\"[^}]*\\\"version>=?\\\"?\\s*:\\s*)(\\\"[^\\\"]*\\\")"};
  std::string replaced =
      std::regex_replace(content, ver_re, "$1\"" + std::string(new_version) + "\"");

  if (replaced == content) {
    result.success = false;
    result.message = fmt::format("Could not update version for {} in vcpkg.json", name);
    return result;
  }

  if (!fs_->write_file(path, replaced)) {
    result.success = false;
    result.message = "Failed to write vcpkg.json";
    return result;
  }

  result.success = true;
  result.message = fmt::format("Updated {} to {} in vcpkg.json", name, new_version);
  return result;
}

std::string VcpkgManifestEditor::source_name() const { return "vcpkg"; }
}  // namespace metis::infrastructure
