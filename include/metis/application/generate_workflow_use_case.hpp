#ifndef METIS_APPLICATION_GENERATE_WORKFLOW_USE_CASE_HPP
#define METIS_APPLICATION_GENERATE_WORKFLOW_USE_CASE_HPP

#include <filesystem>
#include <memory>

#include "metis/domain/config.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/workflow.hpp"

namespace metis::application {

/**
 * @brief Application use case responsible for generate CI workflow files.
 *
 * use case orchestrate workflow generate based on the provided project
 * configuration and target repository. it depending on the filesystem abstraction
 * instead of concrete implementation. following the depedency inversion principle
 */
class GenerateWorkflowUseCase {
 public:
  /**
   * @brief construct use case with filesystem depedency
   *
   * @param file_system Filesystem abstraction used to create directories and write workflow files
   */
  explicit GenerateWorkflowUseCase(std::unique_ptr<domain::ports::IFileSystem> file_system);

  /**
   * @brief generate workflow files for the specified repository
   *
   * @param cfg Project config used to generate workflow
   * @param repo_root Path to the repository root
   * @param platform Traget CI platform. default GithubAction
   *
   * @return true if the workflow was generate success
   * @return false if the workflow generate failed
   */
  [[nodiscard]] bool execute(
      const domain::config::ProjectConfig& cfg, const std::filesystem::path& repo_root,
      domain::workflow::Platform platform = domain::workflow::Platform::GithubAction);

 private:
  /// filesystem abstraction used for all file operations
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
};

}  // namespace metis::application

#endif
