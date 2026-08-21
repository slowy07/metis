#include <gtest/gtest.h>

#include <cstdlib>

// and parses CLI args. Output content assertions add complexity without
// catching more integration bugs — if --help silently became --hel, the
// exit code would change or the parser would crash.
TEST(BinaryRuntimeTest, HelpExitsZero) {
  EXPECT_EQ(std::system(METIS_BINARY_PATH " --help > /dev/null 2>&1"), 0);
}

TEST(BinaryRuntimeTest, VersionExitsZero) {
  EXPECT_EQ(std::system(METIS_BINARY_PATH " --version > /dev/null 2>&1"), 0);
}
