cmake_minimum_required(VERSION 3.8)
project(smooth_scroll)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(fmt REQUIRED)

file(GLOB SOURCES "*.cpp")
add_executable(smooth-scroll ${SOURCES})
target_link_libraries(smooth-scroll fmt::fmt evdev)
