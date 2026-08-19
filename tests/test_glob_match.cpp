#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "sniffercommit/glob_match.hpp"

namespace {
TEST(GlobMatchTest, BareStarMatchesEverything) {
  EXPECT_TRUE(sniffercommit::util::glob_match("src/main.cpp", "*"));
  EXPECT_TRUE(sniffercommit::util::glob_match("build/foo.o", "*"));
}

TEST(GlobMatchTest, MatchesAnyPatternWithStar) {
  std::vector<std::string> patterns = {"*"};
  EXPECT_TRUE(sniffercommit::util::matches_any_pattern("a/b/c.cpp", patterns));
}
}  // namespace
