#include "metis/application/dependency_check_use_case.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <map>
#include <ostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "metis/domain/ports/dependency_parser.hpp"

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
  : shell_(std::move(shell))
  , file_system_(std::move(file_system)) {}

void DependencyCheckUseCase::register_parser(
    std::unique_ptr<domain::ports::IDependencyParser> parser) {
  parsers_.add_parser(std::move(parser));
}

void DependencyCheckUseCase::register_version_checker(
    std::unique_ptr<domain::ports::IDependencyVersionChecker> checker) {
  version_checkers_.add_checker(std::move(checker));
}

domain::DependencyCheckResult DependencyCheckUseCase::execute(
    const std::filesystem::path& repo_root, const DependencyCheckOptions& opts) {
  domain::DependencyCheckResult result;
  std::vector<domain::Dependency> all_deps = parsers_.parse_all(repo_root);

  if (opts.check_updates) {
    for (auto& dep : all_deps) {
      auto latest = version_checkers_.latest_version(dep);
      if (latest.has_value() && !latest->empty()) {
        dep.latest_version = latest;
        dep.has_update = !dep.version.empty() && dep.version != *latest;
      }
    }
  }

  if (opts.display_tree) {
    display_tree(all_deps);
  }

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
      bool installed = true;

      if (dep.source == "conan" && shell_->command_exists("conan")) {
        installed = conan_dep_installed(dep.name);
      } else if (dep.source == "vcpkg" && shell_->command_exists("vcpkg")) {
        installed = vcpkg_dep_installed(dep.name);
      }

      if (!installed) {
        deps_validation.ok = false;
        deps_validation.message = "not installed locally";
      }
    }

    result.validations.push_back(std::move(deps_validation));
  }

  if (opts.check_updates) {
    for (const auto& dep : all_deps) {
      if (dep.is_outdated()) {
        result.outdated.push_back(dep);
      }
    }
  }

  check_lockfiles(repo_root, result);
  detect_duplicates(all_deps, result);
  if (opts.generate_graph) {
    generate_dot_graph(all_deps, repo_root / opts.graph_output_path);
  }

  return result;
}

bool DependencyCheckUseCase::is_valid_semver(std::string_view version) {
  static const std::regex semver_re{R"(^[vV]?([0-9]+)(\.[0-9]+)?(\.[0-9]+)?([+\-].*)?$)"};
  return !version.empty() && std::regex_match(std::string(version), semver_re);
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
  bool has_vcpkg = file_system_->exists(repo_root / "vcpkg.json");

  if (has_conan && !file_system_->exists(repo_root / "conan.lock")) {
    out.lockfile_issues.emplace_back("conanfile.py exists but conan.lock is missing");
  }

  if (has_vcpkg && !file_system_->exists(repo_root / "vcpkg-configuration.json")) {
    out.lockfile_issues.emplace_back(
        "vcpkg.json exists but no lockfile (vcpkg-configuration.json)");
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

void DependencyCheckUseCase::display_tree(const std::vector<domain::Dependency>& all,
                                          std::ostream& out) {
  if (all.empty()) {
    out << "No dependencies found.\n";
    return;
  }

  std::map<std::string, std::vector<domain::Dependency>> by_source;
  for (const auto& dep : all) {
    by_source[dep.source].push_back(dep);
  }

  out << "\nDependecy Tree:\n\n";
  out << "Project\n";

  std::size_t source_idx = 0;
  const std::size_t source_count = by_source.size();

  for (const auto& [source, deps] : by_source) {
    const bool is_last_source = (source_idx + 1 == source_count);
    const std::string src_branch = is_last_source ? "└── " : "├── ";
    const std::string src_indent = is_last_source ? "    " : "│   ";

    out << src_branch << source << "\n";

    for (std::size_t i = 0; i < deps.size(); ++i) {
      const bool is_last_dep = (i + 1 == deps.size());
      const std::string dep_branch = is_last_dep ? "└── " : "├── ";

      out << src_indent << dep_branch << deps[i].name;

      if (!deps[i].version.empty()) {
        out << " @ " << deps[i].version;
      }

      out << "\n";
    }

    ++source_idx;
  }

  out << "\n";
}

}  // namespace metis::application
