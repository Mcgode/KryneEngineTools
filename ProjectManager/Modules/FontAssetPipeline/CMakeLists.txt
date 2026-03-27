
add_library(ProjectManagerLib_FontAssetPipeline STATIC
        Source/FontAssetPipeline.cpp
        Include/ProjectManager/FontAssetPipeline/FontAssetPipeline.hpp
)

target_include_directories(ProjectManagerLib_FontAssetPipeline PUBLIC Include)

find_package(Freetype REQUIRED)
target_link_libraries(ProjectManagerLib_FontAssetPipeline PRIVATE Freetype::Freetype msdfgen-core)
target_link_libraries(ProjectManagerLib_FontAssetPipeline PUBLIC ProjectManagerLib KryneEngine_Modules_TextRendering)

set_target_properties(ProjectManagerLib_FontAssetPipeline PROPERTIES FOLDER EngineTools)