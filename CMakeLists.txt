cmake_minimum_required(VERSION 3.14)
project(smooth_scroll VERSION 0.1.0)

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

file(GLOB SOURCES "src/*.cpp")
add_executable(smooth-scroll ${SOURCES})
target_compile_definitions(smooth-scroll PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
target_include_directories(smooth-scroll PRIVATE ${tomlplusplus_SOURCE_DIR}/include)
target_link_libraries(smooth-scroll fmt::fmt evdev)

install(TARGETS smooth-scroll DESTINATION bin)
install(FILES debian/smooth-scroll.service DESTINATION /etc/systemd/system)
install(DIRECTORY config/ DESTINATION /etc/smooth-scroll)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "smooth-scroll")
set(CPACK_DEBIAN_PACKAGE_NAME "smooth-scroll")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Wayne <759858299@qq.com>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "A lightweight tool that transforms your mouse wheel scrolling into a fluid, natural experience.")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libfmt9 (>= 9.0.0), libevdev2 (>= 1.10.0)")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
  "${CMAKE_SOURCE_DIR}/debian/postinst"
  "${CMAKE_SOURCE_DIR}/debian/prerm"
)

include(CPack)
