#ifndef METIS_DOMAIN_PERFORMANCE_HPP
#define METIS_DOMAIN_PERFORMANCE_HPP

#include <algorithm>
#include <string>
#include <vector>

namespace metis::domain {

struct PerformanceMetric {
  std::string name;
  double value = 0;
  std::string unit;
  bool ok = true;
  std::string message;
};

struct PerformanceCheckResult {
  std::vector<PerformanceMetric> metrics;
  std::vector<std::string> regressions;

  [[nodiscard]] bool success() const noexcept {
    if (!regressions.empty()) {
      return false;
    }

    return std::ranges::all_of(metrics, [](const auto& metr) { return metr.ok; });
  }
};
}  // namespace metis::domain

#endif  // !METIS_DOMAIN_PERFORMANCE_HPP
