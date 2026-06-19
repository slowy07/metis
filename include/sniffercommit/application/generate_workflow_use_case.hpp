#ifndef SNIFFERCOMMIT_APPLICATION_GENERATE_WORKFLOW_USE_CASE_HPP
#define SNIFFERCOMMIT_APPLICATION_GENERATE_WORKFLOW_USE_CASE_HPP

#include <filesystem>
#include <memory>

#include "sniffercommit/domain/config.hpp"
#include "sniffercommit/domain/ports/file_system.hpp"

namespace sniffercommit::application {

class GenerateWorkflowUseCase {
 public:
  explicit GenerateWorkflowUseCase(std::unique_ptr<domain::ports::IFileSystem> file_system);

  [[nodiscard]] bool execute(const domain::config::ProjectConfig& cfg,
                             const std::filesystem::path& repo_root);

 private:
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
};

}  // namespace sniffercommit::application

#endif
