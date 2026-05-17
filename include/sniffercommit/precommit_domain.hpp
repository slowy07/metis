#ifndef SNIFFERCOMMIT_PRECOMMIT_DOMAIN_HPP
#define SNIFFERCOMMIT_PRECOMMIT_DOMAIN_HPP

#include <filesystem>
#include <string>

namespace sniffercommit::project {
struct ProjectConfig;
}

namespace sniffercommit::precommit {
// INFO: pre-commit hook cycle
// to generate hook generation, installation, execution

// bash hook script from project configs
[[nodiscard]] std::string generate_hook(const project::ProjectConfig& cfg);
// hook insert into .git/hooks/pre-commit
[[nodiscard]] bool install(const std::filesystem::path& repo_root, const std::string& hook_content);
// validating hook syntax without get installing
[[nodiscard]] bool validate_syntax(const std::string& hook_content);
// remove hook
bool uninstall(const std::filesystem::path& repo_root);

// INFO: check hook is installing and update
[[nodiscard]] bool is_installed(const std::filesystem::path& repo_root);

}  // namespace sniffercommit::precommit

#endif  // !SNIFFERCOMMIT_PRECOMMIT_DOMAIN_HPP
