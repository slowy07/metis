#include "metis/infrastructure/compiler/process_compiler_prober.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "metis/domain/compiler/ports.hpp"

#ifdef _WIN32
#define METIS_POPEN _popen
#define METIS_PCLOSE _pclose
#else
#define METIS_POPEN popen
#define METIS_PCLOSE pclose
#endif

namespace metis::infrastructure::compiler {
namespace {
class PipeHandle {
 public:
  explicit PipeHandle(FILE* pipe) : pipe_(pipe) {}
  ~PipeHandle() { (void)close(); }

  PipeHandle(const PipeHandle&) = delete;
  PipeHandle& operator=(const PipeHandle&) = delete;

  [[nodiscard]] FILE* get() const noexcept { return pipe_; }

  int close() {
    if (pipe_ == nullptr) {
      return -1;
    }
    int rc = METIS_PCLOSE(pipe_);
    pipe_ = nullptr;
    return rc;
  }

 private:
  FILE* pipe_ = nullptr;
};

std::string read_all(FILE* pipe) {
  std::string out;
  std::array<char, 256> buffer{};
  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    out += buffer.data();
  }
  return out;
}

std::string first_line(std::string text) {
  auto pos = text.find('\n');
  if (pos != std::string::npos) {
    text.resize(pos);
  }
  if (!text.empty() && text.back() == '\r') {
    text.pop_back();
  }
  return text;
}

std::string shell_quote(const std::string& s) {
  std::string quoted = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') {
      quoted += '\\';
    }
    quoted += c;
  }
  quoted += '"';
  return quoted;
}

}  // namespace

domain::compiler::ProbedCompiler ProcessCompilerProber::probe(
    const std::filesystem::path& cxx) const {
  domain::compiler::ProbedCompiler result;

  {
    std::string cmd = shell_quote(cxx.string()) + " --version 2>&1";
    PipeHandle pipe(METIS_POPEN(cmd.c_str(), "r"));
    if (pipe.get() != nullptr) {
      auto out = first_line(read_all(pipe.get()));
      if (pipe.close() == 0) {
        result.version.raw = std::move(out);
      }
    }
  }

  {
    std::error_code ec;
    auto tmp = std::filesystem::temp_directory_path(ec) / "metis-cxx20-probe.cpp";
    if (!ec) {
      std::ofstream out(tmp, std::ios::trunc);
      if (out) {
        out << "int main(){}\n";
        out.close();
        if (out) {
          std::string cmd = shell_quote(cxx.string()) + " -std=c++20 -fsyntax-only " +
                            shell_quote(tmp.string()) + " 2>&1";
          PipeHandle pipe(METIS_POPEN(cmd.c_str(), "r"));
          if (pipe.get() != nullptr) {
            (void)read_all(pipe.get());
            result.capabilities.cxx20 = (pipe.close() == 0);
          }
        }
      }
      std::filesystem::remove(tmp, ec);
    }
  }

  return result;
}

}  // namespace metis::infrastructure::compiler
