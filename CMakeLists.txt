# ---------------------------------------------------------------------------
# The Network Project (TNP)
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)

project(TNP
    VERSION 0.5.0
    DESCRIPTION "The Network Project - visual network design, simulation and analysis"
    LANGUAGES CXX)

# --- Global settings -------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Multi-config generators put binaries in per-config sub-directories; keep the
# runtime assets discoverable by pinning every configuration to the same place.
foreach(_cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}/bin")
endforeach()

option(TNP_BUILD_TESTS "Build the TNP test suite" ON)
option(TNP_BUILD_UI    "Build the graphical application (requires GLFW/OpenGL)" ON)
option(TNP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(TnpCompilerOptions)
include(Dependencies)

# --- Modules ---------------------------------------------------------------
# The dependency direction is strictly one-way:
#
#   ui -> app -> {cli, testing, commands, validation, simulation, serialization}
#                                   -> core -> utilities
#
# No lower layer may link against a higher one.
add_subdirectory(src/utilities)
add_subdirectory(src/core)
add_subdirectory(src/simulation)
add_subdirectory(src/serialization)
add_subdirectory(src/validation)
add_subdirectory(src/testing)
add_subdirectory(src/commands)
add_subdirectory(src/cli)
add_subdirectory(src/app)

if(TNP_BUILD_UI)
    add_subdirectory(src/ui)
endif()

# --- Application executable ------------------------------------------------
if(TNP_BUILD_UI)
    add_executable(tnp src/app/main.cpp)
    target_link_libraries(tnp PRIVATE tnp::tnp_ui tnp::tnp_app)
    tnp_set_target_options(tnp)

    if(WIN32)
        # A release build is a desktop application, so it is linked into the
        # Windows subsystem and no console window appears behind it. Debug keeps
        # the console, where the log is genuinely useful.
        set_target_properties(tnp PROPERTIES WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)

        # That subsystem expects WinMain. main.cpp deliberately defines a plain,
        # portable main instead, so the CRT entry point is pointed at it rather
        # than adding a Windows-only shim to an otherwise portable file.
        # MinGW needs none of this: it supplies its own WinMain that calls main.
        if(MSVC)
            target_link_options(tnp PRIVATE $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>)
        endif()
    endif()

    if(APPLE)
        set_target_properties(tnp PROPERTIES MACOSX_BUNDLE FALSE)
    endif()
endif()

# --- Headless console tool -------------------------------------------------
# Validates, simulates and tests a project without opening a window. Also used
# by CI to exercise the engine end-to-end.
add_executable(tnpcli src/app/cli_main.cpp)
target_link_libraries(tnpcli PRIVATE tnp::tnp_app)
tnp_set_target_options(tnpcli)

# --- Tests -----------------------------------------------------------------
if(TNP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Summary ---------------------------------------------------------------
message(STATUS "")
message(STATUS "TNP ${PROJECT_VERSION}")
message(STATUS "  Build type ......... ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard ....... ${CMAKE_CXX_STANDARD}")
message(STATUS "  Graphical UI ....... ${TNP_BUILD_UI}")
message(STATUS "  Tests .............. ${TNP_BUILD_TESTS}")
message(STATUS "")
