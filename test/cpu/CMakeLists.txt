add_subdirectory(alu)

add_executable(cpu_tests main.cpp cpu_test.cpp)
target_include_directories(cpu_tests PRIVATE ../../src)
# TODO put this in a library
target_sources(cpu_tests PRIVATE ../../src/cpu/cpux86.cpp)
target_sources(cpu_tests PRIVATE ../../src/bus/memory.cpp)
target_link_libraries(cpu_tests PRIVATE GTest::gtest_main GTest::gmock)
target_link_libraries(cpu_tests PRIVATE spdlog::spdlog)
