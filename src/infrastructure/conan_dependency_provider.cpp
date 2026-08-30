#include "metis/infrastructure/conan_dependency_provider.hpp"

#include <fmt/format.h>

#include <cctype>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "metis/domain/ports/dependency_manifest_editor.hpp"
#include "metis/domain/ports/file_system.hpp"

namespace metis::infrastructure {
namespace {
std::string normalize_conan_name(std::string_view name) {
  std::string out;

  out.reserve(name.size());

  for (char chr : name) {
    out += static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));

    if (chr == '-') {
      out.back() = '_';
    }
  }

  return out;
}
}  // namespace

ConanDependencyParser::ConanDependencyParser(domain::ports::IFileSystem* fs)
  : fs_(fs) {}

bool ConanDependencyParser::can_parse(const std::filesystem::path& repo_root) const {
  return fs_->exists(repo_root / "conanfile.py");
}

std::vector<domain::Dependency> ConanDependencyParser::parse(
    const std::filesystem::path& repo_root) const {
  std::vector<domain::Dependency> out;
  auto path = repo_root / "conanfile.py";
  if (!fs_->exists(path)) {
    return out;
  }

  std::string content = fs_->read_file(path);
  if (content.empty()) {
    return out;
  }

  std::regex req_re{R"(self\.requires\s*\(\s*"([^"/@]+)/([^"/@]+)[^"]*"\s*\))"};
  std::sregex_iterator iter(content.begin(), content.end(), req_re);
  std::sregex_iterator end;

  for (; iter != end; ++iter) {
    domain::Dependency dep;
    dep.name = (*iter)[1].str();
    dep.version = (*iter)[2].str();
    dep.source = "conan";
    out.push_back(std::move(dep));
  }

  std::regex test_re{R"(self\.test_requires\s*\(\s*"([^"/@]+)/([^"/@]+)[^"]*"\s*\))"};
  iter = std::sregex_iterator(content.begin(), content.end(), test_re);

  for (; iter != end; ++iter) {
    domain::Dependency dep;
    dep.name = (*iter)[1].str();
    dep.version = (*iter)[2].str();
    dep.source = "conan";
    out.push_back(std::move(dep));
  }

  return out;
}

std::string ConanDependencyParser::source_name() const { return "conan"; }

ConanVersionChecker::ConanVersionChecker(domain::ports::IShellExecutor* shell)
  : shell_(shell) {}

std::optional<std::string> ConanVersionChecker::latest_version(
    const std::string& package_name) const {
  if (!shell_->command_exists("conan")) {
    return std::nullopt;
  }

  try {
    auto cmd =
        fmt::format("conan search {} -r conancenter --raw 2>/dev/null | head -n 1", package_name);
    auto res = shell_->exec_captured(cmd);

    if (res.exit_code_ != 0 || res.output_.empty()) {
      return std::nullopt;
    }

    auto slash = res.output_.find('/');
    if (slash != std::string::npos) {
      auto ver_start = slash + 1;
      auto ver_end = res.output_.find('/', ver_start);
      if (ver_end == std::string::npos) {
        ver_end = res.output_.find('@', ver_start);
      }
      std::string ver = res.output_.substr(ver_start, ver_end - ver_start);
      ver.erase(ver.find_last_not_of(" \n\r\t") + 1);
      if (!ver.empty()) {
        return ver;
      }
    }

  } catch (...) {
    return std::nullopt;
  }

  return std::nullopt;
}

std::string ConanVersionChecker::source_name() const { return "conan"; }

ConanManifestEditor::ConanManifestEditor(domain::ports::IFileSystem* fs)
  : fs_(fs) {}

bool ConanManifestEditor::can_edit(const std::filesystem::path& repo_root) const {
  return fs_->exists(repo_root / "conanfile.py");
}

domain::ports::ManifestEditResult ConanManifestEditor::add_dependency(
    const std::filesystem::path& repo_root, const std::string& name,
    const std::string& version) const {
  domain::ports::ManifestEditResult result;
  auto path = repo_root / "conanfile.py";
  std::string content = fs_->read_file(path);

  std::string ver = version.empty() ? "0.1.0" : version;
  std::string line =
      fmt::format("        self.requires(\"{}/{}\")", normalize_conan_name(name), ver);

  auto pos = content.find("def requirements(self):");
  if (pos == std::string::npos) {
    result.success = false;
    result.message = "Could not find requirements() method in conanfile.py";
    return result;
  }

  auto next_method = content.find("\n    def ", pos + 1);
  auto insert_pos = content.rfind('\n', next_method);
  if (insert_pos == std::string::npos || insert_pos <= pos) {
    insert_pos = content.find('\n', pos);
  }

  content.insert(insert_pos + 1, line + "\n");
  if (!fs_->write_file(path, content)) {
    result.success = false;
    result.message = "Failed to write conanfile.py";
    return result;
  }

  result.success = true;
  result.message = fmt::format("Added {}@{} to conanfile.py", name, ver);
  return result;
}

domain::ports::ManifestEditResult ConanManifestEditor::remove_dependency(
    const std::filesystem::path& repo_root, const std::string& name) const {
  domain::ports::ManifestEditResult result;

  auto path = repo_root / "conanfile.py";
  std::string content = fs_->read_file(path);

  std::regex req_re{
      fmt::format(R"(self\.requires\s*\(\s*"{}[/][^"]*"\s*\)\n?)", normalize_conan_name(name))};
  std::string replaced = std::regex_replace(content, req_re, "");

  if (replaced == content) {
    result.success = false;
    result.message = fmt::format("{} not found in conanfile.py", name);
    return result;
  }

  if (!fs_->write_file(path, replaced)) {
    result.success = false;
    result.message = "Failed to write conanfile.py";
    return result;
  }

  result.success = true;
  result.message = fmt::format("Removed {} from conanfile.py", name);
  return result;
}

domain::ports::ManifestEditResult ConanManifestEditor::update_dependency(
    const std::filesystem::path& repo_root, const std::string& name,
    const std::string& new_version) const {
  domain::ports::ManifestEditResult result;
  auto path = repo_root / "conanfile.py";
  std::string content = fs_->read_file(path);

  std::regex req_re{std::string("(self\\.requires\\s*\\(\\s*\"") + normalize_conan_name(name) +
                    ")(/[^\"/@]+)([^\"]*\"\\s*\\))"};

  std::string replaced = std::regex_replace(content, req_re, fmt::format("$1/{}$3", new_version));

  if (replaced == content) {
    result.success = false;
    result.message = fmt::format("{} not found in conanfile.py", name);
    return result;
  }

  if (!fs_->write_file(path, replaced)) {
    result.success = false;
    result.message = "Failed to write conanfile.py";
    return result;
  }

  result.success = true;
  result.message = fmt::format("Update {} to {} in conanfile.py", name, new_version);

  return result;
}

std::string ConanManifestEditor::source_name() const { return "conan"; }

}  // namespace metis::infrastructure
