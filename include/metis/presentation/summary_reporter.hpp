#ifndef METIS_PRESENTATION_SUMMARY_REPORTER_HPP
#define METIS_PRESENTATION_SUMMARY_REPORTER_HPP

#include <string>
#include <vector>

#include "metis/domain/config.hpp"

namespace metis::presentation {
  class SummaryReporter {
    public:
      struct RunSummary {
        int total_checks = 0;
        int passed = 0;
        int failed = 0;
        int skipped = 0;
        double duration_sec = 0.0;
        bool parallel = false;
      };

      static void print_run_summary(const RunSummary& summary);

      struct TestSummary {
        bool success = false;
        int total_tests = 0;
        int failed_tests = 0;
        double line_coverage = -1.0;
        double branch_coverage = -1.0;
        double function_coverage = -1.0;
        bool coverage_enabled = false;
        bool coverage_ok = true;
      };

      static void print_test_summary(const TestSummary& summary);

      struct DepSummary {
        int total = 0;
        int valid = 0;
        int invalid = 0;
        int duplicates = 0;
        int lockfile_issues = 0;
      };

      static void print_dep_summary(const DepSummary& summary);

      struct PhaseSummary {
        std::string phase_name;
        bool success = false;
        std::vector<std::string> details;
      };

      static void print_phase_summary(const PhaseSummary& summary);

      struct InitSummary {
        std::string project_name;
        std::string style;
        bool clang_tidy = false;
        std::string tidy_preset;
        bool cmake = false;
        bool conan = false;
        bool compiler_checks = false;
        std::vector<std::string> generated_files;
      };

      static void print_init_summary(const InitSummary& summary);

      static void print_empty_line();
      static void print_done();
  };
}

#endif // !METIS_PRESENTATION_SUMMARY_REPORTER_HPP
