cmake_minimum_required(VERSION 3.16)
project(NotepadPP_UnitTests LANGUAGES CXX)

get_filename_component(NPP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(POWEREDITOR_SRC ${NPP_ROOT}/PowerEditor/src)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(npp_platform STATIC
    ${POWEREDITOR_SRC}/Platform/FileSystem.cpp
    ${POWEREDITOR_SRC}/Platform/FileChangePoller.cpp
    ${POWEREDITOR_SRC}/Platform/PreferencesMigrator.cpp
    ${POWEREDITOR_SRC}/Platform/PreferencesSync.cpp
    ${POWEREDITOR_SRC}/Platform/PathProvider.cpp
    ${POWEREDITOR_SRC}/Platform/SettingsLocator.cpp
    ${POWEREDITOR_SRC}/Platform/SystemInfo.cpp
    ${POWEREDITOR_SRC}/Platform/SystemServices.cpp
    ${POWEREDITOR_SRC}/Platform/SystemServices_Windows.cpp
)

target_include_directories(npp_platform
    PUBLIC
        ${POWEREDITOR_SRC}
)

if(WIN32)
    target_link_libraries(npp_platform
        PUBLIC
            shell32
            advapi32
            gdi32
            user32
    )
endif()

enable_testing()

add_executable(npp_platform_tests
    platform/test_path_provider.cpp
    platform/test_settings_locator.cpp
    platform/test_file_system.cpp
    platform/test_system_info.cpp
    platform/test_file_change_poller.cpp
    platform/test_system_services.cpp
    platform/test_preferences_store.cpp
    platform/test_preferences_sync.cpp
    platform/test_system_appearance.cpp
    platform/test_preferences_migrator.cpp
)

target_link_libraries(npp_platform_tests
    PRIVATE
        npp_platform
)

target_include_directories(npp_platform_tests
    PRIVATE
        ${POWEREDITOR_SRC}
        ${NPP_ROOT}/scintilla/test/unit
        ${NPP_ROOT}/lexilla/test/unit
)

add_test(NAME npp_platform_tests COMMAND npp_platform_tests)

add_library(npp_ui STATIC
    ${POWEREDITOR_SRC}/UI/Factory.cpp
    ${POWEREDITOR_SRC}/UI/DockingLayout.cpp
    ${POWEREDITOR_SRC}/UI/ScintillaTheme.cpp
    ${POWEREDITOR_SRC}/UI/ShortcutNormalization.cpp
)

target_include_directories(npp_ui
    PUBLIC
        ${POWEREDITOR_SRC}
)

add_executable(npp_ui_tests
    ui/catch_main.cpp
    ui/test_ui_factory.cpp
    ui/test_scintilla_theme.cpp
    ui/test_shortcut_normalization.cpp
    ui/test_docking_layout.cpp
    ui/test_ui_qt_backend.cpp
)

target_link_libraries(npp_ui_tests
    PRIVATE
        npp_ui
)

target_include_directories(npp_ui_tests
    PRIVATE
        ${POWEREDITOR_SRC}
)

add_test(NAME npp_ui_tests COMMAND npp_ui_tests)
