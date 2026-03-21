/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2026.
 */

#pragma once

#include <filesystem>
#include <EASTL/queue.h>
#include <EASTL/vector_set.h>
#include <EASTL/unique_ptr.h>
#include <KryneEngine/Core/Common/StringHelpers.hpp>
#include <KryneEngine/Core/Memory/Containers/FlatHashMap.hpp>
#include <KryneEngine/Core/Threads/LightweightMutex.hpp>

#include "KryneEngine/Core/Memory/DynamicArray.hpp"
#include "ProjectManager/Logger/Logger.hpp"

namespace ProjectManager
{
    class Database;
    class DirectoryMonitor;
    class IAssetPipeline;

    class AssetCooker
    {
    public:
        explicit AssetCooker(Database* _database);
        ~AssetCooker();

        void RegisterPipeline(IAssetPipeline* _pipeline);

        void SetOutputDirectory(eastl::string_view _directory);

        [[nodiscard]] bool AddRawAssetDirectory(eastl::string_view _directory);

        void Run();

        static constexpr KryneEngine::u64 kLogCategory = Logger::MakeCategoryId("AssetCooker");

    private:
        Database* m_database;
        KryneEngine::LightweightMutex m_mutex;
        eastl::vector<IAssetPipeline*> m_pipelines;
        std::filesystem::path m_outputDirectory;
        eastl::vector<std::filesystem::path> m_rawAssetDirectories;
        bool m_running = false;
        KryneEngine::FlatHashMap<KryneEngine::StringHash, IAssetPipeline*> m_pipelineMap;
        eastl::unique_ptr<DirectoryMonitor> m_directoryMonitor;

        std::thread m_probingThread;
        struct QueueEntry
        {
            std::filesystem::path m_asset {};
            std::filesystem::path m_assetDirectory {};
            IAssetPipeline* m_pipeline = nullptr;
            KryneEngine::u32 m_assetId = 0;
            KryneEngine::u32 m_pipelineId = 0;
            bool m_forceCook = false;
        };
        std::mutex m_queueMutex;
        std::condition_variable m_queueCondition;
        eastl::queue<QueueEntry> m_updateQueue;
        eastl::vector_map<std::filesystem::path, KryneEngine::u32> m_cookingAssets;

        KryneEngine::DynamicArray<std::thread> m_workThreads;
        volatile bool m_stopWork = false;

        void ProbeDirectory(
            const std::filesystem::path& _path);
    };
}
