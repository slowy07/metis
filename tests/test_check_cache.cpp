#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "metis/infrastructure/check_cache.hpp"

namespace {

namespace fs = std::filesystem;

class TempDir {
 public:
  TempDir() {
    path_ = fs::temp_directory_path() / "metis_check_cache_tests";
    fs::remove_all(path_);
    fs::create_directories(path_);
  }
  ~TempDir() { fs::remove_all(path_); }
  fs::path path() const { return path_; }
  fs::path make_file(const std::string& name, const std::string& content = "x\n") {
    auto p = path_ / name;
    std::ofstream(p) << content;
    return p;
  }

 private:
  fs::path path_;
};

TEST(CheckCacheTest, StoreThenLookupHits) {
  TempDir tmp;
  auto file = tmp.make_file("a.cpp");
  {
    metis::infrastructure::CheckCache cache(tmp.path());
    cache.store("lint", "clang-tidy", {"--quiet"}, {file.string()}, 0);
  }
  {
    metis::infrastructure::CheckCache cache(tmp.path());
    int exit_code = -1;
    bool hit = cache.lookup("lint", "clang-tidy", {"--quiet"}, {file.string()}, exit_code);
    EXPECT_TRUE(hit);
    EXPECT_EQ(exit_code, 0);
  }
}

TEST(CheckCacheTest, DifferentArgsMispresent) {
  TempDir tmp;
  auto file = tmp.make_file("a.cpp");
  metis::infrastructure::CheckCache cache(tmp.path());
  cache.store("lint", "clang-tidy", {"--quiet"}, {file.string()}, 0);

  int exit_code = -1;
  EXPECT_FALSE(cache.lookup("lint", "clang-tidy", {"--verbose"}, {file.string()}, exit_code));
}

TEST(CheckCacheTest, ChangedFileInvalidatesEntry) {
  TempDir tmp;
  auto file = tmp.make_file("a.cpp", "aaa");
  {
    metis::infrastructure::CheckCache cache(tmp.path());
    cache.store("lint", "grep", {"-E", "zzz"}, {file.string()}, 0);
  }

  std::ofstream(file) << "changed content\n";

  metis::infrastructure::CheckCache cache(tmp.path());
  int exit_code = -1;
  EXPECT_FALSE(cache.lookup("lint", "grep", {"-E", "zzz"}, {file.string()}, exit_code));
}

TEST(CheckCacheTest, MissingDirectoryCreatesCacheFile) {
  TempDir tmp;
  auto file = tmp.make_file("a.cpp");
  metis::infrastructure::CheckCache cache(tmp.path());
  cache.store("check", "true", {}, {file.string()}, 0);
  EXPECT_TRUE(fs::exists(tmp.path() / ".metis-cache" / "check-cache"));
}

}  // namespace
