#include "sniffercommit/project_config.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>
#include <utility>

#include <fmt/format.h>

namespace sniffercommit::project {
std::string Check::validate() const noexcept {
  if (name.empty()) {
    return "Check name cannot be empty";
  }

  if (command.empty()) {
    return fmt::format("Check `{}` missing require `command`", name);
  }

  return "";
}

std::string ProjectConfig::validate() const noexcept {
  if (project_name.empty()) {
    return "Project name cannot be empty";
  }

  if (checks.empty()) {
    return "Config must contain at least one [[checks]] entry";
  }

  for (const auto& check : checks) {
    if (auto err = check.validate(); !err.empty()) {
      return err;
    }
  }

  // check duplicate name
  for (size_t i = 0; i < checks.size(); ++i) {
    for (size_t j = i + 1; j < checks.size(); ++j) {
      if (checks[i].name == checks[j].name) {
        return fmt::format("Duplicate check name: `{}`", checks[i].name);
      }
    }
  }

  return "";
}

bool ProjectConfig::has_command(std::string_view cmd) const noexcept {
  for (const auto& check : checks) {
    if (check.command == cmd) {
      return true;
    }
  }

  return false;
}

bool ProjectConfig::has_matching_checks(const std::string& file) const noexcept {
  for (const auto& check : checks) {
    for (const auto& pattern : check.patterns) {
      if (pattern == "*") {
        return true;
      }

      if (pattern.starts_with("*.") && file.ends_with(pattern.substr(1))) {
        return true;
      }

      if (pattern.ends_with("/**") &&
          file.starts_with(pattern.substr(0, pattern.size() - 3) + "/")) {
        return true;
      }

      if (pattern.starts_with("**/")) {
        std::string suffix = pattern.substr(3);
        if (file.ends_with(suffix)) return true;
      }

      if (file.starts_with(pattern) || file == pattern) {
        return true;
      }
    }
  }

  return false;
}

// INFO: load from TOML files
ProjectConfig load(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("Config file not found: " + path.string());
  }

  toml::table tbl;

  try {
    tbl = toml::parse_file(path.string());
  } catch (const toml::parse_error& error_parsing) {
    throw std::runtime_error("TOML parse error: " + std::string(error_parsing.description()));
  }

  ProjectConfig cfg;

  if (auto project = tbl["project"].as_table()) {
    cfg.project_name = (*project)["name"].value_or("unnamed");
  }

  if (auto* checks_arr = tbl["checks"].as_array()) {
    for (auto& item : *checks_arr) {
      if (auto* check_tbl = item.as_table()) {
        Check c;
        c.name = (*check_tbl)["name"].value_or("unnamed");
        c.command = (*check_tbl)["command"].value_or("");

        // args arrays
        if (auto* args = (*check_tbl)["args"].as_array()) {
          for (auto& a : *args) {
            c.args.push_back(a.value_or(""));
          }
        }

        // patterns array
        if (auto* pats = (*check_tbl)["patterns"].as_array()) {
          for (auto& p : *pats) {
            c.patterns.push_back(p.value_or(""));
          }
        }

        cfg.checks.push_back(std::move(c));
      }
    }
  }

  // [exclude]
  if (auto* exclude_tbl = tbl["exclude"].as_table()) {
    if (auto* paths = (*exclude_tbl)["paths"].as_array()) {
      for (auto& p : *paths) {
        cfg.exclude_paths.push_back(p.value_or(""));
      }
    }
  }

  // [output]
  if (auto* output = tbl["output"].as_table()) {
    cfg.generate_local_hook = (*output)["local_hook"].value_or(true);
    cfg.generate_gha = (*output)["github_actions"].value_or(false);
  }

  // [execution]
  if (auto* exec = tbl["execution"].as_table()) {
    cfg.parallel = (*exec)["parallel"].value_or(true);
  } else {
    cfg.parallel = true;
  }

  // NOTE: validate after loading
  if (auto err = cfg.validate(); !err.empty()) {
    throw std::runtime_error("Config validation failed: " + err);
  }

  return cfg;
}

// INFO: saving TOML
bool save(const std::filesystem::path& path, const ProjectConfig& cfg) {
  std::ofstream out(path, std::ios::trunc);

  if (!out) {
    return false;
  }

  auto fmt_str = [](const std::string& s) -> std::string { return fmt::format("\"{}\"", s); };

  auto fmt_str_array = [&fmt_str](const std::vector<std::string>& arr) -> std::string {
    if (arr.empty()) {
      return "[]";
    }

    std::string result = "[";
    for (size_t i = 0; i < arr.size(); ++i) {
      if (i > 0) result += ", ";
      result += fmt_str(arr[i]);
    }

    result += "]";
    return result;
  };

  out << "[project]\n";
  out << "name = " << fmt_str(cfg.project_name) << "\n\n";

  for (const auto& check : cfg.checks) {
    out << "[[checks]]\n";
    out << "name = " << fmt_str(check.name) << "\n";
    out << "command = " << fmt_str(check.command) << "\n";

    if (!check.args.empty()) {
      out << "args = " << fmt_str_array(check.args) << "\n";
    }

    if (!check.patterns.empty()) {
      out << "patterns = " << fmt_str_array(check.patterns) << "\n";
    }

    out << "\n";
  }

  if (!cfg.exclude_paths.empty()) {
    out << "[exclude]\n";
    out << "paths = " << fmt_str_array(cfg.exclude_paths) << "\n\n";
  }

  out << "[output]\n";
  out << "local_hook = " << (cfg.generate_local_hook ? "true" : "false") << "\n";
  out << "github_actions = " << (cfg.generate_gha ? "true" : "false") << "\n\n";

  out << "[execution]\n";
  out << "parallel = " << (cfg.parallel ? "true" : "false") << "\n";

  return out.good();
}

std::string generate_default(const std::string& project_name, const std::string& fallback_style) {
  return fmt::format(
      R"([project]
name = "{}"

[[checks]]
name = "clang-format"
command = "clang-format"
args = [
  "-i",
  "--fallback-style={}",
  "-style=file"
]
patterns = ["*.cpp", "*.hpp", "*.h", "*.cc"]

[[checks]]
name = "trailing-whitespace"
command = "grep"
args = ["-E", "--text", "[[:space:]]+$"]
patterns = ["*"]

[exclude]
paths = ["build/", "third_party/", ".git/"]

[output]
local_hook = true
github_actions = false

[execution]
parallel = true
)",
      project_name, fallback_style);
}

}  // namespace sniffercommit::project
