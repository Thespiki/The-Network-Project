# Shared compiler configuration for every TNP target.
#
# Kept in one place so warning levels stay consistent across the modules and so
# platform specific flags never leak into the individual CMakeLists files.

function(tnp_set_target_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-        # standards conformant mode
            /Zc:__cplusplus     # report the real __cplusplus value
            /Zc:preprocessor
            /utf-8
            /MP                 # parallel compilation
            /external:W0        # silence warnings from CMake SYSTEM includes
            /wd4251             # STL types in exported class interfaces
        )
        target_compile_definitions(${target} PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
        )
        if(TNP_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wno-missing-field-initializers
        )
        if(TNP_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Declares a TNP module: a static library plus its `tnp::` alias, with the
# repository `src/` directory exposed as an include root.
function(tnp_add_module name)
    cmake_parse_arguments(ARG "" "" "SOURCES;PUBLIC_DEPS;PRIVATE_DEPS" ${ARGN})

    add_library(${name} STATIC ${ARG_SOURCES})
    add_library(tnp::${name} ALIAS ${name})

    string(REPLACE "tnp_" "" _short "${name}")
    set_target_properties(${name} PROPERTIES
        OUTPUT_NAME "tnp_${_short}"
        FOLDER "TNP")

    target_include_directories(${name} PUBLIC "${CMAKE_SOURCE_DIR}/src")

    if(ARG_PUBLIC_DEPS)
        target_link_libraries(${name} PUBLIC ${ARG_PUBLIC_DEPS})
    endif()
    if(ARG_PRIVATE_DEPS)
        target_link_libraries(${name} PRIVATE ${ARG_PRIVATE_DEPS})
    endif()

    tnp_set_target_options(${name})
endfunction()
