#ifndef SNIFFERCOMMIT_GENERATORS_CONAN_GENERATOR_HPP
#define SNIFFERCOMMIT_GENERATORS_CONAN_GENERATOR_HPP

#include <string>
#include <string_view>
#include <vector>

namespace sniffercommit::generators {

[[nodiscard]] std::string generate_conanfile(std::string_view project_name, bool enable_testing,
                                             const std::vector<std::string>& dependencies);

}  // namespace sniffercommit::generators

#endif
