cmake_minimum_required(VERSION 3.16)
project(PenTool VERSION 1.0.0 LANGUAGES CXX)

# =============================================================================
# User-facing cache variables
# =============================================================================
set(NUKE_ROOT "" CACHE PATH
    "Nuke installation directory, e.g. C:/Program Files/Nuke17.0v1")

# =============================================================================
# Auto-detect NUKE_ROOT for Nuke 17 on Windows
# =============================================================================
if(NOT NUKE_ROOT)
    if(WIN32)
        foreach(_candidate
                "C:/Program Files/Nuke17.1v1"
                "C:/Program Files/Nuke17.0v3"
                "C:/Program Files/Nuke17.0v2"
                "C:/Program Files/Nuke17.0v1"
                "C:/Program Files/Nuke16.0v5"
                "D:/Nuke17.0v1")
            if(EXISTS "${_candidate}")
                set(NUKE_ROOT "${_candidate}")
                break()
            endif()
        endforeach()
    elseif(APPLE)
        foreach(_candidate
                "/Applications/Nuke17.0v1/Nuke17.0v1.app/Contents/MacOS"
                "/Applications/Nuke17.1v1/Nuke17.1v1.app/Contents/MacOS")
            if(EXISTS "${_candidate}")
                set(NUKE_ROOT "${_candidate}")
                break()
            endif()
        endforeach()
    else()
        foreach(_candidate
                "/usr/local/Nuke17.0v1"
                "/usr/local/Nuke17.1v1"
                "/opt/Nuke17.0v1")
            if(EXISTS "${_candidate}")
                set(NUKE_ROOT "${_candidate}")
                break()
            endif()
        endforeach()
    endif()

    if(NOT NUKE_ROOT)
        message(FATAL_ERROR
            "Could not auto-detect NUKE_ROOT.\n"
            "Pass it explicitly:\n"
            "  cmake -DNUKE_ROOT=\"C:/Program Files/Nuke17.0v1\" ..")
    endif()
endif()

message(STATUS "NUKE_ROOT : ${NUKE_ROOT}")

# =============================================================================
# C++ standard
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# MSVC toolset + runtime check
#
# Nuke 17 is compiled with MSVC 2019 (toolset v142).
# VS 2022 (v143) is generally ABI-compatible for Release builds.
#
# CRITICAL: The plugin MUST link the multi-threaded DLL CRT (/MD, not /MT).
# A mismatched CRT causes crashes the instant any STL type (std::string,
# vector, etc.) crosses the plugin/host boundary.
# =============================================================================
if(MSVC)
    message(STATUS "MSVC version : ${MSVC_VERSION}  toolset v${MSVC_TOOLSET_VERSION}")

    if(MSVC_TOOLSET_VERSION LESS 140)
        message(FATAL_ERROR "Nuke 17 NDK requires MSVC 2015 (v140) or newer.")
    endif()

    # Enforce /MD (dynamic CRT) in every configuration
    foreach(_flag_var
            CMAKE_CXX_FLAGS
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS_MINSIZEREL)
        string(REGEX REPLACE "/MT([d]?)" "/MD\\1" ${_flag_var} "${${_flag_var}}")
    endforeach()
endif()

# =============================================================================
# Qt
#
# Nuke 15 / 16  →  Qt 5.15
# Nuke 17+      →  Qt 6.x  (Foundry switched in Nuke 17)
#
# The Qt cmake config files live inside the Nuke install tree.
# Expected location (Nuke 17):
#   <NUKE_ROOT>/lib/cmake/Qt6/Qt6Config.cmake
# =============================================================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---------------------------------------------------------------------------
# Qt auto-detection
#
# Nuke 17 ships Qt6 runtime DLLs but NOT the SDK headers/import libs.
# We look for a Qt6 MSVC 2019 x64 SDK in standard install locations.
# Override by passing:  -DQt6_DIR="C:/Qt/6.5.3/msvc2019_64/lib/cmake/Qt6"
# ---------------------------------------------------------------------------
if(NOT Qt6_DIR)
    foreach(_base "C:/Qt" "D:/Qt" "$ENV{USERPROFILE}/Qt")
        if(NOT EXISTS "${_base}")
            continue()
        endif()
        # Collect all 6.x version dirs, sort highest first
        file(GLOB _vers LIST_DIRECTORIES true "${_base}/6.*")
        list(SORT _vers ORDER DESCENDING)
        foreach(_ver ${_vers})
            set(_candidate "${_ver}/msvc2019_64/lib/cmake/Qt6")
            if(EXISTS "${_candidate}/Qt6Config.cmake")
                set(Qt6_DIR "${_candidate}" CACHE PATH "Qt6 cmake dir" FORCE)
                break()
            endif()
        endforeach()
        if(Qt6_DIR)
            break()
        endif()
    endforeach()
endif()

if(NOT Qt6_DIR OR NOT EXISTS "${Qt6_DIR}/Qt6Config.cmake")
    message(FATAL_ERROR
        "Qt6 SDK not found.\n"
        "Nuke 17 ships Qt6 DLLs but not the build SDK.\n"
        "Install Qt6 from https://www.qt.io/download-qt-installer\n"
        "  Select: Qt 6.x.x -> MSVC 2019 64-bit\n"
        "Then pass: -DQt6_DIR=\"C:/Qt/6.x.x/msvc2019_64/lib/cmake/Qt6\"\n"
        "\n"
        "Check Nuke's Qt version:\n"
        "  (Get-Item 'C:/Program Files/Nuke17.0v1/Qt6Core.dll').VersionInfo.FileVersion")
endif()

message(STATUS "Qt6_DIR : ${Qt6_DIR}")

# Derive the Qt SDK root (three levels up from lib/cmake/Qt6)
# and add it to CMAKE_PREFIX_PATH so Qt's tool dependencies
# (Qt6GuiTools, Qt6WidgetsTools, moc, uic etc.) are all found.
get_filename_component(_qt_root "${Qt6_DIR}/../../.." ABSOLUTE)
list(APPEND CMAKE_PREFIX_PATH "${_qt_root}")
message(STATUS "Qt6 root : ${_qt_root}")

find_package(Qt6 REQUIRED
    COMPONENTS Core Widgets Gui OpenGL OpenGLWidgets
)

message(STATUS "Qt6 version : ${Qt6_VERSION}")

set(QT_LIBS Qt6::Core Qt6::Widgets Qt6::Gui Qt6::OpenGL Qt6::OpenGLWidgets)

# =============================================================================
# NDK paths
# =============================================================================
set(NDK_INCLUDE_DIR "${NUKE_ROOT}/include")

if(NOT EXISTS "${NDK_INCLUDE_DIR}/DDImage/Op.h")
    message(FATAL_ERROR
        "NDK headers not found at: ${NDK_INCLUDE_DIR}\n"
        "Verify that NUKE_ROOT points to the Nuke installation directory.\n"
        "The NDK ships with Nuke 13+ — no separate download is needed.")
endif()

if(WIN32)
    set(NDK_LIB "${NUKE_ROOT}/DDImage.lib")
elseif(APPLE)
    set(NDK_LIB "${NUKE_ROOT}/Nuke.framework/Nuke")
else()
    set(NDK_LIB "${NUKE_ROOT}/libDDImage.so")
endif()

if(NOT EXISTS "${NDK_LIB}")
    message(FATAL_ERROR "DDImage library not found: ${NDK_LIB}")
endif()

message(STATUS "NDK include : ${NDK_INCLUDE_DIR}")
message(STATUS "NDK lib     : ${NDK_LIB}")

# =============================================================================
# Plugin target
# =============================================================================
add_library(PenTool SHARED
    src/PenTool.cpp
    src/PenStroke.cpp
    src/PenOverlay.cpp
    src/AnnotationNode.cpp

    # Listed explicitly so AUTOMOC scans them for Q_OBJECT
    include/PenStroke.h
    include/PenOverlay.h
    include/AnnotationNode.h
)

target_include_directories(PenTool
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${NDK_INCLUDE_DIR}
        # No Python.h needed — Python API loaded at runtime via GetProcAddress
)

if(WIN32)
    # No python3xx.lib in Nuke install — symbols resolved at runtime via GetProcAddress
    message(STATUS "Python: resolved at runtime from Nuke's loaded python3xx.dll")
endif()

target_link_libraries(PenTool
    PRIVATE
        ${NDK_LIB}
        ${QT_LIBS}
)

# =============================================================================
# Platform-specific flags
# =============================================================================

# ── Windows / MSVC ──────────────────────────────────────────────────────────
if(WIN32)
    target_compile_options(PenTool PRIVATE
        /W3
        /WX-
        /wd4251         # 'X needs dll-interface' — expected for STL members
        /wd4275         # Non-DLL-interface base class
        /wd4996         # Suppress STLAllocator/Description deprecation warnings from NDK headers
        /wd4244         # Suppress streamsize narrowing from NDK headers
        /MP
        /permissive-
    )

    target_compile_definitions(PenTool PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX            # Prevent windows.h min/max macros clobbering std::
        _USE_MATH_DEFINES   # M_PI etc. in <cmath>
    )

    # Nuke discovers plugins by filename — no "lib" prefix, must be .dll
    set_target_properties(PenTool PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
    )

    # Export MMSInit / MMSLoaded via a .def file.
    # This is more reliable than __declspec(dllexport) because it guarantees
    # the undecorated C symbol name regardless of calling convention.
    set(_def_file "${CMAKE_CURRENT_BINARY_DIR}/PenTool.def")
    file(WRITE "${_def_file}"
        "LIBRARY PenTool\n"
        "EXPORTS\n"
        "    MMSInit\n"
        "    MMSLoaded\n"
    )
    target_link_options(PenTool PRIVATE "/DEF:${_def_file}")

# ── Linux / GCC ─────────────────────────────────────────────────────────────
elseif(UNIX AND NOT APPLE)
    target_compile_options(PenTool PRIVATE
        -fPIC
        -Wall -Wextra -Wpedantic
        -D_GLIBCXX_USE_CXX11_ABI=0   # CRITICAL — must match Nuke's CXX ABI
    )
    set_target_properties(PenTool PROPERTIES
        PREFIX ""
        SUFFIX ".so"
    )

# ── macOS ────────────────────────────────────────────────────────────────────
elseif(APPLE)
    set_target_properties(PenTool PROPERTIES
        PREFIX ""
        SUFFIX ".dylib"
    )
endif()

# =============================================================================
# Install
# =============================================================================
install(TARGETS PenTool
    DESTINATION "${CMAKE_INSTALL_PREFIX}"
)
install(FILES python/menu.py
    DESTINATION "${CMAKE_INSTALL_PREFIX}"
)

# =============================================================================
# Summary
# =============================================================================
message(STATUS "
  ┌─ PenTool configured ──────────────────────────────────────────────────┐
  │  Platform : ${CMAKE_SYSTEM_NAME}
  │  Compiler : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}
  │  Qt       : ${Qt5_VERSION}
  │
  │  Build:
  │    cmake --build . --config Release
  │
  │  Install:
  │    cmake --install . --prefix \"%USERPROFILE%/.nuke/plugins/PenTool\"
  └───────────────────────────────────────────────────────────────────────┘
")
