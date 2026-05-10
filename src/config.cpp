#include "../include/sniffercommit/config.hpp"
#include <optional>
#include <stdexcept>
#include <toml++/toml.hpp>

#include <string>

namespace sniffercommit {

Config load_config(const std::filesystem::path &path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("Config file not found: " + path.string());
  }

  toml::table tbl;
  try {
    tbl = toml::parse_file(path.string());
  } catch (const toml::parse_error &e) {
    throw std::runtime_error("TOML parse error: " +
                             std::string(e.description()));
  }

  Config cfg;

  // [project]
  cfg.project_name =
      tbl["project"]["name"].value<std::string>().value_or("unnamed");

  // [[checks]]
  if (auto *checks_arr = tbl["checks"].as_array()) {
    for (auto &item : *checks_arr) {
      if (auto *check_tbl = item.as_table()) {
        Check c;
        // Use operator[] + .value<T>().value_or() for safe extraction
        c.name = (*check_tbl)["name"].value<std::string>().value_or("unnamed");
        c.command = (*check_tbl)["command"].value<std::string>().value_or("");

        if (c.command.empty()) {
          throw std::runtime_error("Check '" + c.name +
                                   "' missing required 'command'");
        }

        if (auto *args = (*check_tbl)["args"].as_array()) {
          for (auto &a : *args) {
            c.args.push_back(a.value<std::string>().value_or(""));
          }
        }
        if (auto *pats = (*check_tbl)["patterns"].as_array()) {
          for (auto &p : *pats) {
            c.patterns.push_back(p.value<std::string>().value_or(""));
          }
        }
        cfg.checks.push_back(std::move(c));
      }
    }
  }

  // [exclude]
  if (auto *paths = tbl["exclude"]["paths"].as_array()) {
    for (auto &p : *paths) {
      cfg.exclude_paths.push_back(p.value<std::string>().value_or(""));
    }
  }

  // [output]
  cfg.generate_local_hook =
      tbl["output"]["local_hook"].value<bool>().value_or(true);
  cfg.generate_gha =
      tbl["output"]["github_actions"].value<bool>().value_or(false);

  if (cfg.checks.empty()) {
    throw std::runtime_error(
        "Config must contain at least one [[checks]] entry");
  }

  return cfg;
}

} // namespace sniffercommit
