cmake_minimum_required(VERSION 3.14)
project(smooth_scroll)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

find_package(fmt REQUIRED)

file(GLOB SOURCES "*.cpp")
add_executable(smooth-scroll ${SOURCES})
target_compile_definitions(smooth-scroll PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
target_include_directories(smooth-scroll PRIVATE ${tomlplusplus_SOURCE_DIR}/include)
target_link_libraries(smooth-scroll fmt::fmt evdev)
