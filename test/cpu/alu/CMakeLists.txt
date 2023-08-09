add_executable(alu_tests alu_tests.cpp)
target_include_directories(alu_tests PRIVATE ../../../src)
target_link_libraries(alu_tests PRIVATE GTest::gtest_main GTest::gmock)

include(GoogleTest)
gtest_discover_tests(alu_tests)
