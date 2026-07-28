#ifndef SNIFFERCOMMIT_GENERATORS_CMAKE_GENERATOR_HPP
#define SNIFFERCOMMIT_GENERATORS_CMAKE_GENERATOR_HPP

#include <string>
#include <string_view>
#include <vector>

namespace sniffercommit::generators {

[[nodiscard]] std::string generate_cmake_lists(std::string_view project_name,
                                               std::string_view cpp_standard,
                                               std::string_view target_type, bool enable_testing,
                                               bool enable_sanitizers, bool enable_warnings,
                                               bool enable_clang_tidy, bool use_conan,
                                               const std::vector<std::string>& dependencies);

}  // namespace sniffercommit::generators

#endif
