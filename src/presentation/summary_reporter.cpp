#include "metis/presentation/summary_reporter.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "metis/presentation/console.hpp"

namespace metis::presentation {

void SummaryReporter::print_test_summary(const TestSummary& summary) {
  std::vector<std::string> box_lines;

  if (summary.success) {
    box_lines.push_back(Console::green("Tests passed") + "  " +
                        Console::dim(fmt::format("({} tests)", summary.total_tests)));
  } else {
    box_lines.push_back(
        Console::red("Tests failed") + "  " +
        Console::dim(fmt::format("({} of {} failed)", summary.failed_tests, summary.total_tests)));
  }

  if (summary.coverage_enabled) {
    auto cov_color = summary.coverage_ok ? &Console::green : &Console::red;

    if (summary.line_coverage >= 0.0) {
      box_lines.push_back(Console::dim("Line coverage:    ") +
                          cov_color(fmt::format("{:.1f}%", summary.line_coverage)));
    }

    if (summary.branch_coverage >= 0.0) {
      box_lines.push_back(Console::dim("Branch coverage:  ") +
                          cov_color(fmt::format("{:.1f}%", summary.branch_coverage)));
    }
    if (summary.function_coverage >= 0.0) {
      box_lines.push_back(Console::dim("Func coverage:    ") +
                          cov_color(fmt::format("{:.1f}%", summary.function_coverage)));
    }
  }

  Console::print_summary_box(box_lines);

  if (summary.success) {
    Console::print_next_steps({
        "Run " + Console::bold("metis test --verbose") + " to see failure details",
        "Fix failing tests and re-run",
    });
  }
}

void SummaryReporter::print_dep_summary(const DepSummary& summary) {
  std::vector<std::string> box_lines;

  if (summary.invalid == 0 && summary.duplicates == 0 && summary.lockfile_issues == 0 &&
      summary.outdated == 0) {
    box_lines.push_back(Console::green("Dependencies OK") + "  " +
                        Console::dim(fmt::format("({} total)", summary.total)));
  } else {
    box_lines.push_back(Console::red("Dependency issues found"));
    if (summary.invalid > 0) {
      box_lines.push_back("  " + Console::red("✖") + " " +
                          Console::dim(fmt::format("{} invalid", summary.invalid)));
    }
    if (summary.duplicates > 0) {
      box_lines.push_back("  " + Console::yellow("⚠") + " " +
                          Console::dim(fmt::format("{} duplicates", summary.duplicates)));
    }
    if (summary.lockfile_issues > 0) {
      box_lines.push_back("  " + Console::yellow("⚠") + " " +
                          Console::dim(fmt::format("{} lockfile issues", summary.lockfile_issues)));
    }
    if (summary.outdated > 0) {
      box_lines.push_back("  " + Console::cyan("⬆") + " " +
                          Console::dim(fmt::format("{} updates available", summary.outdated)));
    }
  }

  Console::print_summary_box(box_lines);
}

void SummaryReporter::print_dep_updates(const DepUpdateSummary& summary) {
  if (summary.outdated.empty()) {
    Console::print_success_block("All dependencies are up to date");
    return;
  }

  Console::print_header("Available Updates");

  size_t name_width = 0;
  for (const auto& dep : summary.outdated) {
    name_width = std::max(name_width, dep.name.size());
  }

  for (const auto& dep : summary.outdated) {
    std::cout << "  " << Console::cyan("⬆") << " " << dep.name
              << std::string(name_width - dep.name.size(), ' ') << "  " << Console::dim(dep.version)
              << " → " << Console::green(dep.latest_version.value()) << "  "
              << Console::dim("(" + dep.source + ")") << "\n";
  }

  std::cout << "\n";
  Console::print_hint_block("Run 'metis deps --update' to apply all updates");
}

void SummaryReporter::print_phase_summary(const PhaseSummary& summary) {
  std::vector<std::string> box_lines;

  if (summary.success) {
    box_lines.push_back(Console::green("✓ ") + Console::bold(summary.phase_name) + "  " +
                        Console::dim("passed"));
  } else {
    box_lines.push_back(Console::red("✖ ") + Console::bold(summary.phase_name) + "  " +
                        Console::dim("failed"));
  }

  for (const auto& detail : summary.details) {
    box_lines.push_back("  " + Console::dim(detail));
  }

  Console::print_summary_box(box_lines);
}

void SummaryReporter::print_init_summary(const InitSummary& summary) {
  Console::print_header("metis initialized");

  std::cout << "\n" << Console::bold("Project") << "\n";
  Console::print_bullet("Name:   " + summary.project_name);
  Console::print_bullet("Style:  " + summary.style);

  std::cout << "\n" << Console::bold("Generated files") << "\n";
  for (const auto& file : summary.generated_files) {
    Console::print_check_item(file, true);
  }

  if (summary.clang_tidy) {
    std::cout << "\n" << Console::bold("Static analysis") << "\n";
    Console::print_bullet("Preset: " + summary.tidy_preset);
  }

  Console::print_next_steps({
      Console::bold("metis install") + " — set up pre-commit hooks",
      Console::bold("metis run") + "     — run checks manually",
      Console::bold("git add . && git commit -m \"init\"") + " — first commit",
  });
}

}  // namespace metis::presentation
