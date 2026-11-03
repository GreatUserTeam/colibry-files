cmake_minimum_required(VERSION 3.21)

project(colibry-file
    VERSION 0.2.0
    DESCRIPTION "Files for the Colibry"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(Colibry_BUILD_TESTS "Build unit tests" ON)
option(Colibry_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(_qt_components Widgets)
if(Colibry_BUILD_TESTS)
    list(APPEND _qt_components Test)
endif()
find_package(Qt6 6.4 REQUIRED COMPONENTS ${_qt_components})

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(ColibryApps)

if(Colibry_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(common)
add_library(gus_fm_core STATIC
    src/core/FileOperations.h
    src/core/FileOperations.cpp
    src/core/NavigationHistory.h
    src/core/NavigationHistory.cpp
    src/core/FileClipboard.h
    src/core/FileClipboard.cpp
)
target_include_directories(gus_fm_core PUBLIC src)
target_link_libraries(gus_fm_core PUBLIC Qt6::Widgets)
colibry_target_defaults(gus_fm_core)

add_library(gus_fm_ui STATIC
    src/FileIcons.h
    src/FileIcons.cpp
    src/Launcher.h
    src/Launcher.cpp
    src/PropertiesDialog.h
    src/PropertiesDialog.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)
target_include_directories(gus_fm_ui PUBLIC src)
target_link_libraries(gus_fm_ui PUBLIC gus_fm_core colibry_common Qt6::Widgets)
colibry_target_defaults(gus_fm_ui)

add_executable(colibry-files src/main.cpp)
target_link_libraries(colibry-files PRIVATE gus_fm_ui)
colibry_target_defaults(colibry-files)

colibry_install_app(colibry-files
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/colibry-files.desktop
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/colibry-files.svg)

if(Colibry_BUILD_TESTS)
    colibry_add_test(fm_fileoperations SOURCES tests/test_fileoperations.cpp LINK gus_fm_core)
    colibry_add_test(fm_navigation SOURCES tests/test_navigation.cpp LINK gus_fm_core)
    colibry_add_test(fm_ui SOURCES tests/test_ui.cpp LINK gus_fm_ui)
endif()
