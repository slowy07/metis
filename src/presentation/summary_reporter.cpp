#include "metis/presentation/summary_reporter.hpp"

#include <fmt/format.h>

#include <iostream>
#include <string>
#include <vector>

#include "metis/presentation/console.hpp"

namespace metis::presentation {

void SummaryReporter::print_run_summary(const RunSummary& summary) {
  std::vector<std::string> box_lines;

  if (summary.failed == 0) {
    box_lines.push_back(Console::green("All checks passed") + "  " +
                        Console::dim(fmt::format("({} checked, {:.1f}s)", summary.total_checks,
                                                 summary.duration_sec)));
  } else {
    box_lines.push_back(Console::red("Checks failed") + "  " +
                        Console::dim(fmt::format("({} passed, {} failed, {} skipped)",
                                                 summary.passed, summary.failed, summary.skipped)));
  }

  if (summary.parallel) {
    box_lines.push_back(Console::dim("Executed in parallel mode"));
  }

  Console::print_summary_box(box_lines);

  if (summary.failed > 0) {
    Console::print_next_steps({
        "Fix the failing checks above",
        "Run " + Console::bold("metis run --verbose") + " for full output",
        "Skip temporarily with " + Console::bold("git commit --no-verify") + " (not recommended)",
    });
  } else {
    Console::print_next_steps({
        "Stage your changes: " + Console::bold("git add -u"),
        "Commit: " + Console::bold("git commit -m \"your message\""),
    });
  }
}

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

  if (summary.invalid == 0 && summary.duplicates == 0 && summary.lockfile_issues == 0) {
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
  }

  Console::print_summary_box(box_lines);
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
  Console::print_header("metis initialied");

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

void SummaryReporter::print_empty_line() { std::cout << "\n"; }

void SummaryReporter::print_done() { std::cout << "\n" << Console::green("Done.") << "\n"; }

}  // namespace metis::presentation
