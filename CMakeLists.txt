cmake_minimum_required(VERSION 3.10)
project(ghost-watch)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include external)

add_executable(ghost-watch src/main.cpp)
