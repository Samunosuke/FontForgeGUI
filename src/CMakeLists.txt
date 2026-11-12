cmake_minimum_required(VERSION 3.20)

project(FontForgeGUI VERSION 0.5.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Path to the external FontForge repository (sibling ../FontForge by default)
set(_default_fontforge_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../FontForge")
if(DEFINED ENV{FONTFORGE_SOURCE_DIR} AND NOT DEFINED FONTFORGE_SOURCE_DIR)
    set(_default_fontforge_dir "$ENV{FONTFORGE_SOURCE_DIR}")
endif()
set(FONTFORGE_SOURCE_DIR "${_default_fontforge_dir}" CACHE PATH "Path to FontForge source repository")

if(NOT EXISTS "${FONTFORGE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "FontForge source directory not found at '${FONTFORGE_SOURCE_DIR}'. Pass -DFONTFORGE_SOURCE_DIR=/path/to/FontForge.")
endif()

# Configure FontForge as a headless shared core library (no GTK/GDraw/X11)
set(ENABLE_GUI OFF CACHE BOOL "Build FontForge without legacy GUI" FORCE)
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build libfontforge as shared library" FORCE)
set(ENABLE_DOCS OFF CACHE BOOL "Disable FontForge Sphinx docs" FORCE)
set(ENABLE_PYTHON_SCRIPTING OFF CACHE BOOL "Disable FontForge Python scripting" FORCE)
set(ENABLE_PYTHON_EXTENSION OFF CACHE BOOL "Disable FontForge Python extension" FORCE)
set(ENABLE_NATIVE_SCRIPTING OFF CACHE BOOL "Disable FontForge legacy scripting" FORCE)

add_subdirectory("${FONTFORGE_SOURCE_DIR}" fontforge-build EXCLUDE_FROM_ALL)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Xml Svg)

# Ensure binaries find libfontforge.so in the build tree and relative install lib dir
set(CMAKE_BUILD_RPATH "${CMAKE_CURRENT_BINARY_DIR}/fontforge-build/lib")
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

add_library(ffgui_core STATIC
    backend/FontTypes.hpp
    backend/FontForgeBridge.hpp
    backend/FontForgeBridge.cpp
    backend/FontWorker.hpp
    backend/FontWorker.cpp
    model/UnicodeRangeFilter.hpp
    model/UnicodeRangeFilter.cpp
    model/CustomGlyphAreaConfig.hpp
    model/CustomGlyphAreaConfig.cpp
    model/GlyphListModel.hpp
    model/GlyphListModel.cpp
    model/GlyphMetadataUtils.hpp
    model/GlyphMetadataUtils.cpp
    model/SvgSanitizer.hpp
    model/SvgSanitizer.cpp
    model/BitmapTracer.hpp
    model/BitmapTracer.cpp
    model/BatchImportDefinition.hpp
    model/BatchImportDefinition.cpp
    model/FontSettingsUtils.hpp
    model/GlyphClipboardUtils.hpp
)

target_include_directories(ffgui_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(ffgui_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Svg
    PRIVATE
        Qt6::Xml
        fontforge
)

target_compile_features(ffgui_core PUBLIC cxx_std_20)

add_executable(FontForgeGUI WIN32
    main.cpp
    ui/MainWindow.hpp
    ui/MainWindow.cpp
    ui/GlyphGridDelegate.hpp
    ui/GlyphGridDelegate.cpp
    ui/GlyphGridView.hpp
    ui/GlyphGridView.cpp
    ui/GlyphInspectorPanel.hpp
    ui/GlyphInspectorPanel.cpp
    ui/CreateGlyphDialog.hpp
    ui/CreateGlyphDialog.cpp
    ui/ImportSvgDialog.hpp
    ui/ImportSvgDialog.cpp
    ui/ExportFontDialog.hpp
    ui/ExportFontDialog.cpp
    ui/FontMetadataDialog.hpp
    ui/FontMetadataDialog.cpp
    ui/ValidationPanel.hpp
    ui/ValidationPanel.cpp
    ui/NewFontDialog.hpp
    ui/NewFontDialog.cpp
    ui/MoveGlyphsDialog.hpp
    ui/MoveGlyphsDialog.cpp
    ui/BatchImportDialog.hpp
    ui/BatchImportDialog.cpp
    ui/ImportFilterSettingsDialog.hpp
    ui/ImportFilterSettingsDialog.cpp
    ui/ImportFontDialog.hpp
    ui/ImportFontDialog.cpp
)

target_link_libraries(FontForgeGUI
    PRIVATE
        ffgui_core
        Qt6::Widgets
        Qt6::Svg
)

target_compile_features(FontForgeGUI PRIVATE cxx_std_20)

if(WIN32)
    add_custom_command(TARGET FontForgeGUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:fontforge>"
            "$<TARGET_FILE_DIR:FontForgeGUI>"
        COMMENT "Copying libfontforge DLL to build directory"
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_backend_smoke tests/test_backend_smoke.cpp)
    target_link_libraries(test_backend_smoke PRIVATE ffgui_core)
    target_compile_definitions(test_backend_smoke PRIVATE
        TEST_FONTS_DIR="${FONTFORGE_SOURCE_DIR}/tests/fonts"
    )
    add_test(NAME backend_smoke COMMAND test_backend_smoke)

    add_executable(test_unicode_range tests/test_unicode_range.cpp)
    target_link_libraries(test_unicode_range PRIVATE ffgui_core)
    add_test(NAME unicode_range COMMAND test_unicode_range)

    add_executable(test_v02_metadata_validation tests/test_v02_metadata_validation.cpp)
    target_link_libraries(test_v02_metadata_validation PRIVATE ffgui_core)
    target_compile_definitions(test_v02_metadata_validation PRIVATE
        TEST_FONTS_DIR="${FONTFORGE_SOURCE_DIR}/tests/fonts"
    )
    add_test(NAME v02_metadata_validation COMMAND test_v02_metadata_validation)

    add_executable(test_v03_svg_import tests/test_v03_svg_import.cpp)
    target_link_libraries(test_v03_svg_import PRIVATE ffgui_core)
    target_compile_definitions(test_v03_svg_import PRIVATE
        TEST_FONTS_DIR="${FONTFORGE_SOURCE_DIR}/tests/fonts"
    )
    add_test(NAME v03_svg_import COMMAND test_v03_svg_import)

    add_executable(test_v04_features tests/test_v04_features.cpp)
    target_link_libraries(test_v04_features PRIVATE ffgui_core)
    target_compile_definitions(test_v04_features PRIVATE
        TEST_FONTS_DIR="${FONTFORGE_SOURCE_DIR}/tests/fonts"
    )
    add_test(NAME v04_features COMMAND test_v04_features)

    add_executable(test_v05_features tests/test_v05_features.cpp)
    target_link_libraries(test_v05_features PRIVATE ffgui_core)
    target_compile_definitions(test_v05_features PRIVATE
        TEST_FONTS_DIR="${FONTFORGE_SOURCE_DIR}/tests/fonts"
    )
    add_test(NAME v05_features COMMAND test_v05_features)
endif()
