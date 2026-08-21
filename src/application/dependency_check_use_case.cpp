#include "metis/application/dependency_check_use_case.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace metis::application {

namespace {
std::string normalize_name(std::string_view name) {
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

DependencyCheckUseCase::DependencyCheckUseCase(
    std::unique_ptr<domain::ports::IShellExecutor> shell,
    std::unique_ptr<domain::ports::IFileSystem> file_system)
    : shell_(std::move(shell)), file_system_(std::move(file_system)) {}

domain::DependencyCheckResult DependencyCheckUseCase::execute(
    const std::filesystem::path& repo_root, const DependencyCheckOptions& opts) {
  domain::DependencyCheckResult result;
  std::vector<domain::Dependency> all_deps;

  auto conan_deps = parse_conanfile(repo_root);
  all_deps.insert(all_deps.end(), conan_deps.begin(), conan_deps.end());

  auto vcpkg_deps = parse_vcpkg_json(repo_root);
  all_deps.insert(all_deps.end(), vcpkg_deps.begin(), vcpkg_deps.end());

  auto cmake_deps = parse_cmake_fetchcontent(repo_root);
  all_deps.insert(all_deps.end(), cmake_deps.begin(), cmake_deps.end());

  for (const auto& dep : all_deps) {
    domain::DependencyValidation deps_validation;

    deps_validation.dep = dep;

    if (dep.version.empty()) {
      deps_validation.ok = false;
      deps_validation.message = "missing version";
    } else if (!is_valid_semver(dep.version)) {
      deps_validation.ok = false;
      deps_validation.message = "invalid semver: " + dep.version;
    } else {
      if (dep.source == "vcpkg" && shell_->command_exists("vcpkg")) {
        if (!vcpkg_dep_installed(dep.name)) {
          deps_validation.ok = false;
          deps_validation.message = "not installed locally";
        }
      }
    }

    result.validations.push_back(std::move(deps_validation));
  }

  check_lockfiles(repo_root, result);
  detect_duplicates(all_deps, result);
  if (opts.generate_graph) {
    generate_dot_graph(all_deps, repo_root / opts.graph_output_path);
  }

  return result;
}

std::vector<domain::Dependency> DependencyCheckUseCase::parse_conanfile(
    const std::filesystem::path& repo_root) {
  std::vector<domain::Dependency> out;
  auto path = repo_root / "conanfile.py";
  if (!file_system_->exists(path)) {
    return out;
  }

  std::string content = file_system_->read_file(path);
  if (content.empty()) {
    return out;
  }

  std::regex req_re{R"(self\.requires\s*\(\s*"([^"/@]+)/([^"/@]+)[^"]*"\s*\))"};
  std::sregex_iterator iter(content.begin(), content.end(), req_re);
  std::sregex_iterator end;
  for (; iter != end; ++iter) {
    domain::Dependency deps;
    deps.name = (*iter)[1].str();
    deps.version = (*iter)[2].str();
    deps.source = "conan";
    out.push_back(std::move(deps));
  }

  std::regex test_re{R"(self\.test_requires\s*\(\s*"([^"/@]+)/([^"/@]+)[^"]*"\s*\))"};
  iter = std::sregex_iterator(content.begin(), content.end(), test_re);
  for (; iter != end; ++iter) {
    domain::Dependency deps;
    deps.name = (*iter)[1].str();
    deps.version = (*iter)[2].str();
    deps.source = "conan";
    out.push_back(std::move(deps));
  }

  return out;
}

std::vector<domain::Dependency> DependencyCheckUseCase::parse_vcpkg_json(
    const std::filesystem::path& repo_root) {
  std::vector<domain::Dependency> out;
  auto path = repo_root / "vcpkg.json";

  if (!file_system_->exists(path)) {
    return out;
  }

  std::string content = file_system_->read_file(path);
  if (content.empty()) {
    return out;
  }

  auto dep_pos = content.find("\"dependencies\"");
  if (dep_pos == std::string::npos) {
    return out;
  }

  auto arr_start = content.find('[', dep_pos);
  if (arr_start == std::string::npos) {
    return out;
  }

  int depth = 0;
  std::string current_obj;
  bool in_string = false;
  bool escape = false;

  for (size_t i = arr_start; i < content.size(); ++i) {
    char chr = content[i];

    if (escape) {
      escape = false;
      continue;
    }

    if (chr == '\\') {
      escape = true;
      continue;
    }

    if (chr == '"') {
      in_string = !in_string;
      continue;
    }

    if (in_string) {
      continue;
    }

    if (chr == '[' || chr == '{') {
      ++depth;

      if (depth == 1) {
        continue;
      }
    } else if (chr == ']' || chr == '}') {
      --depth;
      if (depth == 0) {
        break;
      }

      if (depth == 1) {
        domain::Dependency deps;

        deps.source = "vcpkg";

        std::regex name_re{R"re("name"\s*:\s*"([^"]+)")re"};
        std::smatch match;

        if (std::regex_search(current_obj, match, name_re)) {
          deps.name = match[1].str();
        }

        std::regex ver_re{R"re("version>=?"?\s*:\s*"([^"]+)")re"};
        if (std::regex_search(current_obj, match, ver_re)) {
          deps.version = match[1].str();
        }

        if (!deps.name.empty()) {
          out.push_back(std::move(deps));
        }

        current_obj.clear();
        continue;
      }
    }

    if (depth >= 2) {
      current_obj += chr;
    }
  }

  std::regex str_re{R"re(\[\s*"([^"]+)"\s*\])re"};
  std::smatch match;
  std::string arr_slice = content.substr(arr_start, content.find(']', arr_start) - arr_start + 1);
  std::sregex_iterator sit(arr_slice.begin(), arr_slice.end(), str_re);
  std::sregex_iterator send;
  for (; sit != send; ++sit) {
    domain::Dependency deps;
    deps.name = (*sit)[1].str();
    deps.source = "vcpkg";
    out.push_back(std::move(deps));
  }

  return out;
}

std::vector<domain::Dependency> DependencyCheckUseCase::parse_cmake_fetchcontent(
    const std::filesystem::path& repo_root) {
  std::vector<domain::Dependency> out;
  auto path = repo_root / "CMakeLists.txt";

  if (!file_system_->exists(path)) {
    return out;
  }

  std::string content = file_system_->read_file(path);
  if (content.empty()) {
    return out;
  }

  std::regex fc_re{
      R"(FetchContent_Declare\s*\(\s*([A-Za-z0-9_\-]+)[^\)]*VERSION\s+([0-9]+\.[0-9]+(?:\.[0-9]+)?)\s*[^\)]*\))",
      std::regex::icase};
  std::sregex_iterator iter(content.begin(), content.end(), fc_re);
  std::sregex_iterator end;
  for (; iter != end; ++iter) {
    domain::Dependency domain_deps;
    domain_deps.name = (*iter)[1].str();
    domain_deps.version = (*iter)[2].str();
    domain_deps.source = "cmake-fetchcontent";
    out.push_back(std::move(domain_deps));
  }

  std::regex git_re{
      R"(FetchContent_Declare\s*\(\s*([A-Za-z0-9_\-]+)[^\)]*GIT_TAG\s+([vV]?[0-9]+\.[0-9]+(?:\.[0-9]+)?)\s*[^\)]*\))",
      std::regex::icase};
  iter = std::sregex_iterator(content.begin(), content.end(), git_re);
  for (; iter != end; ++iter) {
    domain::Dependency domain_deps;
    domain_deps.name = (*iter)[1].str();
    domain_deps.version = (*iter)[2].str();
    if (!domain_deps.version.empty() &&
        (domain_deps.version[0] == 'v' || domain_deps.version[0] == 'V')) {
      domain_deps.version = domain_deps.version.substr(1);
    }
    domain_deps.source = "cmake-fetchcontent";
    out.push_back(std::move(domain_deps));
  }

  return out;
}

bool DependencyCheckUseCase::is_valid_semver(std::string_view version) {
  if (version.empty()) {
    return false;
  }

  std::regex semver_re{R"(^[vV]?([0-9]+)(\.[0-9]+)?(\.[0-9]+)?([+\-].*)?$)"};
  return std::regex_match(std::string(version), semver_re);
}

bool DependencyCheckUseCase::conan_dep_installed(const std::string& name) const {
  try {
    auto res = shell_->exec_captured("conan list \"*" + name + "*\" 2>/dev/null");
    return res.exit_code_ == 0 && res.output_.find(name) != std::string::npos;
  } catch (...) {
    return false;
  }
}

bool DependencyCheckUseCase::vcpkg_dep_installed(const std::string& name) const {
  try {
    auto res = shell_->exec_captured("vcpkg list " + name + " 2>/dev/null");
    return res.exit_code_ == 0 && res.output_.find(name) != std::string::npos;
  } catch (...) {
    return false;
  }
}

void DependencyCheckUseCase::check_lockfiles(const std::filesystem::path& repo_root,
                                             domain::DependencyCheckResult& out) const {
  bool has_conan = file_system_->exists(repo_root / "conanfile.py");
  bool has_vcpkg = file_system_->exists(repo_root / "vckpg.json");

  if (has_conan && !file_system_->exists(repo_root / "conan.lock")) {
    out.lockfile_issues.emplace_back("conanfile.py exists but conan.lock is missing");
  }

  if (has_vcpkg) {
    bool has_lock = file_system_->exists(repo_root / "vcpkg-configuration.json") ||
                    file_system_->exists(repo_root / "vcpkg-manifest-install.log");

    if (has_lock) {
      out.lockfile_issues.emplace_back(
          "vckpg.json exists but no lockfile (vcpkg-configuration.json)");
    }
  }
}

void DependencyCheckUseCase::detect_duplicates(const std::vector<domain::Dependency>& all,
                                               domain::DependencyCheckResult& out) {
  std::map<std::string, std::set<std::string>> seen;
  for (const auto& deps : all) {
    seen[normalize_name(deps.name)].insert(deps.source);
  }

  for (const auto& [name, sources] : seen) {
    if (sources.size() > 1) {
      std::string message = name + " declared in: ";
      bool first = true;

      for (const auto& src : sources) {
        if (!first) {
          message += ", ";
        }

        message += src;
      }

      out.duplicates.push_back(message);
    }
  }
}

void DependencyCheckUseCase::generate_dot_graph(const std::vector<domain::Dependency>& all,
                                                const std::filesystem::path& out_path) {
  std::ostringstream dot;
  dot << "digraph dependencies {\n";
  dot << "  rankdir=LR;\n";
  dot << "  node [shape=box, fontname=Helvetica];\n";
  dot << "  \"project\" [style=filled, fillcolor=lightblue];\n";

  for (const auto& dep : all) {
    std::string label = dep.name + "\\n" + dep.version;
    dot << fmt::format(R"(  "{}" [label="{}"];)"
                       "\n",
                       dep.name, label);
    dot << fmt::format(R"(  "project" -> "{}";)"
                       "\n",
                       dep.name);
  }

  dot << "}\n";

  std::ofstream ofs(out_path);

  if (ofs) {
    ofs << dot.str();
  }
}

}  // namespace metis::application
