cmake_minimum_required(VERSION 3.20)

project(KryneEngineTools)

option(KE_TOOLS_PROJECT_MANAGER "Toggles inclusion of ProjectManager tool suite" ON)
option(KE_TOOLS_SHADER_COMPILER "Toggles inclusion of ShaderCompiler tool" ON)

if (KE_TOOLS_PROJECT_MANAGER)
    add_subdirectory(ProjectManager)
endif ()

if (KE_TOOLS_SHADER_COMPILER)
    add_subdirectory(SlangCompiler)
endif()