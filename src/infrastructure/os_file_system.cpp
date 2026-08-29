#include "metis/infrastructure/os_file_system.hpp"

#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <system_error>

namespace metis::infrastructure {

bool OsFileSystem::exists(const std::filesystem::path& path) {
  std::error_code err_code;
  return std::filesystem::exists(path, err_code);
}

bool OsFileSystem::create_directories(const std::filesystem::path& path) {
  std::error_code err_code;
  std::filesystem::create_directories(path, err_code);
  return !err_code;
}

bool OsFileSystem::write_file(const std::filesystem::path& path, const std::string& content) {
  std::error_code err_code;

  auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, err_code);
    if (err_code) {
      return false;
    }
  }

  auto temp_path = path;
  temp_path += ".tmp";

  auto cleanup_temp = [&]() {
    std::error_code cleanup_ec;
    std::filesystem::remove(temp_path, cleanup_ec);
  };

  try {
    std::ofstream out(temp_path, std::ios::trunc | std::ios::binary);
    if (!out) {
      cleanup_temp();
      return false;
    }
    out << content;
    out.flush();
    if (!out.good()) {
      cleanup_temp();
      return false;
    }
  } catch (...) {
    cleanup_temp();
    return false;
  }

  std::filesystem::rename(temp_path, path, err_code);
  if (err_code) {
    cleanup_temp();
    return false;
  }

  return true;
}

std::string OsFileSystem::read_file(const std::filesystem::path& path) {
  std::ifstream infile(path);
  if (!infile) {
    return {};
  }
  std::ostringstream stringstream;
  stringstream << infile.rdbuf();
  return stringstream.str();
}

bool OsFileSystem::set_permissions(const std::filesystem::path& path, std::filesystem::perms perms,
                                   std::filesystem::perm_options opts) {
  std::error_code err_code;
  std::filesystem::permissions(path, perms, opts, err_code);
  return !err_code;
}

std::filesystem::path OsFileSystem::current_path() {
  std::error_code err_code;
  auto curr_path = std::filesystem::current_path(err_code);
  return err_code ? std::filesystem::path{} : curr_path;
}

std::filesystem::path OsFileSystem::absolute(const std::filesystem::path& path) {
  std::error_code err_code;
  auto abs_path = std::filesystem::absolute(path, err_code);
  return err_code ? std::filesystem::path{} : abs_path;
}

}  // namespace metis::infrastructure
