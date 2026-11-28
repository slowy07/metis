#include "metis/infrastructure/compiler/env_manager.hpp"

#include <sstream>
#include <vector>

namespace metis::infrastructure::compiler {

std::vector<domain::compiler::EnvVar> PosixEnvManager::env_for(
    const domain::compiler::Compiler& compiler) const {
  std::vector<domain::compiler::EnvVar> out;

  if (!compiler.cc.empty()) {
    out.push_back({.key = "CC", .value = compiler.cc.string()});
  }

  if (!compiler.cxx.empty()) {
    out.push_back({.key = "CXX", .value = compiler.cxx.string()});
  }

  if (compiler.archiver && !compiler.archiver->empty()) {
    out.push_back({.key = "AR", .value = compiler.archiver->string()});
  }

  out.push_back({.key = "METIS_TOOLCHAIN", .value = compiler.id});
  return out;
}

std::string shell_quote(std::string_view value) {
  std::string out = "'";
  for (char c : value) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out += c;
    }
  }
  out += '\'';
  return out;
}

std::string render_posix_env(const std::vector<domain::compiler::EnvVar>& vars) {
  std::ostringstream out;
  for (const auto& var : vars) {
    out << "export " << var.key << "=" << shell_quote(var.value) << '\n';
  }
  return out.str();
}

}  // namespace metis::infrastructure::compiler
