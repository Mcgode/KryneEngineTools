/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2026.
 */

#pragma once

#include <KryneEngine/Core/Threads/LightweightMutex.hpp>

#include "KryneEngine/Core/Common/StringHelpers.hpp"
#include "KryneEngine/Core/Memory/Containers/FlatHashMap.hpp"
#include "ProjectManager/Logger/Logger.hpp"

namespace ProjectManager
{
    class IAssetPipeline;

    class AssetCooker
    {
    public:
        AssetCooker();
        ~AssetCooker();

        void RegisterPipeline(IAssetPipeline* _pipeline);

        void SetOutputDirectory(eastl::string_view _directory);

        [[nodiscard]] bool AddRawAssetDirectory(eastl::string_view _directory);

        static constexpr KryneEngine::u64 kLogCategory = Logger::MakeCategoryId("AssetCooker");

    private:
        KryneEngine::LightweightMutex m_mutex;
        eastl::vector<IAssetPipeline*> m_pipelines;
        eastl::string m_outputDirectory;
        eastl::vector<eastl::string> m_rawAssetDirectories;
        KryneEngine::FlatHashMap<KryneEngine::StringHash, IAssetPipeline*> m_pipelineMap;
    };
}
