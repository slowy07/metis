#include "metis/application/performance_check_use_case.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace metis::application {
namespace {
std::string format_bytes(double bytes) {
  const char* units[] = {"B", "KB", "MB", "GB"};
  int unit_idx = 0;
  double size = bytes;

  while (size >= 1024.0 && unit_idx < 3) {
    size /= 1024.0;
    ++unit_idx;
  }

  return fmt::format("{:.2f} {}", size, units[unit_idx]);
}

bool parse_baseline_line(std::string_view line, std::string& out_key, double& out_val) {
  auto eq = line.find('=');

  if (eq == std::string_view::npos) {
    return false;
  }

  std::string key = std::string(trim(std::string(line.substr(0, eq))));
  std::string val_str = std::string(trim(std::string(line.substr(eq + 1))));

  try {
    out_val = std::stod(val_str);
    out_key = std::move(key);
    return true;
  } catch (...) {
    return false;
  }
}

std::string trim(std::string_view s) {
  size_t a = 0;
  while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) {
    ++a;
  }
  size_t b = s.size();
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) {
    --b;
  }
  return std::string(s.substr(a, b - a));
}

}  // namespace

void PerformanceCheckUseCase::detect_regressions(
    const std::vector<domain::PerformanceMetric>& current,
    const std::vector<domain::PerformanceMetric>& baseline, double threshold_pct,
    domain::PerformanceCheckResult& out) const {
  for (const auto& cur : current) {
    auto it =
        std::ranges::find_if(baseline, [&cur](const auto& base) { return base.name == cur.name; });

    if (it == baseline.end()) {
      continue;
    }

    const auto& base = *it;
    if (base.value == 0.0) {
      continue;
    }

    double change_pct = ((cur.value - base.value) / base.value) * 100.0;

    if (change_pct > threshold_pct) {
      out.regressions.push_back(
          fmt::format("{} regressed by *{:.1f}% baseline {:.3f} {} -> current {:.3f} {}", cur.name,
                      change_pct, base.value, base.unit, cur.value, cur.unit));

      for (auto& met : out.metrics) {
        if (met.name == cur.name) {
          met.ok = false;
          met.message += fmt::format("  (+{:.1f}%)", change_pct);
          break;
        }
      }
    } else {
      for (auto& met : out.metrics) {
        if (met.name == cur.name) {
          if (std::abs(change_pct) > 0.5) {
            met.message += fmt::format("  ({:+.1f}%)", change_pct);
          } else {
            met.message += "  (±0%)";
          }

          break;
        }
      }
    }
  }
}

}  // namespace metis::application
