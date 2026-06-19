#include "sniffercommit/application/generate_workflow_use_case.hpp"

#include <filesystem>

#include "sniffercommit/domain/workflow.hpp"

namespace sniffercommit::application {

GenerateWorkflowUseCase::GenerateWorkflowUseCase(
    std::unique_ptr<domain::ports::IFileSystem> file_system)
    : file_system_(std::move(file_system)) {}

bool GenerateWorkflowUseCase::execute(const domain::config::ProjectConfig& cfg,
                                      const std::filesystem::path& repo_root) {
  domain::workflow::WorkflowConfig wf_cfg;
  auto content = domain::workflow::generate_github_actions(cfg, wf_cfg);

  auto dir = repo_root / ".github" / "workflows";
  if (!file_system_->exists(dir)) {
    if (!file_system_->create_directories(dir)) {
      return false;
    }
  }

  auto path = dir / "sniffercommit.yml";
  return file_system_->write_file(path, content);
}

}  // namespace sniffercommit::application
