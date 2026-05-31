cmake_minimum_required(VERSION 3.14)

project(smooth_scroll_cli LANGUAGES CXX)

include(GNUInstallDirs)

get_filename_component(SMOOTH_SCROLL_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)

add_library(smooth_scroll_cli_ipc INTERFACE)
target_include_directories(smooth_scroll_cli_ipc
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${SMOOTH_SCROLL_ROOT_DIR}/include"
)
target_link_libraries(smooth_scroll_cli_ipc INTERFACE Threads::Threads)

add_executable(ss-status src/ss_status.cpp)
target_link_libraries(ss-status PRIVATE smooth_scroll_cli_ipc)

add_executable(ss-stop src/ss_stop.cpp)
target_link_libraries(ss-stop PRIVATE smooth_scroll_cli_ipc)

add_executable(ss-passthrough src/ss_passthrough.cpp)
target_link_libraries(ss-passthrough PRIVATE smooth_scroll_cli_ipc)

install(
  TARGETS ss-status ss-stop ss-passthrough
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
  FILES
    "${SMOOTH_SCROLL_ROOT_DIR}/include/smooth_scroll/ipc_protocol.h"
    include/smooth_scroll/ipc_client.h
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/smooth_scroll"
)
