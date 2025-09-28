cmake_minimum_required(VERSION 3.21)
project(NotepadPlusPlusMacPreview LANGUAGES OBJCXX)

# Enforce a modern deployment target so that we can rely on current Cocoa behavior
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version supported by the preview bundle")

set(APP_VERSION "0.1.0-preview")
set(APP_BUNDLE_NAME "NotepadPlusPlusPreview")
set(MACOS_BUNDLE_IDENTIFIER "org.notepadplusplus.preview")
set(MINIMUM_SYSTEM_VERSION "${CMAKE_OSX_DEPLOYMENT_TARGET}")
string(TIMESTAMP CURRENT_YEAR "%Y")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in ${CMAKE_BINARY_DIR}/Info.plist @ONLY)

add_executable(${APP_BUNDLE_NAME}
    MACOSX_BUNDLE
    main.mm
)

target_compile_options(${APP_BUNDLE_NAME}
    PRIVATE
        "-fobjc-arc"
)

target_link_libraries(${APP_BUNDLE_NAME}
    PRIVATE
        "-framework Cocoa"
)

# Ensure ARC is enabled regardless of generator
set_target_properties(${APP_BUNDLE_NAME} PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist"
    OUTPUT_NAME "NotepadPlusPlusPreview"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
)

install(TARGETS ${APP_BUNDLE_NAME}
        BUNDLE DESTINATION .
)
