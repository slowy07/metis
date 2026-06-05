#include "sniffercommit/util.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace sniffercommit::util {

std::string exec_cmd(const std::string& cmd) {
  std::string result;
  result.reserve(4096);

#ifdef _WIN32
  PipePtr pipe(_popen(cmd.c_str(), "r"));
#else
  PipePtr pipe(popen(cmd.c_str(), "r"));
#endif

  if (!pipe) {
    throw std::runtime_error("popen() failed " + cmd);
  }

  std::array<char, 4096> buffer{};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
    result += buffer.data();
  }

  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }

  return result;
}

bool command_exists(const std::string& cmd) {
  std::string test = "command -v " + shell_escape(cmd) + " >/dev/null 2>&1";
  return std::system(test.c_str()) == 0;
}

std::string shell_escape(const std::string& value) {
  std::string escaped = "'";

  for (char chr : value) {
    if (chr == '\'') {
      escaped += "'\\''";
    } else {
      escaped += chr;
    }
  }

  escaped += "'";
  return escaped;
}

CwdGuard::CwdGuard(const std::filesystem::path& target)
    : original_cwd_(std::filesystem::current_path()) {
  std::filesystem::current_path(target);
}

CwdGuard::~CwdGuard() {
  try {
    std::filesystem::current_path(original_cwd_);
  } catch (std::exception& error) {
    std::cerr << error.what() << "\n";
  }
}

bool matches_pattern(const std::string& file, const std::vector<std::string>& patterns) {
  if (patterns.empty()) {
    return true;
  }

  return std::ranges::any_of(patterns, [&file](const auto& pattern) {
    if (pattern.empty()) {
      return true;
    }

    if (pattern.starts_with("*.") && file.ends_with(pattern.substr(1))) {
      return true;
    }

    if (pattern.ends_with("/**") && file.starts_with(pattern.substr(0, pattern.size() - 3) + "/")) {
      return true;
    }

    if (pattern.starts_with("**/")) {
      std::string suffix = pattern.substr(3);
      if (file.ends_with(suffix)) {
        return true;
      }
    }

    if (file == pattern || file.starts_with(pattern)) {
      return true;
    }

    return false;
  });
}

bool is_excluded(const std::string& file, const std::vector<std::string>& excludes) {
  for (const auto& excl : excludes) {
    if (file == excl) {
      return true;
    }

    if (excl.starts_with("*.") && file.ends_with(excl.substr(1))) {
      return true;
    }

    std::string norm_e = excl;

    if (!norm_e.empty() && norm_e.back() != '/') {
      norm_e += '/';
    }

    if (file.starts_with(norm_e)) {
      return true;
    }
  }

  return false;
}

}  // namespace sniffercommit::util
