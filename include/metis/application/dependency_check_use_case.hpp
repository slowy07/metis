#ifndef METIS_APPLICATION_DEPENDENCY_CHECK_USE_CASE_HPP
#define METIS_APPLICATION_DEPENDENCY_CHECK_USE_CASE_HPP

#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "metis/application/dependency_composite.hpp"
#include "metis/domain/dependency.hpp"
#include "metis/domain/ports/dependency_parser.hpp"
#include "metis/domain/ports/dependency_version_checker.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application {

struct DependencyCheckOptions {
  bool verbose = false;
  bool generate_graph = false;
  bool display_tree = false;
  bool check_updates = false;
  std::string graph_output_path = "dependencies.dot";
};

class DependencyCheckUseCase {
 public:
  DependencyCheckUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                         std::unique_ptr<domain::ports::IFileSystem> file_system);

  void register_parser(std::unique_ptr<domain::ports::IDependencyParser> parser);
  void register_version_checker(std::unique_ptr<domain::ports::IDependencyVersionChecker> checker);

  [[nodiscard]] domain::DependencyCheckResult execute(const std::filesystem::path& repo_root,
                                                      const DependencyCheckOptions& opts);

  static void display_tree(const std::vector<domain::Dependency>& all,
                           std::ostream& out = std::cout);

 private:
  static bool is_valid_semver(std::string_view version);
  [[nodiscard]] bool conan_dep_installed(const std::string& name) const;
  [[nodiscard]] bool vcpkg_dep_installed(const std::string& name) const;

  void check_lockfiles(const std::filesystem::path& repo_root,
                       domain::DependencyCheckResult& out) const;
  static void detect_duplicates(const std::vector<domain::Dependency>& all,
                                domain::DependencyCheckResult& out);
  static void generate_dot_graph(const std::vector<domain::Dependency>& all,
                                 const std::filesystem::path& out_path);

  std::unique_ptr<domain::ports::IShellExecutor> shell_;
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
  DependencyParserComposite parsers_;
  DependencyVersionCheckerComposite version_checkers_;
};

}  // namespace metis::application

#endif  // !METIS_APPLICATION_DEPENDENCY_CHECK_USE_CASE_HPP
