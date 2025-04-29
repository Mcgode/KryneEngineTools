cmake_minimum_required(VERSION 3.20)

project(SlangCompiler)

add_executable(SlangCompiler
        main.cpp
)

target_include_directories(SlangCompiler PRIVATE external/slang/include)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(PLATFORM "macos")
endif ()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSTEM_PROCESSOR)
if (SYSTEM_PROCESSOR MATCHES "amd64" OR SYSTEM_PROCESSOR MATCHES "x64")
    set(ARCH "x64")
elseif (SYSTEM_PROCESSOR MATCHES "aarch64" OR SYSTEM_PROCESSOR MATCHES "arm64")
    set(ARCH "aarch64")
endif ()

message(STATUS "Platform: ${PLATFORM}")
message(STATUS "Configuration: ${ARCH}")


target_link_libraries(SlangCompiler PUBLIC KryneEngine_Core KryneEngine_Modules_ShaderReflection)
target_link_libraries(SlangCompiler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/slang/lib/${PLATFORM}/${ARCH}/libslang.dylib)