cmake_minimum_required(VERSION 3.21)
project(NotepadPlusPlusMac LANGUAGES CXX OBJCXX)

# Require Qt
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_AUTOMOC ON)

# App metadata
set(APP_VERSION "9.0.0-alpha")
set(APP_BUNDLE_NAME "Notepad++")
set(MACOS_BUNDLE_IDENTIFIER "org.notepadplusplus.notepadplusplus")
set(MINIMUM_SYSTEM_VERSION "${CMAKE_OSX_DEPLOYMENT_TARGET}")
string(TIMESTAMP CURRENT_YEAR "%Y")

# Configure Info.plist
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
               ${CMAKE_CURRENT_BINARY_DIR}/Info.plist @ONLY)

# App icon
set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/AppIcon.icns")

# Main application executable
add_executable(${APP_BUNDLE_NAME}
    MACOSX_BUNDLE
    main_qt.mm
    ${APP_ICON}
)

target_compile_options(${APP_BUNDLE_NAME}
    PRIVATE
        "-fobjc-arc"
)

target_include_directories(${APP_BUNDLE_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/PowerEditor/src
        ${CMAKE_SOURCE_DIR}/PowerEditor/src/UI
        ${CMAKE_SOURCE_DIR}/PowerEditor/src/Platform
        ${CMAKE_SOURCE_DIR}/PowerEditor/src/Core
        ${CMAKE_SOURCE_DIR}/scintilla/include
        ${CMAKE_SOURCE_DIR}/scintilla/qt/ScintillaEdit
        ${CMAKE_SOURCE_DIR}/lexilla/include
)

target_link_libraries(${APP_BUNDLE_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        scintilla
        lexilla
        nppcore
        "-framework Cocoa"
        "-framework Foundation"
        "-framework AppKit"
)

target_compile_definitions(${APP_BUNDLE_NAME}
    PRIVATE
        NPP_UI_WITH_QT=1
)

set_target_properties(${APP_BUNDLE_NAME} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
    RESOURCE "${APP_ICON}"
    OUTPUT_NAME "Notepad++"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++17"
)

install(TARGETS ${APP_BUNDLE_NAME}
        BUNDLE DESTINATION .)

message(STATUS "Notepad++ macOS: Configured bundle with Qt support")
