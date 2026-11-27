#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "metis/application/list_compilers_use_case.hpp"
#include "metis/application/resolve_toolchain_use_case.hpp"
#include "metis/argparse.hpp"
#include "metis/domain/compiler/compiler.hpp"
#include "metis/infrastructure/compiler/path_compiler_detector.hpp"
#include "metis/infrastructure/compiler/process_compiler_prober.hpp"
#include "metis/infrastructure/compiler/toml_compiler_registry.hpp"
#include "metis/presentation/console.hpp"

namespace {

std::string_view family_name(metis::domain::compiler::CompilerFamily family) {
  switch (family) {
    case metis::domain::compiler::CompilerFamily::kGcc:
      return "gcc";
    case metis::domain::compiler::CompilerFamily::kClang:
      return "clang";
    case metis::domain::compiler::CompilerFamily::kAppleClang:
      return "appleclang";
    case metis::domain::compiler::CompilerFamily::kMsvc:
      return "msvc";
    case metis::domain::compiler::CompilerFamily::kMingw:
      return "mingw";
    case metis::domain::compiler::CompilerFamily::kUnknown:
      return "unknown";
  }
  return "unknown";
}

std::filesystem::path default_registry_path() {
  const char* home = std::getenv("HOME");
#ifdef _WIN32
  if (home == nullptr) {
    home = std::getenv("USERPROFILE");
  }
#endif
  if (home == nullptr) {
    return std::filesystem::current_path() / ".metis" / "compilers.toml";
  }
  return std::filesystem::path(home) / ".metis" / "compilers.toml";
}

int run_compiler_command(int argc, char** argv) {
  using namespace metis;
  std::string action;
  std::string name;
  bool installed_only = false;

  for (int i = 2; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "list") {
      action = "list";
    } else if (arg == "which") {
      action = "which";
      if (i + 1 < argc && std::string_view(argv[i + 1]).rfind('-', 0) != 0) {
        name = argv[++i];
      }
    } else if (arg == "--installed" || arg == "--installed-only") {
      installed_only = true;
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage:\n"
                   "  metis compiler list [--installed]\n"
                   "  metis compiler which <name>\n";
      return 0;
    } else if (!arg.empty() && arg[0] != '-' && action.empty()) {
      action = std::string(arg);
    } else if (!arg.empty() && arg[0] != '-' && action == "which" && name.empty()) {
      name = std::string(arg);
    }
  }

  if (action.empty() || action == "list") {
    infrastructure::compiler::ProcessCompilerProber prober;
    infrastructure::compiler::PathCompilerDetector detector(prober);
    infrastructure::compiler::TomlCompilerRegistry registry(default_registry_path());
    application::ListCompilersUseCase use_case(detector, registry);
    auto compilers = use_case.execute(installed_only);

    if (compilers.empty()) {
      presentation::Console::print_info_block("No compilers found");
      return 0;
    }

    for (const auto& c : compilers) {
      std::cout << c.id << "\t" << family_name(c.family) << "\t" << c.version.raw << "\t"
                << c.cxx.string() << "\t" << c.source;
      if (c.capabilities.cxx20) {
        std::cout << "\tc++20";
      }
      std::cout << "\n";
    }
    return 0;
  }

  if (action == "which") {
    if (name.empty()) {
      std::cerr << "[ERROR] compiler which requires a command name\n";
      return 1;
    }
    application::ResolveToolchainUseCase resolver;
    auto path = resolver.which(name);
    if (!path) {
      std::cerr << "[ERROR] compiler not found: " << name << "\n";
      return 1;
    }
    std::cout << path->string() << "\n";
    return 0;
  }

  std::cerr << "[ERROR] Unknown compiler action: " << action << "\n";
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  metis::ArgParser app("metis", "Fast C++20-powered pre-commit & CI generator");
  app.set_version("0.5.0").add_subcommand("compiler", "Detect, list, and resolve compilers");

  if (!app.parse(argc, argv)) {
    return 0;
  }

  std::string_view subcmd = app.get_subcommand();
  if (subcmd == "compiler") {
    return run_compiler_command(argc, argv);
  }

  app.show_help();
  return 1;
}
