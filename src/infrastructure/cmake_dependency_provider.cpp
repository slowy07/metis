#include "metis/infrastructure/cmake_dependency_provider.hpp"

#include <fmt/format.h>

#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "metis/domain/ports/file_system.hpp"

namespace metis::infrastructure {
CMakeDependencyParser::CMakeDependencyParser(domain::ports::IFileSystem* fs)
  : fs_(fs) {}

bool CMakeDependencyParser::can_parse(const std::filesystem::path& repo_root) const {
  return fs_->exists(repo_root / "CMakeLists.txt");
}

std::vector<domain::Dependency> CMakeDependencyParser::parse(
    const std::filesystem::path& repo_root) const {
  std::vector<domain::Dependency> out;
  auto path = repo_root / "CMakeLists.txt";
  if (!fs_->exists(path)) { return out; }

  std::string content = fs_->read_file(path);
  if (content.empty()) { return out; }

  std::regex decl_re{R"(FetchContent_Declare\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s+([^)]*)\))",
                     std::regex::icase};
  std::sregex_iterator iter(content.begin(), content.end(), decl_re);
  std::sregex_iterator end;
  for (; iter != end; ++iter) {
    domain::Dependency dep;
    dep.name = (*iter)[1].str();
    dep.source = "cmake-fetchcontent";
    std::string rest = (*iter)[2].str();
    std::smatch m;
    if (std::regex_search(rest, m, std::regex{R"((?:GIT_TAG|VERSION)\s+([vV]?[0-9][^ )\r\n]*))",
                                                std::regex::icase})) {
      dep.version = m[1].str();
      if (dep.version.size() > 1 && (dep.version[0] == 'v' || dep.version[0] == 'V')) {
        dep.version = dep.version.substr(1);
      }
    }
    out.push_back(std::move(dep));
  }
  return out;
}

std::string CMakeDependencyParser::source_name() const { return "cmake-fetchcontent"; }

CMakeVersionChecker::CMakeVersionChecker(domain::ports::IHttpClient* http) : http_(http) {}

std::optional<std::string> CMakeVersionChecker::latest_version(
    const std::string& package_name) const {
  (void)package_name;
  return std::nullopt;
}

std::string CMakeVersionChecker::source_name() const { return "cmake-fetchcontent"; }

domain::ports::ManifestEditResult CMakeManifestEditor::add_dependency(
    const std::filesystem::path& repo_root, const std::string& name,
    const std::string& version) const {
  domain::ports::ManifestEditResult result;
  auto path = repo_root / "CMakeLists.txt";
  std::string content = fs_->read_file(path);

  std::string entry =
      version.empty()
          ? fmt::format("\nFetchContent_Declare({0} GIT_REPOSITORY <REPO_URL> GIT_TAG main)\n",
                        name)
          : fmt::format("\nFetchContent_Declare({0} GIT_REPOSITORY <REPO_URL> GIT_TAG {1})\n",
                        name, version);
  content += entry;
  content += "\nFetchContent_MakeAvailable(" + name + ")\n";

  if (!fs_->write_file(path, content)) {
    result.success = false;
    result.message = "Failed to write CMakeLists.txt";
    return result;
  }

  result.success = true;
  result.message = fmt::format("Added {} to CMakeLists.txt", name);
  return result;
}

CMakeManifestEditor::CMakeManifestEditor(domain::ports::IFileSystem* fs) : fs_(fs) {}

bool CMakeManifestEditor::can_edit(const std::filesystem::path& repo_root) const {
  return fs_->exists(repo_root / "CMakeLists.txt");
}

domain::ports::ManifestEditResult CMakeManifestEditor::remove_dependency(
    const std::filesystem::path& repo_root, const std::string& name) const {
  domain::ports::ManifestEditResult result;
  auto path = repo_root / "CMakeLists.txt";
  std::string content = fs_->read_file(path);

  std::regex block_re{fmt::format(R"(\s*FetchContent_Declare\s*\(\s*{0}[^\)]*\)\s*)", name),
                      std::regex::icase};
  std::string replaced = std::regex_replace(content, block_re, "");

  std::regex ma_re{fmt::format("FetchContent_MakeAvailable\\s*\\(\\s*{0}\\s*\\)\\s*", name),
                   std::regex::icase};
  replaced = std::regex_replace(replaced, ma_re, "");

  if (replaced == content) {
    result.success = false;
    result.message = fmt::format("{} not found in CMakeLists.txt", name);
    return result;
  }

  if (!fs_->write_file(path, replaced)) {
    result.success = false;
    result.message = "Failed to write CMakeLists.txt";
    return result;
  }

  result.success = true;
  result.message = fmt::format("Removed {} from CMakeLists.txt", name);
  return result;
}

domain::ports::ManifestEditResult CMakeManifestEditor::update_dependency(
    const std::filesystem::path& repo_root, const std::string& name,
    const std::string& new_version) const {
  domain::ports::ManifestEditResult result;
  auto path = repo_root / "CMakeLists.txt";
  std::string content = fs_->read_file(path);

  std::regex ver_re{
      std::string("(FetchContent_Declare\\s*\\(\\s*") + name + "[^)]*VERSION)\\s+([0-9]+\\.[0-9]+(?:\\.[0-9]+)?)",
      std::regex::icase};
  std::string replaced = std::regex_replace(content, ver_re, "$1 " + new_version);

  std::regex git_re{
      std::string("(FetchContent_Declare\\s*\\(\\s*") + name + "[^)]*GIT_TAG)\\s+([vV]?[0-9]+\\.[0-9]+(?:\\.[0-9]+)?)",
      std::regex::icase};
  replaced = std::regex_replace(replaced, git_re, "$1 v" + new_version);

  if (replaced == content) {
    result.success = false;
    result.message = fmt::format("Could not update version for {} in CMakeLists.txt", name);
    return result;
  }

  if (!fs_->write_file(path, replaced)) {
    result.success = false;
    result.message = "Failed to write CMakeLists.txt";
    return result;
  }

  result.success = true;
  result.message = fmt::format("Updated {} to {} in CMakeLists.txt", name, new_version);
  return result;
}

std::string CMakeManifestEditor::source_name() const { return "cmake-fetchcontent"; }

}  // namespace metis::infrastructure
