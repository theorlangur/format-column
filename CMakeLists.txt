cmake_minimum_required(VERSION 3.0)

add_executable(format-columns main.cpp)

target_compile_options(format-columns PUBLIC -std=c++17)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
