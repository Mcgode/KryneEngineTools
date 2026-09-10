cmake_minimum_required(VERSION 3.20)

project(KryneEngineTools)

# ProjectManager is a host-side editor. It is not part of a shipped runtime and its
# asset pipeline assumes a native (non-cross) toolchain, so default it off when
# cross-compiling (e.g. the macOS -> Windows build).
if (CMAKE_CROSSCOMPILING)
    set(_KE_TOOLS_PROJECT_MANAGER_DEFAULT OFF)
else ()
    set(_KE_TOOLS_PROJECT_MANAGER_DEFAULT ON)
endif ()

option(KE_TOOLS_PROJECT_MANAGER "Toggles inclusion of ProjectManager tool suite" ${_KE_TOOLS_PROJECT_MANAGER_DEFAULT})
option(KE_TOOLS_SHADER_COMPILER "Toggles inclusion of ShaderCompiler tool" ON)

if (KE_TOOLS_PROJECT_MANAGER)
    add_subdirectory(ProjectManager)
endif ()

if (KE_TOOLS_SHADER_COMPILER)
    add_subdirectory(SlangCompiler)
endif()