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

#ifdef _WIN32
constexpr char kPathSeparator = ';';
#else
constexpr char kPathSeparator = ':';
#endif

}  // namespace

std::optional<std::filesystem::path> ResolveToolchainUseCase::which(
    std::string_view command) const {
  if (command.empty()) {
    return std::nullopt;
  }

  std::filesystem::path cmd_path{std::string(command)};
  if (cmd_path.has_parent_path()) {
    std::error_code ec;
    if (std::filesystem::is_regular_file(cmd_path, ec) && !ec) {
      return cmd_path;
    }
    return std::nullopt;
  }

  const char* path_env = std::getenv("PATH");
  if (path_env == nullptr) {
    return std::nullopt;
  }

  std::istringstream stream{path_env};
  std::string token;
  while (std::getline(stream, token, kPathSeparator)) {
    if (token.empty()) {
      continue;
    }
    std::filesystem::path candidate = std::filesystem::path(token) / cmd_path;
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
    return candidate;
  }

  return std::nullopt;
}

}  // namespace metis::application
