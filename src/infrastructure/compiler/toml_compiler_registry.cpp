#include "metis/infrastructure/compiler/toml_compiler_registry.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <toml++/toml.hpp>
#include <utility>
#include <vector>

#include "metis/domain/compiler/compiler.hpp"

namespace metis::infrastructure::compiler {
namespace {

std::string family_to_string(domain::compiler::CompilerFamily family) {
  switch (family) {
    case domain::compiler::CompilerFamily::kGcc:
      return "gcc";
    case domain::compiler::CompilerFamily::kClang:
      return "clang";
    case domain::compiler::CompilerFamily::kAppleClang:
      return "appleclang";
    case domain::compiler::CompilerFamily::kMsvc:
      return "msvc";
    case domain::compiler::CompilerFamily::kMingw:
      return "mingw";
    case domain::compiler::CompilerFamily::kUnknown:
      return "unknown";
  }
  return "unknown";
}

domain::compiler::CompilerFamily family_from_string(std::string_view value) {
  if (value == "gcc") return domain::compiler::CompilerFamily::kGcc;
  if (value == "clang") return domain::compiler::CompilerFamily::kClang;
  if (value == "appleclang") return domain::compiler::CompilerFamily::kAppleClang;
  if (value == "msvc") return domain::compiler::CompilerFamily::kMsvc;
  if (value == "mingw") return domain::compiler::CompilerFamily::kMingw;
  return domain::compiler::CompilerFamily::kUnknown;
}

}  // namespace

TomlCompilerRegistry::TomlCompilerRegistry(std::filesystem::path registry_path)
    : registry_path_(std::move(registry_path)) {}

std::vector<domain::compiler::Compiler> TomlCompilerRegistry::load() const {
  std::error_code ec;
  if (!std::filesystem::exists(registry_path_, ec) || ec) {
    return {};
  }

  toml::table tbl;
  try {
    tbl = toml::parse_file(registry_path_.string());
  } catch (const toml::parse_error& err) {
    throw std::runtime_error(std::string("Failed to parse compiler registry: ") + err.what());
  }

  std::vector<domain::compiler::Compiler> compilers;
  auto* arr = tbl["compilers"].as_array();
  if (arr == nullptr) {
    return compilers;
  }

  for (const auto& node : *arr) {
    const auto* t = node.as_table();
    if (t == nullptr) {
      continue;
    }
    domain::compiler::Compiler c;
    c.id = (*t)["id"].value_or(std::string{});
    c.family = family_from_string((*t)["family"].value_or(std::string{"unknown"}));
    c.version.raw = (*t)["version"].value_or(std::string{});
    c.cc = (*t)["cc"].value_or(std::string{});
    c.cxx = (*t)["cxx"].value_or(std::string{});
    c.source = (*t)["source"].value_or(std::string{"registry"});
    c.capabilities.cxx20 = (*t)["cxx20"].value_or(false);
    if (!c.id.empty()) {
      compilers.push_back(std::move(c));
    }
  }

  return compilers;
}

void TomlCompilerRegistry::write(const std::vector<domain::compiler::Compiler>& compilers) const {
  std::error_code ec;
  auto parent = registry_path_.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      throw std::runtime_error("Failed to create compiler registry directory: " + parent.string());
    }
  }

  toml::array arr;
  for (const auto& c : compilers) {
    toml::table t;
    t.insert("id", c.id);
    t.insert("family", family_to_string(c.family));
    t.insert("version", c.version.raw);
    t.insert("cc", c.cc.string());
    t.insert("cxx", c.cxx.string());
    t.insert("source", c.source);
    t.insert("cxx20", c.capabilities.cxx20);
    arr.push_back(std::move(t));
  }

  toml::table root;
  root.insert("compilers", std::move(arr));

  auto tmp = registry_path_;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc | std::ios::binary);
    if (!out) {
      std::filesystem::remove(tmp, ec);
      throw std::runtime_error("Failed to write compiler registry: " + registry_path_.string());
    }
    out << root;
    out.flush();
    if (!out.good()) {
      out.close();
      std::filesystem::remove(tmp, ec);
      throw std::runtime_error("Failed to write compiler registry: " + registry_path_.string());
    }
  }

  std::filesystem::rename(tmp, registry_path_, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    throw std::runtime_error("Failed to persist compiler registry: " + registry_path_.string());
  }
}

std::optional<domain::compiler::Compiler> TomlCompilerRegistry::find(std::string_view id) const {
  auto compilers = load();
  for (auto& c : compilers) {
    if (c.id == id) {
      return c;
    }
  }
  return std::nullopt;
}

std::vector<domain::compiler::Compiler> TomlCompilerRegistry::list() const { return load(); }

void TomlCompilerRegistry::save(const domain::compiler::Compiler& compiler) {
  auto compilers = load();
  auto it = std::find_if(compilers.begin(), compilers.end(),
                         [&](const auto& c) { return c.id == compiler.id; });
  if (it != compilers.end()) {
    *it = compiler;
  } else {
    compilers.push_back(compiler);
  }
  write(compilers);
}

void TomlCompilerRegistry::remove(std::string_view id) {
  auto compilers = load();
  auto old_size = compilers.size();
  compilers.erase(
      std::remove_if(compilers.begin(), compilers.end(), [&](const auto& c) { return c.id == id; }),
      compilers.end());
  if (compilers.size() != old_size) {
    write(compilers);
  }
}

}  // namespace metis::infrastructure::compiler
