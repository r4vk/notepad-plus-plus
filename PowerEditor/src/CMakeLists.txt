# PowerEditor Core Library for macOS - Minimal Build
cmake_minimum_required(VERSION 3.21)

project(NotepadPlusPlusCore LANGUAGES CXX OBJCXX)

# Find only files that actually exist
file(GLOB UI_SOURCES UI/*.cpp)
file(GLOB PLATFORM_SOURCES Platform/*.cpp Platform/*.mm)

# Exclude problematic files on macOS
list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "FileChangePoller\\.cpp$")  # pimpl issues
list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "_Windows\\.cpp$")  # Windows-only files
list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "PreferencesMigrator\\.cpp$")  # Uses Windows API

# Manually verified core files
# Parameters.cpp excluded - requires tinyxmlA.h which is not in build
set(MINIMAL_CORE_SOURCES
    # Parameters.cpp
)

# Combine all sources
set(ALL_SOURCES
    ${UI_SOURCES}
    ${PLATFORM_SOURCES}
    ${MINIMAL_CORE_SOURCES}
)

# Create core library
add_library(nppcore STATIC ${ALL_SOURCES})

target_include_directories(nppcore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Platform
        ${CMAKE_CURRENT_SOURCE_DIR}/UI
        ${CMAKE_CURRENT_SOURCE_DIR}/Core
        ${CMAKE_CURRENT_SOURCE_DIR}/ScintillaComponent
        ${CMAKE_CURRENT_SOURCE_DIR}/MISC/Common
        ${CMAKE_SOURCE_DIR}/scintilla/include
        ${CMAKE_SOURCE_DIR}/scintilla/src
        ${CMAKE_SOURCE_DIR}/scintilla/qt/ScintillaEdit
        ${CMAKE_SOURCE_DIR}/scintilla/qt/ScintillaEditBase
        ${CMAKE_SOURCE_DIR}/lexilla/include
)

target_link_libraries(nppcore
    PUBLIC
        scintilla
        lexilla
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        ${COCOA_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
)

target_compile_definitions(nppcore
    PUBLIC
        UNICODE
        _UNICODE
        NPP_UI_WITH_QT=1
)

target_compile_features(nppcore PUBLIC cxx_std_17)

list(LENGTH ALL_SOURCES SOURCE_COUNT)
message(STATUS "PowerEditor Core: Building with ${SOURCE_COUNT} source files")
