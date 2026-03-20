cmake_minimum_required(VERSION 3.20)

project(ProjectManager LANGUAGES C CXX)

option(KE_TOOLS_PM_STANDALONE "Toggles the build of the Engine standalone executable" ON)

# Set up yyjson

add_library(yyjson STATIC Extern/yyjson-0.12.0/yyjson.c Extern/yyjson-0.12.0/yyjson.h)
target_include_directories(yyjson PUBLIC Extern/yyjson-0.12.0)


# Set up sqlite

add_library(sqlite STATIC
        Extern/sqlite-3.51.3/shell.c
        Extern/sqlite-3.51.3/sqlite3.c
        Extern/sqlite-3.51.3/sqlite3.h
        Extern/sqlite-3.51.3/sqlite3ext.h
)
target_include_directories(sqlite PUBLIC Extern/sqlite-3.51.3)


# Set up Project manager lib

add_library(ProjectManagerLib STATIC
    Source/Application.cpp
        Include/ProjectManager/Application.hpp
        Source/Logger/Logger.cpp
        Include/ProjectManager/Logger/Logger.hpp
        Include/ProjectManager/Logger/LogFilter.hpp
        Include/ProjectManager/Logger/LogSeverity.hpp
        Source/Logger/CoreCategory.hpp
        Source/Logger/LogWindow.cpp
        Source/Logger/LogWindow.hpp
        Include/ProjectManager/IUiWindow.hpp
        Source/AssetCooker/AssetCooker.cpp
        Include/ProjectManager/AssetCooker/AssetCooker.hpp
        Include/ProjectManager/AssetCooker/IAssetPipeline.hpp
        Source/AssetCooker/AssetCookerWindow.cpp
        Source/AssetCooker/AssetCookerWindow.hpp
        Source/AssetCooker/DirectoryMonitor.cpp
        Source/AssetCooker/DirectoryMonitor.hpp
        Source/Database/Database.cpp
        Include/ProjectManager/Database/Database.hpp
        Include/ProjectManager/Database/DatabaseTable.hpp
        Include/ProjectManager/Database/DatabaseType.hpp
)

target_include_directories(ProjectManagerLib PUBLIC Include)
target_include_directories(ProjectManagerLib PRIVATE Source)
target_link_libraries(ProjectManagerLib KryneEngine_Core KryneEngine_Modules_ImGui sqlite)

set_target_properties(ProjectManagerLib PROPERTIES FOLDER "EngineTools")


if (KE_TOOLS_PM_STANDALONE)
    add_executable(ProjectManager Standalone/ProjectManager.cpp)

    target_link_libraries(ProjectManager PUBLIC ProjectManagerLib)
    set_target_properties(ProjectManager PROPERTIES FOLDER "EngineTools")
endif ()