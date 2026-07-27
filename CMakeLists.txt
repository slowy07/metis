cmake_minimum_required(VERSION 3.20)

project(sniffercommit
  LANGUAGES CXX
  VERSION 0.3.20
  DESCRIPTION "Fast, C++ pre-commit hook and CI generator"
  HOMEPAGE_URL "https://github.com/slowy07/sniffercommit"
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
option(SNIFFERCOMMIT_ENABLE_SANITIZERS "Enable Address/UB sanitizers (Debug only)" OFF)
option(SNIFFERCOMMIT_BUILD_TESTS "Build unit tests" ON)
option(SNIFFERCOMMIT_USE_SYSTEM_FMT "Use system-installed fmt" OFF)
option(SNIFFERCOMMIT_USE_SYSTEM_TOMLPLUSPLUS "Use system-installed tomlplusplus" OFF)

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(SNIFFERCOMMIT_PLATFORM "linux")
  if(DEFINED ENV{WSL_DISTRO_NAME})
    set(SNIFFERCOMMIT_PLATFORM "wsl")
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(SNIFFERCOMMIT_PLATFORM "macos")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(SNIFFERCOMMIT_PLATFORM "windows")
else()
  message(WARNING "Unknown platform: ${CMAKE_SYSTEM_NAME}")
  set(SNIFFERCOMMIT_PLATFORM "unknown")
endif()

# Compiler detection
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(SNIFFERCOMMIT_COMPILER "gcc-${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(SNIFFERCOMMIT_COMPILER "clang-${CMAKE_CXX_COMPILER_VERSION}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(SNIFFERCOMMIT_COMPILER "msvc-${CMAKE_CXX_COMPILER_VERSION}")
else()
  set(SNIFFERCOMMIT_COMPILER "unknown")
endif()

# Build metadata (git commit, timestamp)
# Use UTC timestamp without shell expressions for portability
string(TIMESTAMP SNIFFERCOMMIT_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)

execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE SNIFFERCOMMIT_GIT_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT SNIFFERCOMMIT_GIT_COMMIT)
  set(SNIFFERCOMMIT_GIT_COMMIT "unknown")
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/cmake/config.hpp.in
  ${CMAKE_BINARY_DIR}/generated/sniffercommit/config_build.hpp
  @ONLY
)

# Dependencies: fmt and tomlplusplus
include(FetchContent)

if(SNIFFERCOMMIT_USE_SYSTEM_FMT)
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

if(SNIFFERCOMMIT_USE_SYSTEM_TOMLPLUSPLUS)
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
set(SNIFFERCOMMIT_SOURCES
  src/domain/config.cpp
  src/domain/workflow.cpp
  src/application/init_use_case.cpp
  src/application/install_use_case.cpp
  src/application/run_checks_use_case.cpp
  src/application/generate_workflow_use_case.cpp
  src/infrastructure/toml_config_repository.cpp
  src/infrastructure/process_shell_executor.cpp
  src/infrastructure/os_file_system.cpp
  src/infrastructure/cli_git_repository.cpp
  src/generators/clang_format_generator.cpp
  src/generators/clang_tidy_generator.cpp
  src/generators/cmake_generator.cpp
  src/generators/conan_generator.cpp
  src/presentation/interactive_init.cpp
  src/spinner.cpp
  src/util.cpp
  src/glob_match.cpp
)

set(SNIFFERCOMMIT_HEADERS
  include/sniffercommit/argparse.hpp
  include/sniffercommit/domain/config.hpp
  include/sniffercommit/domain/workflow.hpp
  include/sniffercommit/domain/error_codes.hpp
  include/sniffercommit/domain/ports/config_repository.hpp
  include/sniffercommit/domain/ports/shell_executor.hpp
  include/sniffercommit/domain/ports/file_system.hpp
  include/sniffercommit/domain/ports/git_repository.hpp
  include/sniffercommit/application/init_use_case.hpp
  include/sniffercommit/application/install_use_case.hpp
  include/sniffercommit/application/run_checks_use_case.hpp
  include/sniffercommit/application/generate_workflow_use_case.hpp
  include/sniffercommit/infrastructure/toml_config_repository.hpp
  include/sniffercommit/infrastructure/process_shell_executor.hpp
  include/sniffercommit/infrastructure/os_file_system.hpp
  include/sniffercommit/infrastructure/cli_git_repository.hpp
  include/sniffercommit/generators/clang_format_generator.hpp
  include/sniffercommit/generators/clang_tidy_generator.hpp
  include/sniffercommit/generators/cmake_generator.hpp
  include/sniffercommit/generators/conan_generator.hpp
  include/sniffercommit/presentation/interactive_init.hpp
  include/sniffercommit/spinner.hpp
  include/sniffercommit/util.hpp
  include/sniffercommit/glob_match.hpp
)

add_library(sniffercommit_lib STATIC ${SNIFFERCOMMIT_SOURCES})

target_include_directories(sniffercommit_lib
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(sniffercommit_lib PUBLIC cxx_std_20)

target_compile_definitions(sniffercommit_lib
  PRIVATE
    SNIFFERCOMMIT_VERSION="${PROJECT_VERSION}"
    SNIFFERCOMMIT_PLATFORM="${SNIFFERCOMMIT_PLATFORM}"
    SNIFFERCOMMIT_COMPILER="${SNIFFERCOMMIT_COMPILER}"
    $<$<CONFIG:Debug>:SNIFFERCOMMIT_DEBUG>
    $<$<STREQUAL:${SNIFFERCOMMIT_PLATFORM},wsl>:SNIFFERCOMMIT_PLATFORM_WSL>
    $<$<STREQUAL:${SNIFFERCOMMIT_PLATFORM},macos>:SNIFFERCOMMIT_PLATFORM_MACOS>
    $<$<STREQUAL:${SNIFFERCOMMIT_PLATFORM},windows>:SNIFFERCOMMIT_PLATFORM_WINDOWS>
)

target_link_libraries(sniffercommit_lib
  PRIVATE
    tomlplusplus::tomlplusplus
    fmt::fmt
)

# Executable target (CLI frontend)
add_executable(sniffercommit src/main.cpp)
target_link_libraries(sniffercommit
  PRIVATE
    sniffercommit_lib
    tomlplusplus::tomlplusplus
    fmt::fmt
)

target_include_directories(sniffercommit
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(sniffercommit PUBLIC cxx_std_20)

target_compile_definitions(sniffercommit
  PRIVATE
    SNIFFERCOMMIT_VERSION="${PROJECT_VERSION}"
    SNIFFERCOMMIT_PLATFORM="${SNIFFERCOMMIT_PLATFORM}"
    SNIFFERCOMMIT_COMPILER="${SNIFFERCOMMIT_COMPILER}"
    $<$<CONFIG:Debug>:SNIFFERCOMMIT_DEBUG>
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

apply_warnings_and_hardening(sniffercommit)
apply_warnings_and_hardening(sniffercommit_lib)

# Sanitizers (Debug only, applied to both library and executable)
if(SNIFFERCOMMIT_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Enabling AddressSanitizer + UndefinedBehaviorSanitizer")
    foreach(target IN ITEMS sniffercommit sniffercommit_lib)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
  endif()
endif()

# Static linking (Unix only, for the final executable)
option(SNIFFERCOMMIT_PORTABLE_RELEASE "Build static portable release binary" OFF)

if(SNIFFERCOMMIT_PORTABLE_RELEASE)
  set(SNIFFERCOMMIT_USE_SYSTEM_FMT OFF CACHE BOOL "" FORCE)
  set(SNIFFERCOMMIT_USE_SYSTEM_TOMLPLUSPLUS OFF CACHE BOOL "" FORCE)
endif()

if(SNIFFERCOMMIT_PORTABLE_RELEASE AND UNIX AND NOT APPLE)
  message(STATUS "building fully static portable binary")

  target_compile_options(sniffercommit PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
  )

  target_link_options(sniffercommit PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
  )
endif()

# Unit tests (Google Test)
if(SNIFFERCOMMIT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Installation and CPack
include(GNUInstallDirs)

install(TARGETS sniffercommit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/sniffercommit/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sniffercommit
  FILES_MATCHING PATTERN "*.hpp"
)

install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/sniffercommit)

# CPack configuration
set(CPACK_PACKAGE_NAME "sniffercommit")
set(CPACK_PACKAGE_VENDOR "slowy07")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_CONTACT "slowy07")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# Architecture detection for packages
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
  set(SNIFFERCOMMIT_ARCH "amd64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  set(SNIFFERCOMMIT_ARCH "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
  set(SNIFFERCOMMIT_ARCH "armhf")
else()
  set(SNIFFERCOMMIT_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CPACK_GENERATOR "DEB")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "slowy07")
  set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "git, bash")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${SNIFFERCOMMIT_ARCH}")
elseif(APPLE)
  set(CPACK_GENERATOR "TGZ")
  if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
  endif()
endif()

include(CPack)
