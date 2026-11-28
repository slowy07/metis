#include "metis/application/resolve_toolchain_use_case.hpp"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace metis::application {
namespace {

bool is_cc_command(std::string_view cmd) {
  return cmd == "cc" || cmd == "gcc" || cmd == "clang" || cmd == "c";
}

bool is_cxx_command(std::string_view cmd) {
  return cmd == "c++" || cmd == "g++" || cmd == "clang++" || cmd == "cpp";
}

std::optional<std::filesystem::path> path_search(std::string_view command) {
  if (command.empty()) {
    return std::nullopt;
  }

  std::filesystem::path cmd_path{std::string(command)};
  if (cmd_path.has_parent_path()) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(cmd_path, ec) && !ec) {
      return std::filesystem::absolute(cmd_path, ec);
    }
    return std::nullopt;
  }

  const char* env = std::getenv("PATH");
  if (env == nullptr) {
    return std::nullopt;
  }
#ifdef _WIN32
  constexpr char sep = ';';
#else
  constexpr char sep = ':';
#endif
  std::istringstream stream{env};
  std::string token;
  while (std::getline(stream, token, sep)) {
    if (token.empty()) {
      continue;
    }
    auto candidate = std::filesystem::path(token) / cmd_path;
#ifdef _WIN32
    candidate += ".exe";
#endif
    std::error_code ec;
    if (!std::filesystem::is_regular_file(candidate, ec) || ec) {
      continue;
    }
#ifndef _WIN32
    auto perms = std::filesystem::status(candidate, ec).permissions();
    if (ec) {
      continue;
    }
    constexpr auto kExec = std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                           std::filesystem::perms::others_exec;
    if ((perms & kExec) == std::filesystem::perms::none) {
      continue;
    }
#endif
    return std::filesystem::absolute(candidate, ec);
  }
  return std::nullopt;
}

}  // namespace

ResolveToolchainUseCase::ResolveToolchainUseCase(
    const ListCompilersUseCase& list_use_case,
    const domain::compiler::ToolchainConfigRepository& project,
    const domain::compiler::ToolchainConfigRepository& global)
    : list_use_case_(list_use_case), project_(project), global_(global) {}

std::optional<std::filesystem::path> ResolveToolchainUseCase::which(
    std::string_view command) const {
  auto pinned = project_.default_toolchain();
  std::filesystem::path config_path = project_.path();

  if (!pinned) {
    pinned = global_.default_toolchain();
    config_path = global_.path();
  }

  if (pinned) {
    for (const auto& c : list_use_case_.execute(false)) {
      if (c.id != *pinned) {
        continue;
      }

      if (is_cc_command(command)) {
        return c.cc;
      }

      if (is_cxx_command(command)) {
        return c.cxx;
      }

      return path_search(command);
    }

    if (is_cc_command(command) || is_cxx_command(command)) {
      throw std::runtime_error("pinned toolchain '" + *pinned + "' not found (see " +
                               config_path.string() + "); run 'metis toolchain list' to verify");
    }
  }

  return path_search(command);
}

}  // namespace metis::application
