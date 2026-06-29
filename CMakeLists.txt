cmake_minimum_required(VERSION 3.14)
project(ghost-watch-project)
set(CMAKE_CXX_STANDARD 20)

find_package(SQLite3 REQUIRED)

include(FetchContent)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG v7.0.0
)
FetchContent_MakeAvailable(ftxui)

add_executable(ghost-watch src/main.cpp)
target_link_libraries(ghost-watch PRIVATE SQLite3::SQLite3)

add_executable(ghost-watch-tui src/dashboard.cpp)
target_link_libraries(ghost-watch-tui PRIVATE ftxui::ftxui SQLite3::SQLite3)
