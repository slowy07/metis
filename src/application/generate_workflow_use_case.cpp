#include "metis/application/generate_workflow_use_case.hpp"

#include <filesystem>

#include "metis/domain/workflow.hpp"

namespace metis::application {

GenerateWorkflowUseCase::GenerateWorkflowUseCase(
    std::unique_ptr<domain::ports::IFileSystem> file_system)
    : file_system_(std::move(file_system)) {}

bool GenerateWorkflowUseCase::execute(const domain::config::ProjectConfig& cfg,
                                      const std::filesystem::path& repo_root,
                                      domain::workflow::Platform platform) {
  domain::workflow::WorkflowConfig wf_cfg;
  wf_cfg.platform = platform;
  auto content = domain::workflow::generate_workflow(cfg, wf_cfg);

  std::filesystem::path dir;
  std::string filename;
  if (platform == domain::workflow::Platform::GitLabCI) {
    dir = repo_root;
    filename = ".gitlab-ci.yml";
  } else {
    dir = repo_root / ".github" / "workflows";
    filename = "metis.yml";
  }

  if (dir != repo_root && !file_system_->exists(dir)) {
    if (!file_system_->create_directories(dir)) {
      return false;
    }
  }

  return file_system_->write_file(dir / filename, content);
}

}  // namespace metis::application
