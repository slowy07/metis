#include "metis/infrastructure/toml_config_repository.hpp"

#include <fmt/format.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>

namespace metis::infrastructure {

namespace {

// Helper wrappers around toml++ accessors.
// lazy: these exist because toml++ returns raw pointers that need null checks.
// toml++ 3.4+ has value_or() for simple cases, but nested table/array access
// still needs manual null checking.
toml::node* safe_get(toml::table& tbl, std::string_view key) { return tbl.get(key); }

const toml::node* safe_get(const toml::table& tbl, std::string_view key) { return tbl.get(key); }

toml::table* as_table_safe(toml::node* node) {
  return (node != nullptr) ? node->as_table() : nullptr;
}

toml::array* as_array_safe(toml::node* node) {
  return (node != nullptr) ? node->as_array() : nullptr;
}

std::string get_string_safe(const toml::table& tbl, std::string_view key, std::string default_val) {
  if (const auto* node = safe_get(tbl, key)) {
    if (const auto* val = node->as_string()) {
      return {val->get()};
    }
  }

  return {default_val};
}

bool get_bool_safe(const toml::table& tbl, std::string_view key, bool default_val) {
  if (const auto* node = safe_get(tbl, key)) {
    if (const auto* val = node->as_boolean()) {
      return val->get();
    }
  }
  return default_val;
}

int get_int_safe(const toml::table& tbl, std::string_view key, int default_val) {
  if (const auto* node = safe_get(tbl, key)) {
    if (const auto* val = node->as_integer()) {
      return static_cast<int>(val->get());
    }
  }

  return default_val;
}

}  // namespace

TomlConfigRepository::TomlConfigRepository(std::unique_ptr<domain::ports::IFileSystem> fs,
                                           std::unique_ptr<domain::ports::IShellExecutor> shell)
  : fs_(std::move(fs))
  , shell_(std::move(shell)) {}

// Parses a .metis.toml config file into a ProjectConfig struct.
//
// Config structure:
//   [project]        - project name
//   [[checks]]       - array of check definitions (name, command, args, patterns)
//   [exclude]        - paths to exclude from checks
//   [output]         - which CI workflows to generate
//   [execution]      - parallel/sequential execution mode
//
// Throws std::runtime_error if:
//   - File doesn't exist
//   - TOML syntax is invalid
//   - Config validation fails (empty project name, missing checks, etc.)
// Reads one [[checks]] entry; missing fields fall back to defaults.
domain::config::Check parse_check(const toml::table& check_tbl) {
  domain::config::Check check;
  check.name = get_string_safe(check_tbl, "name", "unnamed");
  check.description = get_string_safe(check_tbl, "description", "");
  check.enabled = get_bool_safe(check_tbl, "enabled", true);
  check.command = get_string_safe(check_tbl, "command", "");
  check.timeout = get_int_safe(check_tbl, "timeout", 0);
  check.severity = get_string_safe(check_tbl, "severity", "error");

  if (const auto* args = check_tbl["args"].as_array()) {
    for (const auto& arg : *args) {
      check.args.emplace_back(arg.value_or(""));
    }
  }
  if (const auto* pats = check_tbl["patterns"].as_array()) {
    for (const auto& pat : *pats) {
      check.patterns.emplace_back(pat.value_or(""));
    }
  }
  return check;
}

void parse_checks(toml::table& tbl, domain::config::ProjectConfig& cfg) {
  auto* checks_arr = as_array_safe(safe_get(tbl, "checks"));
  if (checks_arr == nullptr) {
    return;
  }
  for (auto& item : *checks_arr) {
    if (auto* check_tbl = item.as_table()) {
      cfg.checks.push_back(parse_check(*check_tbl));
    }
  }
}

// [exclude] paths = [...]
void parse_exclude(toml::table& tbl, domain::config::ProjectConfig& cfg) {
  auto* exclude_tbl = as_table_safe(safe_get(tbl, "exclude"));
  if (exclude_tbl == nullptr) {
    return;
  }
  if (auto* paths = (*exclude_tbl)["paths"].as_array()) {
    for (auto& path_item : *paths) {
      cfg.exclude_paths.emplace_back(path_item.value_or(""));
    }
  }
}

// [output]: hook/CI generation toggles with local-hook-by-default.
void parse_output(toml::table& tbl, domain::config::ProjectConfig& cfg) {
  // Struct defaults already match the absent-section case (local hook only).
  if (auto* output = as_table_safe(safe_get(tbl, "output"))) {
    cfg.generate_local_hook = get_bool_safe(*output, "local_hook", true);
    cfg.generate_gha = get_bool_safe(*output, "github_actions", false);
    cfg.generate_gitlab_ci = get_bool_safe(*output, "gitlab_ci", false);
  }
}

// [perf]: performance budgets.
void parse_perf(toml::table& tbl, domain::config::ProjectConfig& cfg) {
  auto* perf_tbl = as_table_safe(safe_get(tbl, "perf"));
  if (perf_tbl == nullptr) {
    return;
  }
  cfg.perf.enabled = get_bool_safe(*perf_tbl, "enabled", false);
  cfg.perf.build_dir = get_string_safe(*perf_tbl, "build_dir", "build");
  cfg.perf.binary_path = get_string_safe(*perf_tbl, "binary_path", "");
  cfg.perf.max_binary_size_mb =
      static_cast<std::size_t>(get_int_safe(*perf_tbl, "max_binary_size_mb", 0));
  cfg.perf.max_build_time_sec =
      static_cast<std::size_t>(get_int_safe(*perf_tbl, "max_build_time_sec", 0));
  cfg.perf.benchmark_regex = get_string_safe(*perf_tbl, "benchmark_regex", "");
}

// [execution] parallel defaults to true when absent.
void parse_execution(toml::table& tbl, domain::config::ProjectConfig& cfg) {
  auto* exec = as_table_safe(safe_get(tbl, "execution"));
  cfg.parallel = exec == nullptr || get_bool_safe(*exec, "parallel", true);
}

domain::config::ProjectConfig TomlConfigRepository::load(const std::filesystem::path& path) {
  if (!fs_->exists(path)) {
    throw std::runtime_error("Config file not found: " + path.string());
  }

  toml::table tbl;
  try {
    tbl = toml::parse_file(path.string());
  } catch (const toml::parse_error& error_parsing) {
    throw std::runtime_error("TOML parse error at line " +
                             std::to_string(error_parsing.source().begin.line) + ": " +
                             std::string(error_parsing.description()));
  }

  domain::config::ProjectConfig cfg;

  cfg.project_name = tbl["project"]["name"].value_or("unnamed");

  parse_checks(tbl, cfg);
  parse_exclude(tbl, cfg);
  parse_output(tbl, cfg);
  parse_perf(tbl, cfg);
  parse_execution(tbl, cfg);

  if (auto err = cfg.validate(); !err.empty()) {
    throw std::runtime_error("Config validation failed: " + err);
  }

  return cfg;
}

// Finds the git repository root directory.
// Strategy:
//   1. Try `git rev-parse --show-toplevel` (fast, works everywhere)
//   2. Fall back to walking up the directory tree looking for .git/
//
// The fallback handles cases where git isn't in PATH but .git exists.
// lazy: duplicates CliGitRepository::find_repo_root() — one should be deleted.
std::filesystem::path TomlConfigRepository::find_git_root() {
  try {
    std::string out = shell_->exec("git rev-parse --show-toplevel");
    if (!out.empty()) {
      std::filesystem::path path(out);
      if (fs_->exists(path)) {
        return path;
      }
    }
  } catch (const std::exception&) {
    // fallthrough to manual search
  }

  auto dir = fs_->current_path();
  while (true) {
    if (fs_->exists(dir / ".git")) {
      return dir;
    }
    auto parent = dir.parent_path();
    if (parent == dir) {
      break;
    }
    dir = parent;
  }

  throw std::runtime_error("Not inside a Git repository or git not in PATH");
}

}  // namespace metis::infrastructure
