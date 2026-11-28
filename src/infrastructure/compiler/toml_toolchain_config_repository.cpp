#include "metis/infrastructure/compiler/toml_toolchain_config_repository.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <toml++/toml.hpp>
#include <utility>

namespace metis::infrastructure::compiler {

TomlToolchainConfigRepository::TomlToolchainConfigRepository(std::filesystem::path path)
    : path_(std::move(path)) {}

std::filesystem::path TomlToolchainConfigRepository::path() const { return path_; }

std::optional<std::string> TomlToolchainConfigRepository::default_toolchain() const {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path_, ec) || ec) {
    return std::nullopt;
  }

  toml::table tbl;
  try {
    tbl = toml::parse_file(path_.string());
  } catch (const toml::parse_error& e) {
    throw std::runtime_error("failed to parse " + path_.string() + ": " +
                             std::string(e.description()));
  }

  auto* project = tbl["project"].as_table();
  if (project == nullptr) {
    return std::nullopt;
  }

  auto def = (*project)["default"].value<std::string>();
  if (!def || def->empty()) {
    return std::nullopt;
  }

  return def;
}

void TomlToolchainConfigRepository::set_default(std::string_view id) {
  toml::table tbl;
  std::error_code ec;

  if (std::filesystem::is_regular_file(path_, ec) && !ec) {
    try {
      tbl = toml::parse_file(path_.string());
    } catch (const toml::parse_error& e) {
      throw std::runtime_error("failed to parse " + path_.string() + ": " +
                               std::string(e.description()));
    }
  }

  if (auto* project = tbl["project"].as_table(); project != nullptr) {
    project->insert_or_assign("default", std::string(id));
  } else {
    toml::table new_project;
    new_project.insert("default", std::string(id));
    tbl.insert_or_assign("project", std::move(new_project));
  }

  auto parent = path_.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      throw std::runtime_error("failed to create directory " + parent.string() + ": " +
                               ec.message());
    }
  }

  auto tmp = path_;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc | std::ios::binary);
    if (!out) {
      std::filesystem::remove(tmp, ec);
      throw std::runtime_error("failed to open " + tmp.string() + " for writing");
    }
    out << tbl << "\n";
    out.flush();
    if (!out.good()) {
      out.close();
      std::filesystem::remove(tmp, ec);
      throw std::runtime_error("failed to write " + tmp.string());
    }
  }

  std::filesystem::rename(tmp, path_, ec);
  if (ec) {
    std::error_code remove_ec;
    std::filesystem::remove(path_, remove_ec);
    std::error_code rename_ec;
    std::filesystem::rename(tmp, path_, rename_ec);
    if (rename_ec) {
      std::filesystem::remove(tmp, remove_ec);
      throw std::runtime_error("failed to persist " + path_.string() + ": " + rename_ec.message());
    }
  }
}

std::filesystem::path find_project_config_path(std::filesystem::path start) {
  auto current = std::filesystem::absolute(std::move(start)).lexically_normal();
  while (true) {
    std::error_code ec;
    if (std::filesystem::is_directory(current / ".git", ec)) {
      return current / ".metis" / "toolchains.toml";
    }
    if (current == current.root_path()) {
      break;
    }
    current = current.parent_path();
  }
  return std::filesystem::absolute(start).lexically_normal() / ".metis" / "toolchains.toml";
}

std::filesystem::path find_global_config_path() {
  if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
    return std::filesystem::path(xdg) / "metis" / "toolchains.toml";
  }
  if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
    return std::filesystem::path(home) / ".config" / "metis" / "toolchains.toml";
  }
  if (const char* appdata = std::getenv("APPDATA"); appdata != nullptr && *appdata != '\0') {
    return std::filesystem::path(appdata) / "metis" / "toolchains.toml";
  }
  throw std::runtime_error(
      "cannot locate user config directory (set HOME, XDG_CONFIG_HOME, or APPDATA)");
}

}  // namespace metis::infrastructure::compiler
