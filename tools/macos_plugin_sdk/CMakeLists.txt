cmake_minimum_required(VERSION 3.22)
project(NotepadPlusPlusMacPluginSDK LANGUAGES CXX)

option(NPP_MAC_SDK_BUILD_EXAMPLE "Build the HelloMacPlugin sample" ON)

set(NPP_MAC_SDK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(NPP_MAC_SDK_BUILD_EXAMPLE)
    add_library(HelloMacPlugin MODULE
        src/HelloPlugin.cpp
    )
    target_include_directories(HelloMacPlugin PRIVATE
        ${NPP_MAC_SDK_INCLUDE_DIR}
    )
    target_compile_features(HelloMacPlugin PRIVATE cxx_std_20)
    if(APPLE)
        target_link_libraries(HelloMacPlugin PRIVATE "-framework AppKit")
        set_target_properties(HelloMacPlugin PROPERTIES
            PREFIX ""
            SUFFIX ".nppplugin"
        )
    else()
        set_target_properties(HelloMacPlugin PROPERTIES PREFIX "")
    endif()

    install(TARGETS HelloMacPlugin
        RUNTIME DESTINATION HelloMacPlugin
        LIBRARY DESTINATION HelloMacPlugin
        ARCHIVE DESTINATION HelloMacPlugin
    )
endif()

install(DIRECTORY ${NPP_MAC_SDK_INCLUDE_DIR}/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")

include(CPack)
set(CPACK_PACKAGE_NAME "NotepadPlusPlusMacPluginSDK")
set(CPACK_PACKAGE_VENDOR "Notepad++ Team")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_GENERATOR "TGZ")
