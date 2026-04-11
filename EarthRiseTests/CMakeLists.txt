# EarthRiseTests — native unit test project
# GameLogic and GameRender sources are compiled directly into the test DLL (not linked
# as static libs) because the test DLL requires dynamic CRT (/MD) while the main
# projects use static CRT (/MT).
add_library(EarthRiseTests SHARED
    pch.cpp
    EntityHandleTests.cpp
    EntityPoolTests.cpp
    NetworkingTests.cpp
    SpaceObjectManagerTests.cpp
    SerializationTests.cpp
    RenderMathTests.cpp
    # GameLogic sources needed by tests, compiled with dynamic CRT
    ${CMAKE_SOURCE_DIR}/GameLogic/EntityPool.cpp
    ${CMAKE_SOURCE_DIR}/GameLogic/SpaceObjectManager.cpp
    # GameRender sources needed by tests (CPU-only code, no GPU)
    ${CMAKE_SOURCE_DIR}/GameRender/CmoLoader.cpp
    # NeuronCore networking sources needed by tests
    ${CMAKE_SOURCE_DIR}/NeuronCore/ReliableChannel.cpp
    # NeuronServer sources needed by tests
    ${CMAKE_SOURCE_DIR}/NeuronServer/BandwidthManager.cpp
)

target_include_directories(EarthRiseTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/GameLogic
    ${CMAKE_SOURCE_DIR}/GameRender
    ${CMAKE_SOURCE_DIR}/NeuronClient
    ${CMAKE_SOURCE_DIR}/NeuronClient/Graphics
    ${CMAKE_SOURCE_DIR}/NeuronClient/Audio
    ${CMAKE_SOURCE_DIR}/NeuronCore
    ${CMAKE_SOURCE_DIR}/NeuronCore/GameMath
    ${CMAKE_SOURCE_DIR}/NeuronCore/GameTypes
    ${CMAKE_SOURCE_DIR}/NeuronServer
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
    ${CMAKE_SOURCE_DIR}/GameRender/CmoLoader.cpp
    ${CMAKE_SOURCE_DIR}/NeuronCore/ReliableChannel.cpp
    ${CMAKE_SOURCE_DIR}/NeuronServer/BandwidthManager.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# No library dependencies — sources are compiled directly with dynamic CRT.
# System libs are pulled in via #pragma comment(lib, ...) in headers.

# Pass source dir so tests can locate GameData/Meshes at runtime
target_compile_definitions(EarthRiseTests PRIVATE
    EARTHRISE_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}"
)

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

# ---------------------------------------------------------------------------
# Discover individual TEST_METHOD entries and register each with CTest.
# VS Test Explorer in CMake+Ninja mode only sees CTest entries — the native
# C++ Unit Test adapter does not scan DLLs for CMake projects.  By registering
# each method separately, every test appears individually in Test Explorer.
# ---------------------------------------------------------------------------
function(discover_msvc_unit_tests TARGET)
    find_program(VSTEST_CONSOLE
        NAMES vstest.console.exe
        HINTS
            "$ENV{VSINSTALLDIR}/Common7/IDE/CommonExtensions/Microsoft/TestWindow"
            "$ENV{DevEnvDir}/CommonExtensions/Microsoft/TestWindow"
    )

    if(NOT VSTEST_CONSOLE)
        message(WARNING "vstest.console.exe was not found; ${TARGET} tests will not be registered with CTest.")
        return()
    endif()

    get_target_property(SOURCES ${TARGET} SOURCES)
    foreach(SOURCE ${SOURCES})
        # Resolve relative paths against the target's source directory
        if(NOT IS_ABSOLUTE "${SOURCE}")
            get_target_property(SOURCE_DIR ${TARGET} SOURCE_DIR)
            set(SOURCE "${SOURCE_DIR}/${SOURCE}")
        endif()

        # Skip files that don't exist (generator expressions, etc.)
        if(NOT EXISTS "${SOURCE}")
            continue()
        endif()

        file(STRINGS "${SOURCE}" TEST_LINES REGEX "TEST_METHOD\\(")
        foreach(LINE ${TEST_LINES})
            string(REGEX MATCH "TEST_METHOD\\(([A-Za-z0-9_]+)\\)" _ "${LINE}")
            set(METHOD_NAME "${CMAKE_MATCH_1}")
            if(METHOD_NAME)
                add_test(
                    NAME "${TARGET}.${METHOD_NAME}"
                    COMMAND "${VSTEST_CONSOLE}" "$<TARGET_FILE:${TARGET}>"
                            "/Tests:${METHOD_NAME}" /Platform:x64 /InIsolation
                )
                set_tests_properties("${TARGET}.${METHOD_NAME}" PROPERTIES
                    WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TARGET}>"
                )
            endif()
        endforeach()
    endforeach()
endfunction()

if(BUILD_TESTING)
    discover_msvc_unit_tests(EarthRiseTests)
endif()
