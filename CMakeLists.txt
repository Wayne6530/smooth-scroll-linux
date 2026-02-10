cmake_minimum_required(VERSION 3.14)

find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()

if("${GIT_DESCRIBE}" STREQUAL "")
  set(GIT_DESCRIBE "0.0.0-unknown")
endif()

string(REGEX REPLACE "^v" "" VERSION_FULL "${GIT_DESCRIBE}")

if(VERSION_FULL MATCHES "-")
  string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" VERSION_PURE "${VERSION_FULL}")
else()
  set(VERSION_PURE ${VERSION_FULL})
endif()

project(smooth_scroll VERSION ${VERSION_PURE})

configure_file(
  "${CMAKE_SOURCE_DIR}/src/version.h.in"
  "${CMAKE_BINARY_DIR}/generated/version.h"
)

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
target_include_directories(smooth-scroll
  PRIVATE
    ${tomlplusplus_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated
)
target_link_libraries(smooth-scroll fmt::fmt evdev)

install(TARGETS smooth-scroll DESTINATION /usr/bin)
install(FILES debian/smooth-scroll.service DESTINATION /usr/lib/systemd/system)
install(DIRECTORY config/ DESTINATION /etc/smooth-scroll)

set(CPACK_PACKAGE_NAME "smooth-scroll")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "Wayne <759858299@qq.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A lightweight tool that transforms your mouse wheel scrolling into a fluid, natural experience.")
set(CPACK_PACKAGE_RELOCATABLE "false")

set(CPACK_DEBIAN_PACKAGE_NAME "smooth-scroll")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libfmt9 (>= 9.0.0), libevdev2 (>= 1.10.0)")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
  "${CMAKE_SOURCE_DIR}/debian/postinst"
  "${CMAKE_SOURCE_DIR}/debian/prerm"
)

set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
set(CPACK_RPM_PACKAGE_REQUIRES "fmt >= 9.0.0, libevdev >= 1.10.0")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/rpm/postinst.sh")
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/rpm/prerm.sh")
set(CPACK_RPM_USER_FILELIST "%config(noreplace) /etc/smooth-scroll/smooth-scroll.toml")

include(CPack)
