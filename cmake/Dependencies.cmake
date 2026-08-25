# Third-party dependencies.
#
# Everything is pulled with FetchContent and pinned to an explicit tag so a
# clean checkout always builds the same code. Set TNP_USE_SYSTEM_PACKAGES=ON to
# prefer packages already installed on the machine (useful for distro builds).

include(FetchContent)

option(TNP_USE_SYSTEM_PACKAGES "Prefer find_package() over downloading dependencies" OFF)
if(TNP_USE_SYSTEM_PACKAGES)
    set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE ALWAYS)
endif()

set(FETCHCONTENT_QUIET OFF)

# --- nlohmann/json ---------------------------------------------------------
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
    GIT_SHALLOW    TRUE
    SYSTEM)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# --- Catch2 ----------------------------------------------------------------
if(TNP_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.8.1
        GIT_SHALLOW    TRUE
        SYSTEM)
    set(CATCH_INSTALL_DOCS OFF CACHE INTERNAL "")
    set(CATCH_INSTALL_EXTRAS OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
endif()

if(NOT TNP_BUILD_UI)
    return()
endif()

# --- GLFW ------------------------------------------------------------------
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
    SYSTEM)
set(GLFW_BUILD_DOCS     OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS    OFF CACHE INTERNAL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GLFW_INSTALL        OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(glfw)

# --- Dear ImGui ------------------------------------------------------------
# The docking branch is required: TNP's workspace is built on DockSpace.
# Dear ImGui ships no CMakeLists, so the target is declared here.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9b-docking
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(imgui)

find_package(OpenGL REQUIRED)

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
    # std::string overloads for InputText, so panels can edit strings directly
    # instead of shuffling data through fixed char buffers.
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp")

target_include_directories(imgui SYSTEM PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
    "${imgui_SOURCE_DIR}/misc/cpp")

target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
target_compile_features(imgui PUBLIC cxx_std_20)
set_target_properties(imgui PROPERTIES FOLDER "ThirdParty")

if(APPLE)
    # Silence the OpenGL deprecation noise from the Apple SDK headers; the
    # OpenGL 3.2 core profile is still the most portable ImGui backend.
    target_compile_definitions(imgui PUBLIC GL_SILENCE_DEPRECATION)
endif()
