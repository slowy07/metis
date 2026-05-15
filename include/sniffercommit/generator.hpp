#ifndef SNIFFERCOMMIT_GENERATOR_HPP
#define SNIFFERCOMMIT_GENERATOR_HPP

#include <string>

#include "config.hpp"

namespace sniffercommit {
std::string generate_local_hook(const Config& cfg);
std::string gha_escape(const std::string& value);
bool requires_clang_format(const Config& cfg);
std::string generate_github_actions(const Config& cfg);

}  // namespace sniffercommit

#endif  // !GENERATOR_HPP
