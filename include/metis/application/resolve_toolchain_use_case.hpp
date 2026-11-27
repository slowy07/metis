#ifndef METIS_APPLICATION_RESOLVE_TOOLCHAIN_USE_CASE_HPP
#define METIS_APPLICATION_RESOLVE_TOOLCHAIN_USE_CASE_HPP

#include <filesystem>
#include <optional>
#include <string_view>

namespace metis::application {
class ResolveToolchainUseCase {
 public:
  [[nodiscard]] std::optional<std::filesystem::path> which(std::string_view command) const;
};
}  // namespace metis::application

#endif  // !METIS_APPLICATION_RESOLVE_TOOLCHAIN_USE_CASE_HPP
