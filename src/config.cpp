#include "../include/sniffercommit/config.hpp"

#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>

namespace sniffercommit {

Config load_config(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("Config file not found: " + path.string());
  }

  toml::table tbl;
  try {
    tbl = toml::parse_file(path.string());
  } catch (const toml::parse_error& e) {
    throw std::runtime_error("TOML parse error: " + std::string(e.description()));
  }

  Config cfg;

  // [project]
  if (auto project = tbl["project"].as_table()) {
    cfg.project_name = (*project)["name"].value<std::string>().value_or("unnamed");
  }

  // [[checks]]
  if (auto* checks_arr = tbl["checks"].as_array()) {
    for (auto& item : *checks_arr) {
      if (auto* check_tbl = item.as_table()) {
        Check c;
        // Use operator[] + .value<T>().value_or() for safe extraction
        c.name = (*check_tbl)["name"].value<std::string>().value_or("unnamed");
        c.command = (*check_tbl)["command"].value<std::string>().value_or("");

        if (c.command.empty()) {
          throw std::runtime_error("Check '" + c.name + "' missing required 'command'");
        }

        if (auto* args = (*check_tbl)["args"].as_array()) {
          for (auto& a : *args) {
            c.args.push_back(a.value<std::string>().value_or(""));
          }
        }
        if (auto* pats = (*check_tbl)["patterns"].as_array()) {
          for (auto& p : *pats) {
            c.patterns.push_back(p.value<std::string>().value_or(""));
          }
        }
        cfg.checks.push_back(std::move(c));
      }
    }
  }

  // [exclude]
  if (auto* paths = tbl["exclude"]["paths"].as_array()) {
    for (auto& p : *paths) {
      cfg.exclude_paths.push_back(p.value<std::string>().value_or(""));
    }
  }

  // [output]
  if (auto* output = tbl["output"].as_table()) {
    cfg.generate_local_hook = (*output)["local_hook"].value<bool>().value_or(true);
    cfg.generate_gha = (*output)["github_actions"].value<bool>().value_or(false);
  }

  // [execution]
  if (auto* exec = tbl["execution"].as_table()) {
    cfg.parallel = (*exec)["parallel"].value<bool>().value_or(true);
  } else {
    cfg.parallel = true;
  }

  if (cfg.checks.empty()) {
    throw std::runtime_error("Config must contain at least one [[checks]] entry");
  }

#ifdef SNIFFERCOMMIT_DEBUG
  std::cerr << "[DEBUG]: config loaded: \n";
  std::cerr << " project: " << cfg.project_name << "\n";
  std::cerr << " checks: " << cfg.checks.size() << "\n";
  std::cerr << " parallel: " << (cfg.parallel ? "true" : "false") << "\n";
#endif  // SNIFFERCOMMIT_DEBUG

  return cfg;
}

}  // namespace sniffercommit
