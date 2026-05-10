cmake_minimum_required(VERSION 3.20)
project(sniffercommit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        11.0.2
)
FetchContent_MakeAvailable(fmt)

add_executable(sniffercommit
  src/main.cpp
  src/config.cpp
  src/generator.cpp
  src/installer.cpp
)

target_include_directories(sniffercommit PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_features(sniffercommit PUBLIC cxx_std_20)
target_link_libraries(sniffercommit PRIVATE tomlplusplus::tomlplusplus fmt::fmt)

install(FILES
  templates/pre-commit.sh.in
  templates/github-actions.yml.in
  DESTINATION share/sniffercommit/templates
)
