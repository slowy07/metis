#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "metis/argparse.hpp"

namespace {

using namespace metis;

// Redirect std::cout to capture help output.
struct CoutCapture {
  std::ostringstream oss;
  std::streambuf* original;

  explicit CoutCapture()
    : original(std::cout.rdbuf(oss.rdbuf())) {}

  explicit CoutCapture(std::ostringstream& sink)
    : original(std::cout.rdbuf(sink.rdbuf())) {}

  ~CoutCapture() { std::cout.rdbuf(original); }

  CoutCapture(const CoutCapture&) = delete;
  CoutCapture& operator=(const CoutCapture&) = delete;
};

// Helper to parse with a C-style argv array.
bool parse_args(ArgParser& parser, const std::vector<std::string>& args,
                std::vector<char*> argv_buf) {
  return parser.parse(static_cast<int>(argv_buf.size()), argv_buf.data());
}

// Build a vector of char* from string args for the parser.
std::vector<char*> make_argv(const std::vector<std::string>& args) {
  std::vector<char*> argv;
  for (auto& s : args) {
    argv.push_back(const_cast<char*>(s.c_str()));
  }
  return argv;
}

// --- Subcommand registration ---

TEST(ArgParserTest, SubcommandReturnsItself) {
  ArgParser parser("app", "desc");
  auto& ref = parser.add_subcommand("run", "Run checks");
  EXPECT_EQ(&ref, &parser);
}

TEST(ArgParserTest, SetSubcommandHelpReturnsItself) {
  ArgParser parser("app", "desc");
  parser.add_subcommand("run", "Run checks");
  auto& ref = parser.set_subcommand_help("run", "Usage:\n  app run");
  EXPECT_EQ(&ref, &parser);
}

TEST(ArgParserTest, SetSubcommandHelpNonexistentIsNoop) {
  ArgParser parser("app", "desc");
  // Should not crash or throw
  parser.set_subcommand_help("nonexistent", "text");
}

// --- Basic parsing ---

TEST(ArgParserTest, ValidSubcommandParsed) {
  ArgParser parser("app", "desc");
  parser.add_subcommand("run", "Run checks");

  std::vector<std::string> args = {"app", "run"};
  auto argv = make_argv(args);
  EXPECT_TRUE(parse_args(parser, args, argv));
  EXPECT_EQ(parser.get_subcommand(), "run");
}

TEST(ArgParserTest, UnknownSubcommandRejected) {
  ArgParser parser("app", "desc");
  parser.add_subcommand("run", "Run checks");

  std::vector<std::string> args = {"app", "nope"};
  auto argv = make_argv(args);
  EXPECT_FALSE(parse_args(parser, args, argv));
  EXPECT_EQ(parser.get_subcommand(), "");
}

TEST(ArgParserTest, NoArgsShowsHelpReturnsFalse) {
  ArgParser parser("app", "desc");

  std::ostringstream captured;
  CoutCapture cap(captured);

  std::vector<std::string> args = {"app"};
  auto argv = make_argv(args);
  EXPECT_FALSE(parse_args(parser, args, argv));
}

// --- Help output ---

TEST(ArgParserTest, GlobalHelpPrintedOnDashHelp) {
  ArgParser parser("myapp", "A tool");
  parser.add_subcommand("run", "Run checks");
  parser.set_version("1.0");

  std::ostringstream captured;
  CoutCapture cap(captured);

  std::vector<std::string> args = {"myapp", "--help"};
  auto argv = make_argv(args);
  EXPECT_FALSE(parse_args(parser, args, argv));

  std::string output = captured.str();
  EXPECT_NE(output.find("myapp - A tool"), std::string::npos);
  EXPECT_NE(output.find("Core Workflow"), std::string::npos);
  EXPECT_NE(output.find("run"), std::string::npos);
  EXPECT_NE(output.find("--version"), std::string::npos);
}

TEST(ArgParserTest, SubcommandHelpPrintedOnSubcmdDashHelp) {
  ArgParser parser("myapp", "A tool");
  parser.add_subcommand("test", "Run tests");
  parser.set_subcommand_help("test", "Usage:\n  myapp test [BUILD_DIR]\n");
  parser.set_version("1.0");

  std::ostringstream captured;
  CoutCapture cap(captured);

  std::vector<std::string> args = {"myapp", "test", "--help"};
  auto argv = make_argv(args);
  EXPECT_FALSE(parse_args(parser, args, argv));

  std::string output = captured.str();
  EXPECT_NE(output.find("myapp test"), std::string::npos);
  EXPECT_NE(output.find("Usage:"), std::string::npos);
  EXPECT_NE(output.find("myapp test [BUILD_DIR]"), std::string::npos);
}

TEST(ArgParserTest, SubcommandHelpShownEvenWithNoHelpText) {
  ArgParser parser("myapp", "A tool");
  parser.add_subcommand("run", "Run checks");
  // No set_subcommand_help called

  std::ostringstream captured;
  CoutCapture cap(captured);

  std::vector<std::string> args = {"myapp", "run", "--help"};
  auto argv = make_argv(args);
  EXPECT_FALSE(parse_args(parser, args, argv));

  std::string output = captured.str();
  EXPECT_NE(output.find("Global Options:"), std::string::npos);
}

// --- Options and flags (global, no subcommand) ---
// When a subcommand matches, parse() stops and main.cpp re-scans the
// remaining args for that subcommand's own flags. This tests the global
// option path (no active subcommand).

TEST(ArgParserTest, GlobalOptionWithValueParsed) {
  ArgParser parser("app", "desc");
  std::string target;
  parser.add_option("-c", "--config", "Config path", target, std::string("default"));

  std::vector<std::string> args = {"app", "--config", "myfile.toml"};
  auto argv = make_argv(args);
  EXPECT_TRUE(parse_args(parser, args, argv));
  EXPECT_EQ(target, "myfile.toml");
}

TEST(ArgParserTest, GlobalOptionDefaultShownInHelpOnly) {
  // The default appears in help text; the caller initializes storage.
  ArgParser parser("app", "desc");
  std::string target;
  parser.add_option("-c", "--config", "Config path", target, std::string("default"));

  std::ostringstream captured;
  CoutCapture cap(captured);

  std::vector<std::string> args = {"app", "--help"};
  auto argv = make_argv(args);
  EXPECT_FALSE(parse_args(parser, args, argv));
  EXPECT_NE(captured.str().find("[default: default]"), std::string::npos);
}

TEST(ArgParserTest, GlobalFlagSetWhenPresent) {
  ArgParser parser("app", "desc");
  bool verbose = false;
  parser.add_flag("-V", "--verbose", "Verbose", verbose);

  std::vector<std::string> args = {"app", "--verbose"};
  auto argv = make_argv(args);
  EXPECT_TRUE(parse_args(parser, args, argv));
  EXPECT_TRUE(verbose);
}

TEST(ArgParserTest, GlobalFlagDefaultsToFalse) {
  ArgParser parser("app", "desc");
  bool verbose = false;
  parser.add_flag("-V", "--verbose", "Verbose", verbose);

  // No flag present — nothing else to trigger help; use a global option arg.
  std::string config;
  parser.add_option("-c", "--config", "Config", config, std::string(""));

  std::vector<std::string> args = {"app", "--config", "x"};
  auto argv = make_argv(args);
  EXPECT_TRUE(parse_args(parser, args, argv));
  EXPECT_FALSE(verbose);
}

TEST(ArgParserTest, UnknownOptionRejectedWithoutSubcommand) {
  ArgParser parser("app", "desc");

  std::ostringstream captured;
  CoutCapture cap(captured);

  std::vector<std::string> args = {"app", "--unknown"};
  auto argv = make_argv(args);
  EXPECT_FALSE(parse_args(parser, args, argv));
}

TEST(ArgParserTest, SubcommandOwnsItsFlags) {
  // When a subcommand matches, parse() stops applying options — the
  // subcommand handler owns the remaining flags.
  ArgParser parser("app", "desc");
  parser.add_subcommand("run", "Run");
  bool verbose = false;
  parser.add_flag("-V", "--verbose", "Verbose", verbose);

  std::vector<std::string> args = {"app", "run", "--verbose"};
  auto argv = make_argv(args);
  EXPECT_TRUE(parse_args(parser, args, argv));
  EXPECT_FALSE(verbose);  // not applied by global option pass
}

TEST(ArgParserTest, OptionRequiresValue) {
  ArgParser parser("app", "desc");
  std::string target;
  parser.add_option("-c", "--config", "Config path", target, std::string("default"));

  std::vector<std::string> args = {"app", "--config"};
  auto argv = make_argv(args);
  EXPECT_FALSE(parse_args(parser, args, argv));
}

// --- Version ---

TEST(ArgParserTest, VersionPrintedOnDashV) {
  ArgParser parser("myapp", "A tool");
  parser.set_version("2.0");

  std::ostringstream captured;
  CoutCapture cap(captured);

  std::vector<std::string> args = {"myapp", "--version"};
  auto argv = make_argv(args);
  EXPECT_FALSE(parse_args(parser, args, argv));
  EXPECT_NE(captured.str().find("myapp 2.0"), std::string::npos);
}

TEST(ArgParserTest, ShortVersionPrintedOnLowercaseV) {
  ArgParser parser("myapp", "A tool");
  parser.set_version("3.0");

  std::ostringstream captured;
  CoutCapture cap(captured);

  std::vector<std::string> args = {"myapp", "-v"};
  auto argv = make_argv(args);
  EXPECT_FALSE(parse_args(parser, args, argv));
  EXPECT_NE(captured.str().find("myapp 3.0"), std::string::npos);
}

// --- Store index regression (flag_stores_ vs option_stores_) ---
// The bug was add_option recording flag_stores_.size() as its store index,
// corrupting option stores when flags and options share the parser.

TEST(ArgParserTest, MultipleGlobalOptionsStoreCorrectly) {
  ArgParser parser("app", "desc");
  std::string config;
  std::string output;
  parser.add_option("-c", "--config", "Config", config, std::string(""));
  parser.add_option("-o", "--output", "Output", output, std::string(""));

  std::vector<std::string> args = {"app", "--output", "result.txt"};
  auto argv = make_argv(args);
  EXPECT_TRUE(parse_args(parser, args, argv));
  EXPECT_EQ(config, "");  // untouched
  EXPECT_EQ(output, "result.txt");
}

TEST(ArgParserTest, GlobalFlagsAndOptionsIndependentStores) {
  ArgParser parser("app", "desc");
  bool verbose = false;
  std::string config;
  parser.add_flag("-V", "--verbose", "Verbose", verbose);
  parser.add_option("-c", "--config", "Config", config, std::string("default"));

  std::vector<std::string> args = {"app", "-V", "--config", "custom.toml"};
  auto argv = make_argv(args);
  EXPECT_TRUE(parse_args(parser, args, argv));
  EXPECT_TRUE(verbose);
  EXPECT_EQ(config, "custom.toml");
}

// --- Help content spot checks ---

TEST(ArgParserTest, HelpListsAllSubcommands) {
  ArgParser parser("metis", "Fast C++20-powered pre-commit & CI generator");
  parser.add_subcommand("init", "Create default .metis.toml");
  parser.add_subcommand("install", "Install pre-commit hook");
  parser.add_subcommand("run", "Execute checks on files");

  std::ostringstream captured;
  CoutCapture cap(captured);

  std::vector<std::string> args = {"metis", "--help"};
  auto argv = make_argv(args);
  parse_args(parser, args, argv);

  std::string output = captured.str();
  EXPECT_NE(output.find("init"), std::string::npos);
  EXPECT_NE(output.find("install"), std::string::npos);
  EXPECT_NE(output.find("run"), std::string::npos);
  EXPECT_NE(output.find("Core Workflow"), std::string::npos);
  EXPECT_NE(output.find("Examples"), std::string::npos);
  EXPECT_NE(output.find("Global Options"), std::string::npos);
}

TEST(ArgParserTest, SubcommandHelpIncludesGlobalOptions) {
  ArgParser parser("app", "desc");
  parser.add_subcommand("run", "Run");
  std::string config;
  parser.add_option("-c", "--config", "Config", config, std::string("default"));

  std::ostringstream captured;
  CoutCapture cap(captured);

  std::vector<std::string> args = {"app", "run", "--help"};
  auto argv = make_argv(args);
  parse_args(parser, args, argv);

  std::string output = captured.str();
  EXPECT_NE(output.find("Global Options:"), std::string::npos);
  EXPECT_NE(output.find("--config"), std::string::npos);
}

TEST(ArgParserTest, ShortFlagPrintedInHelpWhenPresent) {
  ArgParser parser("app", "desc");
  parser.add_subcommand("run", "Run");
  std::string config;
  parser.add_option("-c", "--config", "Config", config, std::string(""));

  std::ostringstream captured;
  CoutCapture cap(captured);

  std::vector<std::string> args = {"app", "--help"};
  auto argv = make_argv(args);
  parse_args(parser, args, argv);

  std::string output = captured.str();
  EXPECT_NE(output.find("-c, --config"), std::string::npos);
}

}  // namespace
