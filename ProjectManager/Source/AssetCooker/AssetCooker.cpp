/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2026.
 */

#include "ProjectManager/AssetCooker/AssetCooker.hpp"

#include <filesystem>
#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Memory/Containers/FlatHashMap.inl>

#include "ProjectManager/AssetCooker/IAssetPipeline.hpp"

namespace ProjectManager
{
    AssetCooker::AssetCooker()
        : m_pipelineMap(KryneEngine::AllocatorInstance {})
    {
        Logger::GetInstance()->RegisterCategory(kLogCategory, "AssetCooker");
        Logger::GetInstance()->Log(LogSeverity::Verbose, kLogCategory, "AssetCooker initialized");
    }

    AssetCooker::~AssetCooker() = default;

    void AssetCooker::RegisterPipeline(IAssetPipeline* _pipeline)
    {
        const auto lock = m_mutex.AutoLock();

        KE_ASSERT(_pipeline != nullptr);
        KE_ASSERT(eastl::find(m_pipelines.begin(), m_pipelines.end(), _pipeline) == m_pipelines.end());

        m_pipelines.push_back(_pipeline);

        for (eastl::string& extension: _pipeline->GetHandledAssetFileExtensions())
        {
            KryneEngine::StringHash extensionHash { extension };

            KE_VERIFY(m_pipelineMap.Insert({ extensionHash,  _pipeline }).second);
        }
    }

    void AssetCooker::SetOutputDirectory(const eastl::string_view _directory)
    {
        const auto lock = m_mutex.AutoLock();
        m_outputDirectory = _directory;

        const std::filesystem::path directory { m_outputDirectory.c_str() };
        if (!std::filesystem::exists(directory))
        {
            std::filesystem::create_directories(directory);
        }
    }

    bool AssetCooker::AddRawAssetDirectory(const eastl::string_view _directory)
    {
        const auto lock = m_mutex.AutoLock();

        if (!std::filesystem::exists(std::filesystem::path(_directory.data())))
            return false;

        const eastl::string directory { _directory };
        if (eastl::find(m_rawAssetDirectories.begin(), m_rawAssetDirectories.end(), directory) != m_rawAssetDirectories.end())
            return false;
        m_rawAssetDirectories.emplace_back(eastl::move(directory));
        return true;
    }
}
