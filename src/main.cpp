#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "metis/application/emit_env_use_case.hpp"
#include "metis/application/list_compilers_use_case.hpp"
#include "metis/application/resolve_toolchain_use_case.hpp"
#include "metis/application/switch_toolchain_use_case.hpp"
#include "metis/argparse.hpp"
#include "metis/domain/compiler/compiler.hpp"
#include "metis/infrastructure/compiler/env_manager.hpp"
#include "metis/infrastructure/compiler/path_compiler_detector.hpp"
#include "metis/infrastructure/compiler/process_compiler_prober.hpp"
#include "metis/infrastructure/compiler/toml_compiler_registry.hpp"
#include "metis/infrastructure/compiler/toml_toolchain_config_repository.hpp"
#include "metis/presentation/console.hpp"

namespace {

std::filesystem::path registry_path() {
  if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg != nullptr && *xdg != '\0') {
    return std::filesystem::path(xdg) / "metis" / "registry.toml";
  }
  if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
    return std::filesystem::path(home) / ".local" / "share" / "metis" / "registry.toml";
  }
  return std::filesystem::temp_directory_path() / "metis-registry.toml";
}

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

int run_toolchain(int argc, char** argv) {
  using namespace metis;

  if (argc < 3) {
    std::cerr << "usage: metis toolchain {list|which|use|env} ...\n";
    return 1;
  }

  try {
    infrastructure::compiler::ProcessCompilerProber prober;
    infrastructure::compiler::PathCompilerDetector detector(prober);
    infrastructure::compiler::TomlCompilerRegistry registry(registry_path());
    application::ListCompilersUseCase list(detector, registry);

    std::string_view sub = argv[2];
    if (sub == "list") {
      bool installed_only = false;
      for (int i = 3; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--installed") {
          installed_only = true;
        }
      }
      for (const auto& c : list.execute(installed_only)) {
        std::cout << c.id << "\t" << c.source << "\t" << c.cxx.string() << "\n";
      }
      return 0;
    }

    infrastructure::compiler::TomlToolchainConfigRepository project_config(
        infrastructure::compiler::find_project_config_path(std::filesystem::current_path()));
    infrastructure::compiler::TomlToolchainConfigRepository global_config(
        infrastructure::compiler::find_global_config_path());

    if (sub == "which") {
      if (argc < 4) {
        std::cerr << "usage: metis toolchain which <command>\n";
        return 1;
      }
      application::ResolveToolchainUseCase use_case(list, project_config, global_config);
      auto resolved = use_case.which(argv[3]);
      if (!resolved) {
        std::cerr << "command not found: " << argv[3] << "\n";
        return 1;
      }
      std::cout << resolved->string() << "\n";
      return 0;
    }

    if (sub == "use") {
      if (argc < 4) {
        std::cerr << "usage: metis toolchain use <id> [--global]\n";
        return 1;
      }
      bool global = false;
      for (int i = 4; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--global") {
          global = true;
        }
      }
      auto& target = global ? global_config : project_config;
      application::SwitchToolchainUseCase use_case(list);
      use_case.execute(argv[3], target);
      std::cout << "default toolchain set to '" << argv[3] << "' in " << target.path().string()
                << "\n";
      return 0;
    }

    if (sub == "env") {
      std::optional<std::string_view> id;
      if (argc >= 4) {
        id = argv[3];
      }
      infrastructure::compiler::PosixEnvManager env_manager;
      application::EmitEnvUseCase use_case(list, project_config, global_config, env_manager);
      std::cout << infrastructure::compiler::render_posix_env(use_case.execute(id));
      return 0;
    }

    std::cerr << "unknown toolchain subcommand: " << sub << "\n";
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
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

  infrastructure::compiler::ProcessCompilerProber prober;
  infrastructure::compiler::PathCompilerDetector detector(prober);
  infrastructure::compiler::TomlCompilerRegistry registry(registry_path());
  application::ListCompilersUseCase list(detector, registry);

  infrastructure::compiler::TomlToolchainConfigRepository project_config(
      infrastructure::compiler::find_project_config_path(std::filesystem::current_path()));
  infrastructure::compiler::TomlToolchainConfigRepository global_config(
      infrastructure::compiler::find_global_config_path());

  if (action.empty() || action == "list") {
    auto compilers = list.execute(installed_only);
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
    application::ResolveToolchainUseCase resolver(list, project_config, global_config);
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
  app.set_version("0.5.0")
      .add_subcommand("compiler", "Detect, list, and resolve compilers")
      .add_subcommand("toolchain", "Manage and resolve toolchains");

  app.set_subcommand_help("toolchain",
                          "Usage:\n"
                          "  metis toolchain list [--installed]\n"
                          "  metis toolchain which <command>\n"
                          "  metis toolchain use <id> [--global]\n"
                          "  metis toolchain env [id]\n");

  if (!app.parse(argc, argv)) {
    return 0;
  }

  std::string_view subcmd = app.get_subcommand();
  if (subcmd == "compiler") {
    return run_compiler_command(argc, argv);
  }
  if (subcmd == "toolchain") {
    return run_toolchain(argc, argv);
  }

  app.show_help();
  return 1;
}
