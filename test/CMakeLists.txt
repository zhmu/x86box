add_executable(alu_tests alu_tests.cpp)
target_include_directories(alu_tests PRIVATE ../src)

add_subdirectory(bus)
add_subdirectory(hw)
