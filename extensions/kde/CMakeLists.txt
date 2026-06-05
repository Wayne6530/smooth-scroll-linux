cmake_minimum_required(VERSION 3.25)

project(smooth_scroll_kwin_effect VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(KDE_INSTALL_QTPLUGINDIR "${CMAKE_INSTALL_LIBDIR}/qt6/plugins" CACHE PATH "Qt 6 plugin install directory" FORCE)
set(KDE_INSTALL_PLUGINDIR "${KDE_INSTALL_QTPLUGINDIR}" CACHE PATH "KDE plugin install directory" FORCE)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Quick Widgets)
find_package(KF6 REQUIRED COMPONENTS CoreAddons Config WindowSystem)
find_package(KWin REQUIRED)

set(KWIN_QT_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/qt6/plugins")

add_library(kwin_smooth_scroll MODULE
    src/ipc_client.cpp
    src/overlay_item.cpp
    src/smooth_scroll_effect.cpp
)

set_target_properties(kwin_smooth_scroll PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kwin/effects/plugins"
)

target_include_directories(kwin_smooth_scroll
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(kwin_smooth_scroll
    PRIVATE
        KWin::kwin
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Quick
        KF6::CoreAddons
        KF6::ConfigCore
        KF6::WindowSystem
)

install(TARGETS kwin_smooth_scroll
    LIBRARY DESTINATION "${KWIN_QT_PLUGIN_DIR}/kwin/effects/plugins"
)

install(PROGRAMS bin/smooth-scroll-kde-rule
    DESTINATION "${KDE_INSTALL_BINDIR}"
)
