cmake_minimum_required(VERSION 3.0)

add_executable(format-columns main.cpp)

set_property(TARGET format-columns PROPERTY CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
