#include "sniffercommit/infrastructure/toml_config_repository.hpp"

#include <fmt/format.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>

namespace sniffercommit::infrastructure {

TomlConfigRepository::TomlConfigRepository(std::unique_ptr<domain::ports::IFileSystem> fs,
                                           std::unique_ptr<domain::ports::IShellExecutor> shell)
    : fs_(std::move(fs)), shell_(std::move(shell)) {}

domain::config::ProjectConfig TomlConfigRepository::load(const std::filesystem::path& path) {
  if (!fs_->exists(path)) {
    throw std::runtime_error("Config file not found: " + path.string());
  }

  toml::table tbl;
  try {
    tbl = toml::parse_file(path.string());
  } catch (const toml::parse_error& error_parsing) {
    throw std::runtime_error("TOML parse error: " + std::string(error_parsing.description()));
  }

  domain::config::ProjectConfig cfg;

  if (auto* project = tbl.at("project").as_table()) {
    cfg.project_name = project->at("name").value_or("unnamed");
  }

  if (auto* checks_arr = tbl.at("checks").as_array()) {
    for (auto& item : *checks_arr) {
      if (auto* check_tbl = item.as_table()) {
        domain::config::Check check;
        check.name = check_tbl->at("name").value_or("unnamed");
        check.command = check_tbl->at("command").value_or("");

        if (auto* args = check_tbl->at("args").as_array()) {
          for (auto& arg : *args) {
            check.args.emplace_back(arg.value_or(""));
          }
        }

        if (auto* pats = check_tbl->at("patterns").as_array()) {
          for (auto& pat : *pats) {
            check.patterns.emplace_back(pat.value_or(""));
          }
        }

        cfg.checks.emplace_back(std::move(check));
      }
    }
  }

  if (auto* exclude_tbl = tbl.at("exclude").as_table()) {
    if (auto* paths = exclude_tbl->at("paths").as_array()) {
      for (auto& path_item : *paths) {
        cfg.exclude_paths.emplace_back(path_item.value_or(""));
      }
    }
  }

  if (auto* output = tbl.at("output").as_table()) {
    cfg.generate_local_hook = output->at("local_hook").value_or(true);
    cfg.generate_gha = output->at("github_actions").value_or(false);
  }

  if (auto* exec = tbl.at("execution").as_table()) {
    cfg.parallel = exec->at("parallel").value_or(true);
  } else {
    cfg.parallel = true;
  }

  if (auto err = cfg.validate(); !err.empty()) {
    throw std::runtime_error("Config validation failed: " + err);
  }

  return cfg;
}

bool TomlConfigRepository::save(const std::filesystem::path& path,
                                const domain::config::ProjectConfig& cfg) {
  auto fmt_str = [](const std::string& str) -> std::string { return fmt::format("\"{}\"", str); };

  auto fmt_str_array = [&fmt_str](const std::vector<std::string>& arr) -> std::string {
    if (arr.empty()) {
      return "[]";
    }
    std::string result = "[";
    for (size_t i = 0; i < arr.size(); ++i) {
      if (i > 0) {
        result += ", ";
      }
      result += fmt_str(arr.at(i));
    }
    result += "]";
    return result;
  };

  std::string content;
  content += "[project]\n";
  content += "name = " + fmt_str(cfg.project_name) + "\n\n";

  for (const auto& check : cfg.checks) {
    content += "[[checks]]\n";
    content += "name = " + fmt_str(check.name) + "\n";
    content += "command = " + fmt_str(check.command) + "\n";

    if (!check.args.empty()) {
      content += "args = " + fmt_str_array(check.args) + "\n";
    }

    if (!check.patterns.empty()) {
      content += "patterns = " + fmt_str_array(check.patterns) + "\n";
    }

    content += "\n";
  }

  if (!cfg.exclude_paths.empty()) {
    content += "[exclude]\n";
    content += "paths = " + fmt_str_array(cfg.exclude_paths) + "\n\n";
  }

  content += "[output]\n";
  content += "local_hook = " + std::string(cfg.generate_local_hook ? "true" : "false") + "\n";
  content += "github_actions = " + std::string(cfg.generate_gha ? "true" : "false") + "\n\n";

  content += "[execution]\n";
  content += "parallel = " + std::string(cfg.parallel ? "true" : "false") + "\n";

  return fs_->write_file(path, content);
}

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

}  // namespace sniffercommit::infrastructure
