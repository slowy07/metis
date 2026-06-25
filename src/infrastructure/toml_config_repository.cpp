#include "sniffercommit/infrastructure/toml_config_repository.hpp"

#include <fmt/format.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>

namespace sniffercommit::infrastructure {

namespace {

toml::node* safe_get(toml::table& tbl, std::string_view key) { return tbl.get(key); }

const toml::node* safe_get(const toml::table& tbl, std::string_view key) { return tbl.get(key); }

toml::table* as_table_safe(toml::node* node) {
  return (node != nullptr) ? node->as_table() : nullptr;
}

toml::array* as_array_safe(toml::node* node) {
  return (node != nullptr) ? node->as_array() : nullptr;
}

std::string get_string_safe(const toml::table& tbl, std::string_view key,
                            std::string_view default_val) {
  if (const auto* node = safe_get(tbl, key)) {
    if (const auto* val = node->as_string()) {
      return std::string(val->get());
    }
  }

  return std::string(default_val);
}

bool get_bool_safe(const toml::table& tbl, std::string_view key, bool default_val) {
  if (const auto* node = safe_get(tbl, key)) {
    if (const auto* val = node->as_boolean()) {
      return val->get();
    }
  }
  return default_val;
}

}  // namespace

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

  if (auto* project = as_table_safe(safe_get(tbl, "project"))) {
    cfg.project_name = get_string_safe(*project, "name", "unnamed");
  } else {
    cfg.project_name = "unnamed";
  }

  if (auto* checks_arr = as_array_safe(safe_get(tbl, "checks"))) {
    for (auto& item : *checks_arr) {
      if (auto* check_tbl = item.as_table()) {
        domain::config::Check check;
        check.name = get_string_safe(*check_tbl, "name", "unnamed");
        check.command = get_string_safe(*check_tbl, "command", "");

        if (auto* args = as_array_safe(safe_get(*check_tbl, "args"))) {
          for (auto& arg : *args) {
            if (auto* val = arg.as_string()) {
              check.args.emplace_back(val->get());
            } else {
              check.args.emplace_back("");
            }
          }
        }

        if (auto* pats = as_array_safe(safe_get(*check_tbl, "patterns"))) {
          for (auto& pat : *pats) {
            if (auto* val = pat.as_string()) {
              check.patterns.emplace_back(val->get());
            } else {
              check.patterns.emplace_back("");
            }
          }
        }

        cfg.checks.emplace_back(std::move(check));
      }
    }
  }

  if (auto* exclude_tbl = as_table_safe(safe_get(tbl, "exclude"))) {
    if (auto* paths = as_array_safe(safe_get(*exclude_tbl, "paths"))) {
      for (auto& path_item : *paths) {
        if (auto* val = path_item.as_string()) {
          cfg.exclude_paths.emplace_back(val->get());
        } else {
          cfg.exclude_paths.emplace_back("");
        }
      }
    }
  }

  if (auto* output = as_table_safe(safe_get(tbl, "output"))) {
    cfg.generate_local_hook = get_bool_safe(*output, "local_hook", true);
    cfg.generate_gha = get_bool_safe(*output, "github_actions", false);
  } else {
    cfg.generate_local_hook = true;
    cfg.generate_gha = false;
  }

  if (auto* exec = as_table_safe(safe_get(tbl, "execution"))) {
    cfg.parallel = get_bool_safe(*exec, "parallel", true);
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
