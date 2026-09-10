cmake_minimum_required(VERSION 3.20)

project(SlangCompiler)

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(PLATFORM "macos")
    set(SLANG_LIB_NAME "libslang.dylib")
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM "windows")
    set(SLANG_LIB_NAME "slang.lib")
elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM "linux")
    set(SLANG_LIB_NAME "libslang.so")
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
