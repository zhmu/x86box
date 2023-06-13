cmake_minimum_required(VERSION 3.18)
project(x86box)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(external/spdlog-1.11.0)

find_package(SDL2 REQUIRED)

add_subdirectory(src)
add_subdirectory(test)
