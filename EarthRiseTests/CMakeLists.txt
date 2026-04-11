# EarthRiseTests — native unit test project
# GameLogic sources are compiled directly into the test DLL (not linked as a static lib)
# because the test DLL requires dynamic CRT (/MD) while the main projects use static CRT (/MT).
# NeuronCore is header-only for test purposes (types + templates are in headers).
add_library(EarthRiseTests SHARED
    pch.cpp
    EntityHandleTests.cpp
    EntityPoolTests.cpp
    SpaceObjectManagerTests.cpp
    SerializationTests.cpp
    # GameLogic sources needed by tests, compiled with dynamic CRT
    ${CMAKE_SOURCE_DIR}/GameLogic/EntityPool.cpp
    ${CMAKE_SOURCE_DIR}/GameLogic/SpaceObjectManager.cpp
)

target_include_directories(EarthRiseTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/GameLogic
    ${CMAKE_SOURCE_DIR}/NeuronCore
    ${CMAKE_SOURCE_DIR}/NeuronCore/GameMath
    ${CMAKE_SOURCE_DIR}/NeuronCore/GameTypes
)

# Per-module build output → <module>/<Config>/<Platform>/
set_target_properties(EarthRiseTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY     "${CMAKE_SOURCE_DIR}/${EARTHRISE_PLATFORM_NAME}/$<CONFIG>"
    # Unit test DLLs must use dynamic CRT (/MD) to match CppUnitTestFramework
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

# Precompiled header (skip for GameLogic sources compiled into this target)
target_precompile_headers(EarthRiseTests PRIVATE pch.h)
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/GameLogic/EntityPool.cpp
    ${CMAKE_SOURCE_DIR}/GameLogic/SpaceObjectManager.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# No library dependencies — NeuronCore types are header-only for test purposes.
# System libs are pulled in via #pragma comment(lib, ...) in NeuronCoreBase.h.

# Visual Studio Native Unit Test Framework
# The unit test framework ships with VS under VC/Auxiliary/VS/UnitTest/.
# Locate it via VSINSTALLDIR (set by VS developer prompt) or VCToolsInstallDir.
if(DEFINED ENV{VSINSTALLDIR})
    set(VS_INSTALL_DIR "$ENV{VSINSTALLDIR}")
    set(UNITTEST_INCLUDE_DIR "${VS_INSTALL_DIR}VC/Auxiliary/VS/UnitTest/include")
    set(UNITTEST_LIB_DIR "${VS_INSTALL_DIR}VC/Auxiliary/VS/UnitTest/lib/x64")
elseif(DEFINED ENV{VCToolsInstallDir})
    # VCToolsInstallDir is like .../VC/Tools/MSVC/14.xx.xxxxx/
    # UnitTest is at .../VC/Auxiliary/VS/UnitTest/
    set(VC_TOOLS_DIR "$ENV{VCToolsInstallDir}")
    get_filename_component(VC_DIR "${VC_TOOLS_DIR}/../../.." ABSOLUTE)
    set(UNITTEST_INCLUDE_DIR "${VC_DIR}/Auxiliary/VS/UnitTest/include")
    set(UNITTEST_LIB_DIR "${VC_DIR}/Auxiliary/VS/UnitTest/lib/x64")
else()
    get_filename_component(VC_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(VC_DIR "${VC_BIN_DIR}/../../../../.." ABSOLUTE)
    set(UNITTEST_INCLUDE_DIR "${VC_DIR}/Auxiliary/VS/UnitTest/include")
    set(UNITTEST_LIB_DIR "${VC_DIR}/Auxiliary/VS/UnitTest/lib/x64")
endif()

if(EXISTS "${UNITTEST_INCLUDE_DIR}")
    target_include_directories(EarthRiseTests PRIVATE "${UNITTEST_INCLUDE_DIR}")
    # CppUnitTestCommon.h embeds: #pragma comment(lib, "x64\\...CppUnitTestFramework.lib")
    # We need the parent of x64/ as a lib directory so the relative path resolves.
    cmake_path(GET UNITTEST_LIB_DIR PARENT_PATH UNITTEST_LIB_PARENT)
    target_link_directories(EarthRiseTests PRIVATE "${UNITTEST_LIB_PARENT}")
else()
    message(WARNING "VS Native Unit Test Framework not found at: ${UNITTEST_INCLUDE_DIR}")
endif()

# Compile definitions
target_compile_definitions(EarthRiseTests PRIVATE _LIB)
