cmake_minimum_required(VERSION 3.20)

project(SlangCompiler)

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(PLATFORM "macos")
    set(SLANG_LIB_NAME "libslang-compiler.0.2026.20.dylib")
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM "win32")
    set(SLANG_LIB_NAME "slang-compiler.lib")
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM "linux")
    set(SLANG_LIB_NAME "libslang-compiler.so.0.2026.20")
else ()
    message(FATAL_ERROR "Unsupported host platform: ${CMAKE_HOST_SYSTEM_NAME}")
endif ()

string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" SYSTEM_PROCESSOR)
if (SYSTEM_PROCESSOR MATCHES "amd64|x64|x86_64")
    set(ARCH "x64")
elseif (SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(ARCH "aarch64")
endif ()

message(STATUS "Platform: ${PLATFORM}")
message(STATUS "Configuration: ${ARCH}")

set(SLANG_LIB "${CMAKE_CURRENT_SOURCE_DIR}/external/slang/lib/${PLATFORM}/${ARCH}/${SLANG_LIB_NAME}")

# SlangCompiler is a host-side build tool. It is only built when a prebuilt Slang
# runtime for the current target is vendored - notably it is skipped for the
# macOS -> Windows cross build, where no such binary ships.
if (NOT PLATFORM OR NOT ARCH OR NOT EXISTS "${SLANG_LIB}")
    message(WARNING "SlangCompiler: no Slang runtime for ${PLATFORM}/${ARCH} (looked for '${SLANG_LIB}'); skipping the tool.")
    return ()
endif ()

add_executable(SlangCompiler
        SlangCompiler.cpp
)

target_include_directories(SlangCompiler PRIVATE external/slang/include)

target_link_libraries(SlangCompiler PUBLIC KryneEngine_Core_Link KryneEngine_Modules_ShaderReflection)
target_link_libraries(SlangCompiler PRIVATE "${SLANG_LIB}")
set_target_properties(SlangCompiler PROPERTIES FOLDER "EngineTools")

# Slang is loaded dynamically, so the DLLs have to sit next to the executable (and next to any installed copy).
# Only the ones that are actually needed are copied:
#  - slang-compiler.dll: the compiler, the one slang-compiler.lib imports
#  - slang-glslang.dll: loaded on demand as the 'spirv-opt' downstream, required for SPIR-V output
# gfx, slang-rt, slang-llvm and slang-glsl-module are not needed: we use neither the gfx API, CPU/host-callable
# targets nor GLSL input.
# dxcompiler.dll and dxil.dll are required for the DXIL target. They come from the engine's DirectXCompiler copy,
# so that we don't depend on whichever dxcompiler.dll happens to be in the PATH.
if (PLATFORM STREQUAL "win32")
    set(SLANG_DLL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/slang/lib/${PLATFORM}/${ARCH}")
    if (ARCH STREQUAL "aarch64")
        set(DXC_ARCH "arm64")
    else ()
        set(DXC_ARCH "${ARCH}")
    endif ()
    set(DXC_DLL_DIR "${CMAKE_SOURCE_DIR}/External/DirectXCompiler/win32/bin/${DXC_ARCH}")

    set(SLANG_RUNTIME_DLLS
            "${SLANG_DLL_DIR}/slang-compiler.dll"
            "${SLANG_DLL_DIR}/slang-glslang.dll"
            "${DXC_DLL_DIR}/dxcompiler.dll"
            "${DXC_DLL_DIR}/dxil.dll")

    add_custom_command(TARGET SlangCompiler POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SLANG_RUNTIME_DLLS} "$<TARGET_FILE_DIR:SlangCompiler>"
            VERBATIM)

    install(TARGETS SlangCompiler RUNTIME DESTINATION bin)
    install(FILES ${SLANG_RUNTIME_DLLS} DESTINATION bin)
endif ()
