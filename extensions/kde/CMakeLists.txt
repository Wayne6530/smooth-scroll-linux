cmake_minimum_required(VERSION 3.25)

project(smooth_scroll_kwin_effect VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(QT_MAJOR_VERSION 6)

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
find_package(KF6 REQUIRED COMPONENTS CoreAddons Config KCMUtils WindowSystem)
find_package(KWin REQUIRED)

if(KWin_VERSION VERSION_GREATER_EQUAL "6.7.0")
    set(SMOOTH_SCROLL_KWIN_6_7_OR_NEWER 1)
else()
    set(SMOOTH_SCROLL_KWIN_6_7_OR_NEWER 0)
endif()

set(KWIN_QT_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/qt6/plugins")

add_library(smooth_scroll_settings STATIC src/effect_settings.cpp)
set_target_properties(smooth_scroll_settings PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(smooth_scroll_settings PUBLIC KF6::ConfigCore Qt6::Core)

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

target_compile_definitions(kwin_smooth_scroll
    PRIVATE
        SMOOTH_SCROLL_KWIN_6_7_OR_NEWER=${SMOOTH_SCROLL_KWIN_6_7_OR_NEWER}
)

target_link_libraries(kwin_smooth_scroll
    PRIVATE
        smooth_scroll_settings
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

add_library(kwin_smooth_scroll_config MODULE src/config_module.cpp src/rules_page.cpp)
set_target_properties(kwin_smooth_scroll_config PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/kwin/effects/configs"
)
install(TARGETS kwin_smooth_scroll_config
    LIBRARY DESTINATION "${KWIN_QT_PLUGIN_DIR}/kwin/effects/configs"
)
target_link_libraries(kwin_smooth_scroll_config PRIVATE smooth_scroll_settings
    KF6::KCMUtils Qt6::Widgets Qt6::DBus)

include(CTest)
if(BUILD_TESTING)
    add_executable(ipc_client_test tests/ipc_client_test.cpp src/ipc_client.cpp)
    target_include_directories(ipc_client_test PRIVATE src ../../include)
    add_test(NAME kde_ipc_client COMMAND ipc_client_test)

    add_executable(effect_settings_test tests/effect_settings_test.cpp)
    target_include_directories(effect_settings_test PRIVATE src)
    target_link_libraries(effect_settings_test PRIVATE smooth_scroll_settings)
    add_test(NAME kde_effect_settings COMMAND effect_settings_test)

    add_executable(config_module_test tests/config_module_test.cpp)
    target_include_directories(config_module_test PRIVATE src)
    target_link_libraries(config_module_test PRIVATE smooth_scroll_settings KF6::KCMUtils Qt6::Widgets)
    add_test(NAME kde_config_module COMMAND config_module_test $<TARGET_FILE:kwin_smooth_scroll_config>)
    set_tests_properties(kde_config_module PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/smooth-scroll-test-bus")

    add_executable(shell_profiles_test tests/shell_profiles_test.cpp)
    target_include_directories(shell_profiles_test PRIVATE src)
    target_link_libraries(shell_profiles_test PRIVATE Qt6::Core)
    add_test(NAME kde_shell_profiles COMMAND shell_profiles_test)
endif()
