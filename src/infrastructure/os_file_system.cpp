#include "sniffercommit/infrastructure/os_file_system.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace sniffercommit::infrastructure {

bool OsFileSystem::exists(const std::filesystem::path& path) {
  return std::filesystem::exists(path);
}

bool OsFileSystem::create_directories(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  return !ec;
}

bool OsFileSystem::write_file(const std::filesystem::path& path, const std::string& content) {
  std::error_code err_code;

  auto parent = path.parent_path();
  if (!parent.empty() && !std::filesystem::exists(parent)) {
    std::filesystem::create_directories(parent, err_code);
    if (err_code) {
      return false;
    }
  }

  auto temp_path = path;
  temp_path += ".tmp";

  {
    std::ofstream out(temp_path, std::ios::trunc);
    if (!out) {
      std::filesystem::remove(temp_path, err_code);
      return false;
    }
    out << content;
    out.flush();
    if (!out.good()) {
      std::filesystem::remove(temp_path, err_code);
      return false;
    }
  }

  std::filesystem::rename(temp_path, path, err_code);
  if (err_code) {
    std::filesystem::copy_file(temp_path, path, std::filesystem::copy_options::overwrite_existing,
                               err_code);
    std::filesystem::remove(temp_path, err_code);
    if (err_code) {
      return false;
    }
  }

  return true;
}

std::string OsFileSystem::read_file(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in) {
    return {};
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

bool OsFileSystem::remove(const std::filesystem::path& path) {
  std::error_code ec;
  return std::filesystem::remove(path, ec);
}

bool OsFileSystem::set_permissions(const std::filesystem::path& path, std::filesystem::perms perms,
                                   std::filesystem::perm_options opts) {
  std::error_code ec;
  std::filesystem::permissions(path, perms, opts, ec);
  return !ec;
}

std::filesystem::path OsFileSystem::current_path() { return std::filesystem::current_path(); }

std::filesystem::path OsFileSystem::absolute(const std::filesystem::path& p) {
  return std::filesystem::absolute(p);
}

}  // namespace sniffercommit::infrastructure
