cmake_minimum_required(VERSION 3.20)

project(metis
  LANGUAGES CXX
  VERSION 0.4.0
  DESCRIPTION "Fast, C++ pre-commit hook and CI generator"
  HOMEPAGE_URL "https://github.com/slowy07/metis"
)

# Build environment setup
# Enforce out-of-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are not allowed. Please run:\n"
    "  rm -rf build && mkdir build && cd build\n"
    "  cmake .. && cmake --build . --parallel"
  )
endif()

# Set a default build type if none provided
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
  message(STATUS "Default to Release build type")
endif()

# Global CMake settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# User options
option(METIS_ENABLE_SANITIZERS "Enable Address/UB sanitizers (Debug only)" OFF)
option(METIS_BUILD_TESTS "Build unit tests" ON)
option(METIS_USE_SYSTEM_FMT "Use system-installed fmt" OFF)
option(METIS_USE_SYSTEM_TOMLPLUSPLUS "Use system-installed tomlplusplus" OFF)

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(METIS_PLATFORM "linux")
  if(DEFINED ENV{WSL_DISTRO_NAME})
    set(METIS_PLATFORM "wsl")
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(METIS_PLATFORM "macos")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(METIS_PLATFORM "windows")
else()
  message(WARNING "Unknown platform: ${CMAKE_SYSTEM_NAME}")
  set(METIS_PLATFORM "unknown")
endif()

# Compiler detection
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(METIS_COMPILER "gcc-${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(METIS_COMPILER "clang-${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(METIS_COMPILER "msvc-${CMAKE_CXX_COMPILER_VERSION}")
else()
  set(METIS_COMPILER "unknown")
endif()

# Build metadata (git commit, timestamp)
# Use UTC timestamp without shell expressions for portability
string(TIMESTAMP METIS_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)

execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE METIS_GIT_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT METIS_GIT_COMMIT)
  set(METIS_GIT_COMMIT "unknown")
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/cmake/config.hpp.in
  ${CMAKE_BINARY_DIR}/generated/metis/config_build.hpp
  @ONLY
)

# Dependencies: fmt and tomlplusplus
include(FetchContent)

if(METIS_USE_SYSTEM_FMT)
  find_package(fmt REQUIRED)
else()
  FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        11.0.2
    GIT_SHALLOW    ON
    EXCLUDE_FROM_ALL
  )
  FetchContent_MakeAvailable(fmt)
endif()

if(METIS_USE_SYSTEM_TOMLPLUSPLUS)
  find_package(tomlplusplus REQUIRED)
else()
  FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    ON
    EXCLUDE_FROM_ALL
  )
  FetchContent_MakeAvailable(tomlplusplus)
endif()

# Library target (core logic)
set(METIS_SOURCES
  src/argparse.cpp
  src/domain/config.cpp
  src/domain/check.cpp
  src/domain/workflow.cpp
  src/application/init_use_case.cpp
  src/application/install_use_case.cpp
  src/application/run_checks_use_case.cpp
  src/application/generate_workflow_use_case.cpp
  src/application/install_toolchain_use_case.cpp
  src/application/dependency_check_use_case.cpp
  src/application/perf_checks_use_case.cpp
  src/application/build_use_case.cpp
  src/application/checks/shell_check.cpp
  src/application/checks/clang_format_check.cpp
  src/application/checks/clang_tidy_check.cpp
  src/application/checks/compiler_check.cpp
  src/application/checks/build_check.cpp
  src/application/checks/git_diff_check.cpp
  src/application/sanitizer_checks_use_case.cpp
  src/application/checks/cppcheck_check.cpp
  src/application/checks/iwyu_check.cpp
  src/application/checks/clang_static_analyzer_check.cpp
  src/application/checks/gcc_analyzer_check.cpp
  src/application/checks/security_check.cpp
  src/application/checks/dependency_security_check.cpp
  src/infrastructure/toml_config_repository.cpp
  src/infrastructure/process_shell_executor.cpp
  src/infrastructure/os_file_system.cpp
  src/infrastructure/cli_git_repository.cpp
  src/infrastructure/curl_http_client.cpp
  src/infrastructure/posix_toolchain_provider.cpp
  src/infrastructure/tar_archive_extractor.cpp
  src/infrastructure/toolchain_factory.cpp
  src/infrastructure/windows_gcc_provider.cpp
  src/infrastructure/zip_archive_extractor.cpp
  src/generators/clang_format_generator.cpp
  src/generators/clang_tidy_generator.cpp
  src/generators/cmake_generator.cpp
  src/generators/conan_generator.cpp
  src/presentation/interactive_init.cpp
  src/presentation/console.cpp
  src/presentation/summary_reporter.cpp
  src/spinner.cpp
  src/util.cpp
  src/glob_match.cpp
  src/application/test_checks_use_case.cpp
  $<$<PLATFORM_ID:Windows>:src/infrastructure/windows_clang_provider.cpp>
)

set(METIS_HEADERS
  include/metis/argparse.hpp
  include/metis/domain/config.hpp
  include/metis/domain/workflow.hpp
  include/metis/domain/error_codes.hpp
  include/metis/domain/check.hpp
  include/metis/domain/ports/config_repository.hpp
  include/metis/domain/ports/shell_executor.hpp
  include/metis/domain/ports/file_system.hpp
  include/metis/domain/ports/toolchain_provider.hpp
  include/metis/domain/ports/http_client.hpp
  include/metis/domain/ports/archive_extractor.hpp
  include/metis/domain/ports/git_repository.hpp
  include/metis/application/init_use_case.hpp
  include/metis/application/install_use_case.hpp
  include/metis/application/run_checks_use_case.hpp
  include/metis/application/generate_workflow_use_case.hpp
  include/metis/application/install_toolchain_use_case.hpp
  include/metis/application/perf_checks_use_case.hpp
  include/metis/application/checks/shell_check.hpp
  include/metis/application/checks/clang_format_check.hpp
  include/metis/application/checks/clang_tidy_check.hpp
  include/metis/application/checks/compiler_check.hpp
  include/metis/application/checks/build_check.hpp
  include/metis/application/checks/git_diff_check.hpp
  include/metis/application/run_checks_use_case.hpp
  include/metis/application/sanitizer_checks_use_case.hpp
  include/metis/application/test_checks_use_case.hpp
  include/metis/application/build_use_case.hpp
  include/metis/application/checks/cppcheck_check.hpp
  include/metis/application/checks/iwyu_check.hpp
  include/metis/application/checks/clang_static_analyzer_check.hpp
  include/metis/application/checks/gcc_analyzer_check.hpp
  include/metis/application/checks/security_check.hpp
  include/metis/application/checks/dependency_security_check.hpp
  include/metis/infrastructure/toml_config_repository.hpp
  include/metis/infrastructure/process_shell_executor.hpp
  include/metis/infrastructure/os_file_system.hpp
  include/metis/infrastructure/cli_git_repository.hpp
  include/metis/infrastructure/curl_http_client.hpp
  include/metis/infrastructure/posix_toolchain_provider.hpp
  include/metis/infrastructure/tar_archive_extractor.hpp
  include/metis/infrastructure/toolchain_factory.hpp
  include/metis/infrastructure/windows_clang_provider.hpp
  include/metis/infrastructure/windows_gcc_provider.hpp
  include/metis/infrastructure/zip_archive_extractor.hpp
  include/metis/generators/clang_format_generator.hpp
  include/metis/generators/clang_tidy_generator.hpp
  include/metis/generators/cmake_generator.hpp
  include/metis/generators/conan_generator.hpp
  include/metis/presentation/interactive_init.hpp
  include/metis/spinner.hpp
  include/metis/util.hpp
  include/metis/glob_match.hpp
)

add_library(metis_lib STATIC ${METIS_SOURCES})

target_include_directories(metis_lib
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(metis_lib PUBLIC cxx_std_20)

target_compile_definitions(metis_lib
  PRIVATE
    METIS_VERSION="${PROJECT_VERSION}"
    METIS_PLATFORM="${METIS_PLATFORM}"
    METIS_COMPILER="${METIS_COMPILER}"
    $<$<CONFIG:Debug>:METIS_DEBUG>
    $<$<STREQUAL:${METIS_PLATFORM},wsl>:METIS_PLATFORM_WSL>
    $<$<STREQUAL:${METIS_PLATFORM},macos>:METIS_PLATFORM_MACOS>
    $<$<STREQUAL:${METIS_PLATFORM},windows>:METIS_PLATFORM_WINDOWS>
)

target_link_libraries(metis_lib
  PRIVATE
    tomlplusplus::tomlplusplus
    fmt::fmt
)

# Executable target (CLI frontend)
add_executable(metis src/main.cpp)
target_link_libraries(metis
  PRIVATE
    metis_lib
    tomlplusplus::tomlplusplus
    fmt::fmt
)

target_include_directories(metis
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(metis PUBLIC cxx_std_20)

target_compile_definitions(metis
  PRIVATE
    METIS_VERSION="${PROJECT_VERSION}"
    METIS_PLATFORM="${METIS_PLATFORM}"
    METIS_COMPILER="${METIS_COMPILER}"
    $<$<CONFIG:Debug>:METIS_DEBUG>
)

# Compiler warnings & hardening (applied to both library and executable)
function(apply_warnings_and_hardening TARGET)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${TARGET} PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
      -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
      -Wunused -Woverloaded-virtual -Wnull-dereference
      $<$<CONFIG:Debug>:-O0 -g3>
      $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )

    if(NOT WIN32)
      target_link_options(${TARGET} PRIVATE
        $<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,-z,relro,-z,now>
        $<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,-z,noexecstack>
      )
      if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${TARGET} PRIVATE -fstack-protector-strong)
      endif()
    endif()

  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${TARGET} PRIVATE /W4 /permissive- /sdl /D_CRT_SECURE_NO_WARNINGS)
    target_link_options(${TARGET} PRIVATE /DYNAMICBASE /NXCOMPAT)
  endif()
endfunction()

apply_warnings_and_hardening(metis)
apply_warnings_and_hardening(metis_lib)

# Sanitizers (Debug only, applied to both library and executable)
if(METIS_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Enabling AddressSanitizer + UndefinedBehaviorSanitizer")
    foreach(target IN ITEMS metis metis_lib)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
  endif()
endif()

# Unit tests (Google Test)
if(METIS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Installation and CPack
include(GNUInstallDirs)

install(TARGETS metis RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/metis/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/metis
  FILES_MATCHING PATTERN "*.hpp"
)

install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/metis)

# CPack configuration
set(CPACK_PACKAGE_NAME "metis")
set(CPACK_PACKAGE_VENDOR "slowy07")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_CONTACT "slowy07")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# Architecture detection for packages
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
  set(METIS_ARCH "amd64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  set(METIS_ARCH "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
  set(METIS_ARCH "armhf")
else()
  set(METIS_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CPACK_GENERATOR "DEB")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "slowy07")
  set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "git, bash")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${METIS_ARCH}")
elseif(APPLE)
  set(CPACK_GENERATOR "TGZ")
  if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
  endif()
endif()

include(CPack)
