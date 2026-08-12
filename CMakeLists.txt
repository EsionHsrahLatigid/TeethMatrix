cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(TeethMatrix VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(TeethMatrix VERSION 0.1.0 LANGUAGES C CXX)
endif()

option(TEETHMATRIX_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(TEETHMATRIX_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(TEETHMATRIX_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(TEETHMATRIX_CREATE_AU OFF)
if(APPLE)
    set(TEETHMATRIX_CREATE_AU ON)
endif()
set(TEETHMATRIX_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")

add_library(teethmatrix_engine STATIC
    source/TeethMatrixEngine.cpp)
target_include_directories(teethmatrix_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(teethmatrix_engine PUBLIC cxx_std_20)
set_target_properties(teethmatrix_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(teethmatrix_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(TEETHMATRIX_BUILD_TESTS)
    include(CTest)

    function(enable_test_assertions target_name)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /UNDEBUG)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    add_executable(teethmatrix_engine_tests
        tests/TeethMatrixEngineTests.cpp)
    target_link_libraries(teethmatrix_engine_tests PRIVATE teethmatrix_engine)
    target_compile_features(teethmatrix_engine_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(teethmatrix_engine_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    enable_test_assertions(teethmatrix_engine_tests)
    add_test(NAME teethmatrix_engine_tests COMMAND teethmatrix_engine_tests)
endif()

if(TEETHMATRIX_BUILD_PLUGIN)
    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${TEETHMATRIX_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${TEETHMATRIX_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/kunitoki/yup.git
            GIT_TAG 9a1c9bc699b6a714f6f52486462d98a140c8bf95)
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
        TARGET_NAME teethmatrix
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "TeethMatrix"
        TARGET_APP_ID "jp.ehl.teethmatrix"
        TARGET_APP_NAMESPACE "jp.ehl"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "jp.ehl.teethmatrix"
        PLUGIN_NAME "TeethMatrix"
        PLUGIN_VENDOR "ehl_"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Fractional feedback comb matrix with dispersion and motion"
        PLUGIN_URL "https://github.com/EsionHsrahLatigid/TeethMatrix"
        PLUGIN_IS_SYNTH OFF
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${TEETHMATRIX_CREATE_AU}
        PLUGIN_AU_SUBTYPE "TtMx"
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

    target_sources(teethmatrix_shared INTERFACE
        source/ParameterGridEditor.cpp
        source/ParameterGridEditor.h
        source/ProductState.h
        source/TeethMatrixPlugin.cpp
        source/TeethMatrixPlugin.h)
    target_link_libraries(teethmatrix_shared INTERFACE teethmatrix_engine ehl::yup_plugin_ui)
    target_include_directories(teethmatrix_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    if(TEETHMATRIX_BUILD_TESTS)
        add_executable(teethmatrix_plugin_bridge_tests
            tests/TeethMatrixPluginBridgeTests.cpp
            source/TeethMatrixPlugin.cpp)
        target_link_libraries(teethmatrix_plugin_bridge_tests PRIVATE teethmatrix_engine yup::yup_audio_processors)
        target_include_directories(teethmatrix_plugin_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(teethmatrix_plugin_bridge_tests PRIVATE TEETHMATRIX_HEADLESS_TEST=1)
        target_compile_features(teethmatrix_plugin_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(teethmatrix_plugin_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(teethmatrix_plugin_bridge_tests)
        add_test(NAME teethmatrix_plugin_bridge_tests COMMAND teethmatrix_plugin_bridge_tests)

        add_executable(teethmatrix_standalone_bridge_tests
            tests/TeethMatrixStandaloneBridgeTests.cpp
            source/TeethMatrixPlugin.cpp)
        target_link_libraries(teethmatrix_standalone_bridge_tests PRIVATE teethmatrix_engine yup::yup_audio_processors)
        target_include_directories(teethmatrix_standalone_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(teethmatrix_standalone_bridge_tests PRIVATE
            TEETHMATRIX_HEADLESS_TEST=1
            YUP_AUDIO_PLUGIN_ENABLE_STANDALONE=1)
        target_compile_features(teethmatrix_standalone_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(teethmatrix_standalone_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(teethmatrix_standalone_bridge_tests)
        add_test(NAME teethmatrix_standalone_bridge_tests COMMAND teethmatrix_standalone_bridge_tests)
    endif()

    set(teethmatrix_release_bundle_targets
        teethmatrix_standalone_plugin
        teethmatrix_vst3_plugin)
    if(APPLE)
        list(APPEND teethmatrix_release_bundle_targets teethmatrix_au_plugin)
    endif()

    add_custom_target(teethmatrix_release_bundles
        DEPENDS ${teethmatrix_release_bundle_targets})

    include(cmake/EhlYupArtifactLayout.cmake)
    ehl_yup_add_artifact_target(
        TARGET ehl_stage_products
        PRODUCT TeethMatrix
        SLUG teethmatrix
        STANDALONE_TARGET teethmatrix_standalone_plugin
        VST3_TARGET teethmatrix_vst3_plugin
        AU_TARGET teethmatrix_au_plugin)

    if(APPLE)
        set_target_properties(teethmatrix_standalone_plugin PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "jp.ehl.teethmatrix"
            MACOSX_BUNDLE_BUNDLE_NAME "TeethMatrix"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "jp.ehl.teethmatrix")
    endif()

    if(APPLE AND TEETHMATRIX_ADHOC_SIGN)
        find_program(TEETHMATRIX_CODESIGN_EXECUTABLE codesign REQUIRED)
        add_custom_command(TARGET teethmatrix_vst3_plugin POST_BUILD
            COMMAND "${TEETHMATRIX_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:teethmatrix_vst3_plugin>"
            COMMENT "Ad-hoc signing TeethMatrix VST3 for local loading"
            VERBATIM)
        add_custom_command(TARGET teethmatrix_standalone_plugin POST_BUILD
            COMMAND "${TEETHMATRIX_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:teethmatrix_standalone_plugin>"
            COMMENT "Ad-hoc signing TeethMatrix standalone app for local launch"
            VERBATIM)
    endif()
endif()
