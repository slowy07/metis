#include "sniffercommit/template.hpp"
#include <string>

namespace sniffercommit {
std::string default_sniffercommit_config() {
  return R"([project]
name = "my-project"

[[checks]]
name = "clang-format"
command = "clang-format"
args = ["-i", "--fallback-style=Google", "-style=file"]
patterns = ["*.cpp", "*.hpp", "*.h", "*.cc"]

[[checks]]
name = "trailing-whitespace"
command = "grep"
args = ["-E", "--text", "[[:space:]]+$"]
patterns = ["*"]

[exclude]
paths = ["build/", "third_party/", ".git/"]

[output]
local_hook = true
github_actions = false

[execution]
parallel = true
)";
}

std::string default_clang_format() {
  return R"(---
BasedOnStyle: Google

IndentWidth: 2
ColumnLimit: 100
PointerAlignment: Left
SortIncludes: true

AllowShortFunctionsOnASingleLine: Empty
BreakBeforeBraces: Attach

Standard: Latest
...
)";
}

} // namespace sniffercommit
