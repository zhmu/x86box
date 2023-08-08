cmake_minimum_required(VERSION 3.18)
project(x86box)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(external/spdlog-1.11.0)
add_subdirectory(external/argparse-2.9)
add_subdirectory(external/googletest-1.14.0)

find_package(SDL2 REQUIRED)

if(ENABLE_CODE_COVERAGE)
enable_testing()
include(cmake/CodeCoverage.cmake)
append_coverage_compiler_flags()
endif()

add_subdirectory(src)
add_subdirectory(test)

if(ENABLE_CODE_COVERAGE)
setup_target_for_coverage_gcovr_xml(
        NAME coverage
        EXECUTABLE ctest
        EXCLUDE "external/*"
        DEPENDENCIES
            bus_tests
            cpu_tests
            hw_tests)
endif()
