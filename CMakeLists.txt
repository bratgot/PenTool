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
# Qt 5.15
#
# Nuke 15, 16, and 17 all ship Qt 5.15.x on Windows. The cmake config files
# live inside the Nuke install tree — no separate Qt SDK needed.
#
# Expected location: <NUKE_ROOT>/lib/cmake/Qt5/Qt5Config.cmake
# =============================================================================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.15
    REQUIRED COMPONENTS Core Widgets Gui OpenGL
    HINTS
        "${NUKE_ROOT}"
        "${NUKE_ROOT}/lib/cmake"
    PATH_SUFFIXES
        "lib/cmake/Qt5"
        "cmake/Qt5"
        "cmake"
)

message(STATUS "Qt5 version : ${Qt5_VERSION}")
message(STATUS "Qt5 dir     : ${Qt5Core_DIR}")

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
)

target_link_libraries(PenTool
    PRIVATE
        ${NDK_LIB}
        Qt5::Core
        Qt5::Widgets
        Qt5::Gui
        Qt5::OpenGL
)

# =============================================================================
# Platform-specific flags
# =============================================================================

# ── Windows / MSVC ──────────────────────────────────────────────────────────
if(WIN32)
    target_compile_options(PenTool PRIVATE
        /W3             # Warning level 3
        /WX-            # Warnings are not errors (NDK headers can be noisy)
        /wd4251         # 'X needs dll-interface' — expected for STL members
        /wd4275         # Non-DLL-interface base class — same reason
        /MP             # Multi-processor compilation
        /permissive-    # Stricter standard conformance
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
