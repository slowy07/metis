#ifndef METIS_DOMAIN_COMPILER_COMPILER_HPP
#define METIS_DOMAIN_COMPILER_COMPILER_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace metis::domain::compiler {
enum class CompilerFamily : std::uint8_t {
  kGcc,
  kClang,
  kAppleClang,
  kMsvc,
  kMingw,
  kUnknown,
};

struct CompilerVersion {
  std::string raw;
};

struct TargetTriple {
  std::string triple;
};

struct CompilerCapabilities {
  bool cxx20 = false;
};

struct Compiler {
  std::string id;
  CompilerFamily family = CompilerFamily::kUnknown;
  CompilerVersion version;
  TargetTriple target;
  std::filesystem::path cc;
  std::filesystem::path cxx;
  std::optional<std::filesystem::path> linker;
  std::optional<std::filesystem::path> archiver;
  std::optional<std::filesystem::path> sysroot;
  std::string source;
  CompilerCapabilities capabilities;
};
}  // namespace metis::domain::compiler

#endif  // !METIS_DOMAIN_COMPILER_COMPILER_HPP
