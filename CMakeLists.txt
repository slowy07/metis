cmake_minimum_required(VERSION 3.20)

project(sniffercommit
  LANGUAGES CXX
  VERSION 0.2.1
  DESCRIPTION "Fast, C++ pre-commit hook and CI generator"
  HOMEPAGE_URL "https://github.com/slowy07/sniffercommit"
)

# Enforce out-of-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are not allowed. Please run:\n"
    "  rm -rf build && mkdir build && cd build\n"
    "  cmake .. && cmake --build . --parallel"
  )
endif()

# Global config
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Cache options (consistent naming)
option(SNIFFERCOMMIT_ENABLE_SANITIZERS "Enable Address/UB sanitizers (Debug only)" OFF)
option(SNIFFERCOMMIT_ENABLE_STATIC_LINK "Link statically for portable binary" OFF)
option(SNIFFERCOMMIT_VERBOSE_CONFIG "Print detailed configuration summary" ON)

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(SNIFFERCOMMIT_PLATFORM "linux" CACHE INTERNAL "Detected platform")
  if(DEFINED ENV{WSL_DISTRO_NAME})
    set(SNIFFERCOMMIT_PLATFORM "wsl" CACHE INTERNAL "Detected platform")
    add_compile_definitions(SNIFFERCOMMIT_PLATFORM_WSL)
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(SNIFFERCOMMIT_PLATFORM "macos" CACHE INTERNAL "Detected platform")
  add_compile_definitions(SNIFFERCOMMIT_PLATFORM_MACOS)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(SNIFFERCOMMIT_PLATFORM "windows" CACHE INTERNAL "Detected platform")
  add_compile_definitions(SNIFFERCOMMIT_PLATFORM_WINDOWS)
else()
  message(WARNING "Unknown platform: ${CMAKE_SYSTEM_NAME}")
  set(SNIFFERCOMMIT_PLATFORM "unknown" CACHE INTERNAL "Detected platform")
endif()

# Compiler detection
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(SNIFFERCOMMIT_COMPILER "gcc-${CMAKE_CXX_COMPILER_VERSION}" CACHE INTERNAL "Compiler")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(SNIFFERCOMMIT_COMPILER "clang-${CMAKE_CXX_COMPILER_VERSION}" CACHE INTERNAL "Compiler")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(SNIFFERCOMMIT_COMPILER "msvc-${CMAKE_CXX_COMPILER_VERSION}" CACHE INTERNAL "Compiler")
else()
  # FIXED: was message(), should be set()
  set(SNIFFERCOMMIT_COMPILER "unknown" CACHE INTERNAL "Compiler")
endif()

# Build metadata
set(SNIFFERCOMMIT_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
set(SNIFFERCOMMIT_BUILD_TIMESTAMP "$<IF:$<BOOL:${SOURCE_DATE_EPOCH}>,${SOURCE_DATE_EPOCH},$<SHELL_EXPR:date -u +%Y-%m-%dT%H:%M:%SZ>>")

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

# Dependencies
option(SNIFFERCOMMIT_USE_SYSTEM_FMT
  "Use system-installed fmt"
  ON
)

option(SNIFFERCOMMIT_USE_SYSTEM_TOMLPLUSPLUS
  "Use system-installed tomlplusplus"
  OFF
)

include(FetchContent)

# fmt
if(SNIFFERCOMMIT_USE_SYSTEM_FMT)
  find_package(fmt REQUIRED)
else()
  FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        11.0.2
    GIT_SHALLOW    ON
  )
  FetchContent_MakeAvailable(fmt)
endif()

# tomlplusplus
if(SNIFFERCOMMIT_USE_SYSTEM_TOMLPLUSPLUS)
  find_package(tomlplusplus REQUIRED)
else()
  FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    ON
  )
  FetchContent_MakeAvailable(tomlplusplus)
endif()

# Target definition
add_executable(sniffercommit
  src/main.cpp
  src/project_config.cpp
  src/tooling_config.cpp
  src/precommit_domain.cpp
  src/cicd_domain.cpp
  src/config_manager.cpp
  src/executor.cpp
)

target_include_directories(sniffercommit
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(sniffercommit PUBLIC cxx_std_20)

# FIXED: Use target_compile_definitions for compile-time macros
target_compile_definitions(sniffercommit
  PRIVATE
    SNIFFERCOMMIT_VERSION="${PROJECT_VERSION}"
    SNIFFERCOMMIT_PLATFORM="${SNIFFERCOMMIT_PLATFORM}"
    SNIFFERCOMMIT_COMPILER="${SNIFFERCOMMIT_COMPILER}"
    $<$<CONFIG:Debug>:SNIFFERCOMMIT_DEBUG>
)

# Link libraries (only actual libraries here)
target_link_libraries(sniffercommit
  PRIVATE
    tomlplusplus::tomlplusplus
    fmt::fmt
)

# Compiler warnings & hardening
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(sniffercommit PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
    -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
    -Wunused -Woverloaded-virtual -Wnull-dereference
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
  )

  if(NOT WIN32)
    target_link_options(sniffercommit PRIVATE
      -Wl,-z,relro,-z,now
      $<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,-z,noexecstack>
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(sniffercommit PRIVATE -fstack-protector-strong)
    endif()
  endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(sniffercommit PRIVATE /W4 /permissive- /sdl)
  target_link_options(sniffercommit PRIVATE /DYNAMICBASE /NXCOMPAT)
endif()

# Sanitizers (Debug only)
if(SNIFFERCOMMIT_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Enabling AddressSanitizer + UndefinedBehaviorSanitizer")
    target_compile_options(sniffercommit PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(sniffercommit PRIVATE -fsanitize=address,undefined)
  endif()
endif()

# Static linking (FIXED: option name consistency)
if(SNIFFERCOMMIT_ENABLE_STATIC_LINK AND NOT WIN32)
  message(STATUS "📦 Enabling static linking for portable binary")
  set_target_properties(sniffercommit PROPERTIES
    LINK_SEARCH_START_STATIC ON
    LINK_SEARCH_END_STATIC ON
  )
endif()

# Install & CPack
include(GNUInstallDirs)

install(TARGETS sniffercommit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/sniffercommit/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sniffercommit
  FILES_MATCHING PATTERN "*.hpp"
)



# CPack / for debian packages to
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "sniffercommit")
set(CPACK_PACKAGE_VENDOR "slowy07")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")

set(CPACK_PACKAGE_CONTACT "slowy07")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "slowy07")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "git, bash")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

execute_process(
  COMMAND dpkg --print-architecture
  OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
include(CPack)

install(
  FILES
    README.md
    docs/docs.md
  DESTINATION
    ${CMAKE_INSTALL_DOCDIR}
)

install(
  FILES LICENSE
  DESTINATION
    ${CMAKE_INSTALL_DATADIR}/licenses/sniffercommit
)

# Verbose config summary
if(SNIFFERCOMMIT_VERBOSE_CONFIG)
  message(STATUS "\n"
    "  sniffercommit v${PROJECT_VERSION} - Config\n"
    "  Platform   : ${SNIFFERCOMMIT_PLATFORM}\n"
    "  Compiler   : ${SNIFFERCOMMIT_COMPILER}\n"
    "  Build Type : ${CMAKE_BUILD_TYPE}\n"
    "  Git Commit : ${SNIFFERCOMMIT_GIT_COMMIT}\n"
    "  Sanitizers : ${SNIFFERCOMMIT_ENABLE_SANITIZERS}\n"
    "  Static     : ${SNIFFERCOMMIT_ENABLE_STATIC_LINK}\n"
  )
endif()

