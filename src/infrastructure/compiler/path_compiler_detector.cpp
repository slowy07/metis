#include "metis/infrastructure/compiler/path_compiler_detector.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "metis/domain/compiler/compiler.hpp"
#include "metis/domain/compiler/ports.hpp"

namespace metis::infrastructure::compiler {

namespace {
#ifdef _WIN32
constexpr char kPathSeparator = ';';
#else
constexpr char kPathSeparator = ':';
#endif  // _WIN32

std::vector<std::filesystem::path> split_path() {
  const char* path_env = std::getenv("PATH");

  if (path_env == nullptr) {
    return {};
  }

  std::vector<std::filesystem::path> dirs;
  std::istringstream stream{path_env};
  std::string token;
  while (std::getline(stream, token, kPathSeparator)) {
    if (!token.empty()) {
      dirs.emplace_back(token);
    }
  }

  return dirs;
}

std::optional<std::filesystem::path> find_executable(const std::vector<std::filesystem::path>& dirs,
                                                     std::string_view name) {
  for (const auto& dir : dirs) {
    auto candidate = dir / name;
#ifdef _WIN32
    auto exe = candidate;
    exe += ".exe";
    std::error_code ec;
    if (std::filesystem::is_regular_file(exe, ec)) {
      return exe;
    }
#else
    std::error_code ec;
    if (!std::filesystem::is_regular_file(candidate, ec) || ec) {
      continue;
    }
    auto perms = std::filesystem::status(candidate, ec).permissions();
    if (ec) {
      continue;
    }
    constexpr auto kExec = std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
                           std::filesystem::perms::others_exec;
    if ((perms & kExec) != std::filesystem::perms::none) {
      return candidate;
    }
#endif  // _WIN32
  }

  return std::nullopt;
}

domain::compiler::CompilerFamily family_from_cxx(std::string_view name) {
  if (name.find("clang++") != std::string_view::npos) {
    return domain::compiler::CompilerFamily::kClang;
  }
  if (name == "g++" || name.rfind("g++-", 0) == 0) {
    return domain::compiler::CompilerFamily::kGcc;
  }
  if (name == "c++") {
    return domain::compiler::CompilerFamily::kUnknown;
  }
  return domain::compiler::CompilerFamily::kUnknown;
}

std::string short_version(std::string_view raw) {
  std::string token;
  for (char c : raw) {
    if ((c >= '0' && c <= '9') || c == '.') {
      token += c;
    } else if (!token.empty()) {
      break;
    }
  }
  return token.empty() ? std::string(raw) : token;
}

std::string make_id(domain::compiler::CompilerFamily family, std::string_view version_raw) {
  std::string ver = short_version(version_raw);
  switch (family) {
    case domain::compiler::CompilerFamily::kGcc:
      return "gcc-" + ver;
    case domain::compiler::CompilerFamily::kClang:
      return "clang-" + ver;
    case domain::compiler::CompilerFamily::kAppleClang:
      return "appleclang-" + ver;
    case domain::compiler::CompilerFamily::kMsvc:
      return "msvc-" + ver;
    case domain::compiler::CompilerFamily::kMingw:
      return "mingw-" + ver;
    case domain::compiler::CompilerFamily::kUnknown:
      return "unknown-" + ver;
  }
  return "unknown-" + ver;
}

}  // namespace

PathCompilerDetector::PathCompilerDetector(const domain::compiler::CompilerProber& prober)
    : prober_(prober) {}

std::vector<domain::compiler::Compiler> PathCompilerDetector::detect() const {
  auto dirs = split_path();
  std::vector<domain::compiler::Compiler> result;
  std::set<std::filesystem::path> seen_cxx;

  const std::vector<std::pair<std::string, std::string>> pairs = {
      {"gcc", "g++"},
      {"clang", "clang++"},
      {"cc", "c++"},
  };

  for (const auto& [cc_name, cxx_name] : pairs) {
    auto cc = find_executable(dirs, cc_name);
    auto cxx = find_executable(dirs, cxx_name);
    if (!cc || !cxx) {
      continue;
    }

    std::error_code ec;
    auto canonical_cxx = std::filesystem::weakly_canonical(*cxx, ec);
    if (ec) {
      canonical_cxx = *cxx;
      ec.clear();
    }
    if (seen_cxx.contains(canonical_cxx)) {
      continue;
    }
    seen_cxx.insert(canonical_cxx);

    auto canonical_cc = std::filesystem::weakly_canonical(*cc, ec);
    if (ec) {
      canonical_cc = *cc;
    }

    auto probed = prober_.probe(canonical_cxx);
    auto family = family_from_cxx(cxx_name);
    if (family == domain::compiler::CompilerFamily::kUnknown) {
      if (probed.version.raw.find("clang") != std::string::npos) {
        family = domain::compiler::CompilerFamily::kClang;
      } else if (probed.version.raw.find("GCC") != std::string::npos ||
                 probed.version.raw.find("g++") != std::string::npos) {
        family = domain::compiler::CompilerFamily::kGcc;
      }
    }

    domain::compiler::Compiler compiler;
    compiler.id = make_id(family, probed.version.raw);
    compiler.family = family;
    compiler.version = probed.version;
    compiler.cc = canonical_cc;
    compiler.cxx = canonical_cxx;
    compiler.source = "system";
    compiler.capabilities = probed.capabilities;
    result.push_back(std::move(compiler));
  }

  return result;
}

}  // namespace metis::infrastructure::compiler
