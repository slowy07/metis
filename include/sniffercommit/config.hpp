#ifndef SNIFFERCOMMIT_CONFIG_HPP
#define SNIFFERCOMMIT_CONFIG_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace sniffercommit {

struct Check {
  std::string name;
  std::string command;
  std::vector<std::string> args;
  std::vector<std::string> patterns;
};

struct Config {
  std::string project_name = "unnamed";
  std::vector<Check> checks;
  std::vector<std::string> exclude_paths;
  bool generate_local_hook = true;
  bool generate_gha = false;
};

Config load_config(const std::filesystem::path &path);

} // namespace sniffercommit

#endif // !CONFIG_HPP
