#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "metis/glob_match.hpp"

namespace {
TEST(GlobMatchTest, BareStarMatchesEverything) {
  EXPECT_TRUE(metis::util::glob_match("src/main.cpp", "*"));
  EXPECT_TRUE(metis::util::glob_match("build/foo.o", "*"));
}

TEST(GlobMatchTest, MatchesAnyPatternWithStar) {
  std::vector<std::string> patterns = {"*"};
  EXPECT_TRUE(metis::util::matches_any_pattern("a/b/c.cpp", patterns));
}
}  // namespace
