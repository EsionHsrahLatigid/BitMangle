cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(BitMangle VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(BitMangle VERSION 0.1.0 LANGUAGES C CXX)
endif()

option(BITMANGLE_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(BITMANGLE_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(BITMANGLE_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(BITMANGLE_CREATE_AU OFF)
if(APPLE)
    set(BITMANGLE_CREATE_AU ON)
endif()
set(BITMANGLE_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")

add_library(bitmangle_engine STATIC
    source/BitMangleEngine.cpp)
target_include_directories(bitmangle_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(bitmangle_engine PUBLIC cxx_std_20)
set_target_properties(bitmangle_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(bitmangle_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BITMANGLE_BUILD_TESTS)
    include(CTest)

    function(enable_test_assertions target_name)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /UNDEBUG)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    add_executable(bitmangle_engine_tests
        tests/BitMangleEngineTests.cpp)
    target_link_libraries(bitmangle_engine_tests PRIVATE bitmangle_engine)
    target_compile_features(bitmangle_engine_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(bitmangle_engine_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    enable_test_assertions(bitmangle_engine_tests)
    add_test(NAME bitmangle_engine_tests COMMAND bitmangle_engine_tests)
endif()

if(BITMANGLE_BUILD_PLUGIN)
    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${BITMANGLE_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${BITMANGLE_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/EsionHsrahLatigid/yup.git
            GIT_TAG fa83e8c55664727ae5a53b94a3f6b5b336ce9e57)
        FetchContent_MakeAvailable(yup)
    endif()

    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/external/yup-ehl-design-module/CMakeLists.txt")
        message(FATAL_ERROR "Missing yup-ehl-design-module. Run: git submodule update --init")
    endif()
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/external/yup-ehl-design-module"
                     "${CMAKE_BINARY_DIR}/yup-ehl-design-module")

    # YUP's iconutil-based helper rejects its generated iconset on current
    # macOS. Keep the compatibility fix project-local until upstream changes.
    if(APPLE)
        include(cmake/YupMacOSIconWorkaround.cmake)
    endif()

    yup_audio_plugin(
        TARGET_NAME bitmangle
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "BitMangle"
        TARGET_APP_ID "jp.ehl.bitmangle"
        TARGET_APP_NAMESPACE "jp.ehl"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "jp.ehl.bitmangle"
        PLUGIN_NAME "BitMangle"
        PLUGIN_VENDOR "ehl_"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Deterministic quantizer and sample-rate reducer with dither and grid motion"
        PLUGIN_URL "https://github.com/EsionHsrahLatigid/BitMangle"
        PLUGIN_IS_SYNTH OFF
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${BITMANGLE_CREATE_AU}
        PLUGIN_AU_SUBTYPE "BtMn"
        PLUGIN_AU_MANUFACTURER "EHL1"
        PLUGIN_AU_SANDBOX_SAFE ON
        PLUGIN_CREATE_AUv3 OFF
        PLUGIN_CREATE_LV2 OFF
        PLUGIN_CREATE_AAX OFF
        PLUGIN_CREATE_STANDALONE ON
        PLUGIN_COPY_AFTER_BUILD OFF
        MODULES
            yup::yup_gui
            yup::yup_audio_processors)

    target_sources(bitmangle_shared INTERFACE
        source/ParameterGridEditor.cpp
        source/ParameterGridEditor.h
        source/ProductState.h
        source/BitManglePlugin.cpp
        source/BitManglePlugin.h)
    target_link_libraries(bitmangle_shared INTERFACE bitmangle_engine ehl::yup_plugin_ui)
    target_include_directories(bitmangle_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    if(BITMANGLE_BUILD_TESTS)
        add_executable(bitmangle_plugin_bridge_tests
            tests/BitManglePluginBridgeTests.cpp
            source/BitManglePlugin.cpp)
        target_link_libraries(bitmangle_plugin_bridge_tests PRIVATE bitmangle_engine yup::yup_audio_processors)
        target_include_directories(bitmangle_plugin_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(bitmangle_plugin_bridge_tests PRIVATE BITMANGLE_HEADLESS_TEST=1)
        target_compile_features(bitmangle_plugin_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(bitmangle_plugin_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(bitmangle_plugin_bridge_tests)
        add_test(NAME bitmangle_plugin_bridge_tests COMMAND bitmangle_plugin_bridge_tests)

        add_executable(bitmangle_standalone_bridge_tests
            tests/BitMangleStandaloneBridgeTests.cpp
            source/BitManglePlugin.cpp)
        target_link_libraries(bitmangle_standalone_bridge_tests PRIVATE bitmangle_engine yup::yup_audio_processors)
        target_include_directories(bitmangle_standalone_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(bitmangle_standalone_bridge_tests PRIVATE
            BITMANGLE_HEADLESS_TEST=1
            YUP_AUDIO_PLUGIN_ENABLE_STANDALONE=1)
        target_compile_features(bitmangle_standalone_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(bitmangle_standalone_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(bitmangle_standalone_bridge_tests)
        add_test(NAME bitmangle_standalone_bridge_tests COMMAND bitmangle_standalone_bridge_tests)
    endif()

    set(bitmangle_release_bundle_targets
        bitmangle_standalone_plugin
        bitmangle_vst3_plugin)
    if(APPLE)
        list(APPEND bitmangle_release_bundle_targets bitmangle_au_plugin)
    endif()

    add_custom_target(bitmangle_release_bundles
        DEPENDS ${bitmangle_release_bundle_targets})

    include(cmake/EhlYupArtifactLayout.cmake)
    ehl_yup_add_artifact_target(
        TARGET ehl_stage_products
        PRODUCT BitMangle
        SLUG bitmangle
        STANDALONE_TARGET bitmangle_standalone_plugin
        VST3_TARGET bitmangle_vst3_plugin
        AU_TARGET bitmangle_au_plugin)

    if(APPLE)
        set_target_properties(bitmangle_standalone_plugin PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "jp.ehl.bitmangle"
            MACOSX_BUNDLE_BUNDLE_NAME "BitMangle"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "jp.ehl.bitmangle")
    endif()

    if(APPLE AND BITMANGLE_ADHOC_SIGN)
        find_program(BITMANGLE_CODESIGN_EXECUTABLE codesign REQUIRED)
        add_custom_command(TARGET bitmangle_vst3_plugin POST_BUILD
            COMMAND "${BITMANGLE_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:bitmangle_vst3_plugin>"
            COMMENT "Ad-hoc signing BitMangle VST3 for local loading"
            VERBATIM)
        add_custom_command(TARGET bitmangle_standalone_plugin POST_BUILD
            COMMAND "${BITMANGLE_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:bitmangle_standalone_plugin>"
            COMMENT "Ad-hoc signing BitMangle standalone app for local launch"
            VERBATIM)
    endif()
endif()
