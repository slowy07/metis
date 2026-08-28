# tests/CMakeLists.txt

# Fetch Google Test
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    ON
    EXCLUDE_FROM_ALL
)

# Prevent overriding parent project's compiler/linker options
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Collect all test source files
set(TEST_SOURCES
  test_argparse.cpp
  test_binary_runtime.cpp
  test_config_generators.cpp
  test_config_manager.cpp
  test_dependency_checks.cpp
  test_format_mode.cpp
  test_glob_match.cpp
  test_perf_checks.cpp
  test_precommit_domain.cpp
  test_sanitizer_checks.cpp
  test_security_checks.cpp
  test_sha256_verification.cpp
  test_test_checks_use_case.cpp
  test_toolchain_install.cpp
  test_tooling_config.cpp
)

add_executable(metis_tests ${TEST_SOURCES})

# Link against the core library and Google Test
target_link_libraries(metis_tests
    PRIVATE
        metis_lib
        gtest_main
)

# Inherit the same C++ standard and flags (optional)
target_compile_features(metis_tests PRIVATE cxx_std_20)

# Make sure tests can find the generated config header
target_include_directories(metis_tests PRIVATE ${CMAKE_BINARY_DIR}/generated)

target_compile_definitions(metis_tests PRIVATE
    METIS_BINARY_PATH="${CMAKE_BINARY_DIR}/bin/metis"
)

# Discover tests with CTest
include(GoogleTest)
gtest_discover_tests(metis_tests
    PROPERTIES
        DISCOVERY_TIMEOUT 30
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

