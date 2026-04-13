# CompileShaders.cmake — DXC offline shader compilation to C++ header files.
#
# Compiles HLSL shaders at build time using the DirectX Shader Compiler (dxc.exe).
# Each shader produces a header containing a const BYTE array with the compiled bytecode.
#
# Usage:
#   target_compile_shaders(<target>
#       SHADER_MODEL <model>              # e.g. 6_7
#       SHADERS <file1.hlsl> ...          # paths relative to CMAKE_CURRENT_SOURCE_DIR
#   )
#
# Conventions (inferred from filename suffix):
#   *VS.hlsl → vs_<model>     *PS.hlsl → ps_<model>
#   *GS.hlsl → gs_<model>     *CS.hlsl → cs_<model>
#   *HS.hlsl → hs_<model>     *DS.hlsl → ds_<model>
#
# Outputs:
#   Header:   <source_dir>/CompiledShaders/<Filename>.h
#   Variable: g_p<Filename>  (const BYTE array)
#
# Debug builds add /Qembed_debug and /Zi for shader PDB info.
# Release builds add /O3 for maximum optimisation.

# Map host processor to Windows SDK bin subdirectory name.
# DXC is a host tool — always use the host architecture, not the target.
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64)$")
    set(_DXC_HOST_ARCH "x64")
elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(ARM64|aarch64)$")
    set(_DXC_HOST_ARCH "arm64")
elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(X86|x86)$")
    set(_DXC_HOST_ARCH "x86")
else()
    message(WARNING "target_compile_shaders: Unknown host architecture '${CMAKE_HOST_SYSTEM_PROCESSOR}', falling back to x64")
    set(_DXC_HOST_ARCH "x64")
endif()

find_program(DXC_EXECUTABLE dxc
    HINTS "$ENV{WindowsSdkDir}/bin/$ENV{WindowsSDKVersion}/${_DXC_HOST_ARCH}"
    DOC "DirectX Shader Compiler (dxc.exe)"
    REQUIRED)

message(STATUS "DXC found: ${DXC_EXECUTABLE}")

function(target_compile_shaders TARGET_NAME)
    cmake_parse_arguments(ARG "" "SHADER_MODEL" "SHADERS" ${ARGN})

    if(NOT ARG_SHADER_MODEL)
        message(FATAL_ERROR "target_compile_shaders: SHADER_MODEL is required")
    endif()

    set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/CompiledShaders")
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")

    # Config-dependent DXC flags (single-config generator: CMAKE_BUILD_TYPE is set at configure time)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(DXC_CONFIG_FLAGS -Qembed_debug -Zi)
    else()
        set(DXC_CONFIG_FLAGS -O3)
    endif()

    set(ALL_OUTPUTS "")

    foreach(SHADER_SOURCE IN LISTS ARG_SHADERS)
        # Resolve to absolute path
        get_filename_component(SHADER_ABS "${SHADER_SOURCE}" ABSOLUTE)
        get_filename_component(SHADER_NAME "${SHADER_SOURCE}" NAME_WE)

        # Determine shader type from filename suffix
        string(REGEX MATCH "(VS|PS|GS|CS|HS|DS)$" SHADER_SUFFIX "${SHADER_NAME}")
        if(NOT SHADER_SUFFIX)
            message(FATAL_ERROR
                "target_compile_shaders: Cannot determine shader type from '${SHADER_NAME}'. "
                "Filename must end with VS, PS, GS, CS, HS, or DS.")
        endif()

        string(TOLOWER "${SHADER_SUFFIX}" SHADER_TYPE_LOWER)
        set(SHADER_TARGET "${SHADER_TYPE_LOWER}_${ARG_SHADER_MODEL}")

        set(OUTPUT_HEADER "${OUTPUT_DIR}/${SHADER_NAME}.h")
        set(VAR_NAME "g_p${SHADER_NAME}")

        add_custom_command(
            OUTPUT "${OUTPUT_HEADER}"
            COMMAND "${DXC_EXECUTABLE}"
                -T ${SHADER_TARGET}
                -E main
                -Vn ${VAR_NAME}
                -Fh "${OUTPUT_HEADER}"
                ${DXC_CONFIG_FLAGS}
                "${SHADER_ABS}"
            DEPENDS "${SHADER_ABS}"
            COMMENT "DXC: ${SHADER_NAME} -> ${SHADER_TARGET}"
            VERBATIM
        )

        list(APPEND ALL_OUTPUTS "${OUTPUT_HEADER}")
    endforeach()

    # Custom target that aggregates all shader compilations
    add_custom_target(${TARGET_NAME}_Shaders DEPENDS ${ALL_OUTPUTS})
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_Shaders)

    # Ensure the compiled headers are findable via #include "CompiledShaders/X.h"
    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()
