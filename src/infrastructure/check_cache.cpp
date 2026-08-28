#include "metis/infrastructure/check_cache.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

namespace metis::infrastructure {
namespace {
constexpr std::string_view k_cache_header = "# metis check cache\n";
constexpr std::string_view k_cache_dir_name = ".metis-cache";
constexpr std::string_view k_cache_file_name = "check-cache";

[[nodiscard]] std::string simple_hash(const std::string& input) {
  std::uint64_t hash = 0xcbf29ce484222325ULL;

  for (unsigned char chr : input) {
    hash ^= chr;
    hash *= 0x100000001b3ULL;
  }

  std::ostringstream oss;
  oss << std::hex << hash;
  return oss.str();
}

[[nodiscard]] std::string file_fingerprint(const std::filesystem::path& path) {
  try {
    if (!std::filesystem::exists(path)) {
      return "missing";
    }

    auto size = std::filesystem::file_size(path);
    auto mtime = std::filesystem::last_write_time(path);

    return std::to_string(size) + ":" + std::to_string(mtime.time_since_epoch().count());
  } catch (...) {
    return "error";
  }
}
}  // namespace

CheckCache::CheckCache(std::filesystem::path repo_root)
  : cache_dir_(repo_root / k_cache_dir_name)
  , cache_file_(cache_dir_ / k_cache_file_name) {
  if (!std::filesystem::exists(cache_dir_)) {
    std::filesystem::create_directories(cache_dir_);
  }
}

std::string CheckCache::make_key(const std::string& check_name, const std::string& check_command,
                                 const std::vector<std::string>& check_args,
                                 const std::vector<std::string>& files) const {
  std::string config_part = check_name + "|" + check_command;

  for (const auto& arg : check_args) {
    config_part += "|" + arg;
  }

  std::string files_part;
  for (const auto& file : files) {
    files_part += file + ":" + file_fingerprint(file) + ";";
  }

  return simple_hash(config_part) + "|" + simple_hash(files_part);
}

bool CheckCache::lookup(const std::string& check_name, const std::string& check_command,
                        const std::vector<std::string>& check_args,
                        const std::vector<std::string>& files, int& out_exit_code) const {
  std::scoped_lock lock(mutex_);

  if (!std::filesystem::exists(cache_file_)) {
    return false;
  }

  auto key = make_key(check_name, check_command, check_args, files);
  auto entries = load();

  for (const auto& entry : entries) {
    if (entry.key == key) {
      out_exit_code = entry.exit_code;
      return true;
    }
  }

  return false;
}

void CheckCache::store(const std::string& check_name, const std::string& check_command,
                       const std::vector<std::string>& check_args,
                       const std::vector<std::string>& files, int exit_code) {
  std::scoped_lock lock(mutex_);
  auto key = make_key(check_name, check_command, check_args, files);
  auto entries = load();

  std::erase_if(entries, [&key](const auto& entr) { return entr.key == key; });
  entries.push_back({.key = key, .exit_code = exit_code});

  save(entries);
}

void CheckCache::clear() {
  std::scoped_lock lock(mutex_);
  if (std::filesystem::exists(cache_file_)) {
    std::filesystem::remove(cache_file_);
  }
}

std::vector<CheckCache::Entry> CheckCache::load() const {
  std::vector<Entry> entries;

  if (!std::filesystem::exists(cache_file_)) {
    return entries;
  }

  std::ifstream in(cache_file_);
  if (!in) {
    return entries;
  }

  std::string line;
  std::getline(in, line);

  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    auto pos = line.rfind('|');
    if (pos == std::string::npos || pos == 0) {
      continue;
    }

    try {
      Entry entry;
      entry.key = line.substr(0, pos);
      entry.exit_code = std::stoi(line.substr(pos + 1));
      entries.push_back(std::move(entry));
    } catch (...) {
      continue;
    }
  }

  return entries;
}

void CheckCache::save(const std::vector<Entry>& entries) const {
  auto temp_path = cache_file_.string() + ".tmp";

  {
    std::ofstream out(temp_path, std::ios::trunc);

    if (!out) {
      return;
    }

    out << k_cache_header;
    for (const auto& entry : entries) {
      out << entry.key << "|" << entry.exit_code << "\n";
    }

    out.close();
  }

  std::error_code err_code;
  std::filesystem::rename(temp_path, cache_file_, err_code);
}

}  // namespace metis::infrastructure
