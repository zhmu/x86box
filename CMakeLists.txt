cmake_minimum_required(VERSION 3.18)
project(x86box)

find_package(SDL REQUIRED)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(src)
