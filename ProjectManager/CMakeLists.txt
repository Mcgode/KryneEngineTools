cmake_minimum_required(VERSION 3.20)

project(ProjectManager LANGUAGES C CXX)

option(KE_TOOLS_PM_STANDALONE "Toggles the build of the Engine standalone executable" ON)

# Set up yyjson

add_library(yyjson STATIC Extern/yyjson-0.12.0/yyjson.c Extern/yyjson-0.12.0/yyjson.h)
target_include_directories(yyjson PUBLIC Extern/yyjson-0.12.0)


# Set up Project manager lib

add_library(ProjectManagerLib STATIC
    Source/Application.cpp
        Include/ProjectManager/Application.hpp)

target_include_directories(ProjectManagerLib PUBLIC Include)
target_link_libraries(ProjectManagerLib KryneEngine_Core KryneEngine_Modules_ImGui)

set_target_properties(ProjectManagerLib PROPERTIES FOLDER "EngineTools")


if (KE_TOOLS_PM_STANDALONE)
    add_executable(ProjectManager Standalone/ProjectManager.cpp)

    target_link_libraries(ProjectManager PUBLIC ProjectManagerLib)
    set_target_properties(ProjectManager PROPERTIES FOLDER "EngineTools")
endif ()