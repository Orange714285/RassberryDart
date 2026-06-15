cmake_minimum_required(VERSION 3.10)
project(Dart)

# 全局编译选项
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(
    -O3
    -march=armv8-a
    -Wall
    -Wextra
)

add_subdirectory(lib)
add_subdirectory(app)
