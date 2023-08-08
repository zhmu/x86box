add_executable(bus_tests main.cpp io_test.cpp memory_test.cpp)
target_include_directories(bus_tests PRIVATE ../../src)
# TODO put this in a library
target_sources(bus_tests PRIVATE ../../src/bus/io.cpp)
target_sources(bus_tests PRIVATE ../../src/bus/memory.cpp)
target_link_libraries(bus_tests PRIVATE GTest::gtest_main GTest::gmock)
target_link_libraries(bus_tests PRIVATE spdlog::spdlog argparse)

include(GoogleTest)
gtest_discover_tests(bus_tests)
