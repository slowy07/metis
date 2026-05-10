#ifndef SNIFFERCOMMIT_GENERATOR_HPP
#define SNIFFERCOMMIT_GENERATOR_HPP

#include "config.hpp"
#include <string>

namespace sniffercommit {
std::string generate_local_hook(const Config &cfg);
std::string generate_github_actions(const Config &cfg);
} // namespace sniffercommit

#endif // !GENERATOR_HPP
