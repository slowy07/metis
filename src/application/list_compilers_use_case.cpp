#include "metis/application/list_compilers_use_case.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>
#include <vector>

#include "metis/domain/compiler/compiler.hpp"

namespace metis::application {

ListCompilersUseCase::ListCompilersUseCase(const domain::compiler::CompilerDetector& detector,
                                           const domain::compiler::CompilerRegistry& registry)
    : detector_(detector), registry_(registry) {}

std::vector<domain::compiler::Compiler> ListCompilersUseCase::execute(bool installed_only) const {
  auto registered = registry_.list();
  if (installed_only) {
    return registered;
  }

  auto detected = detector_.detect();
  std::unordered_set<std::string> ids;
  ids.reserve(registered.size() + detected.size());

  std::vector<domain::compiler::Compiler> out;
  out.reserve(registered.size() + detected.size());

  for (auto& c : registered) {
    ids.insert(c.id);
    out.push_back(std::move(c));
  }
  for (auto& c : detected) {
    if (ids.insert(c.id).second) {
      out.push_back(std::move(c));
    }
  }

  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
  return out;
}

}  // namespace metis::application
