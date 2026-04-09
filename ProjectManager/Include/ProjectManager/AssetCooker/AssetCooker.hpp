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
    class ArchivePipeline;

    class AssetCooker
    {
        friend ArchivePipeline;
        friend DirectoryMonitor;
        friend class AssetCookerWindow;

    public:
        explicit AssetCooker(Database* _database);
        ~AssetCooker();

        void RegisterPipeline(IAssetPipeline* _pipeline);

        void SetOutputDirectory(eastl::string_view _directory);

        [[nodiscard]] bool AddRawAssetDirectory(eastl::string_view _directory);

        void Run();

        static constexpr KryneEngine::u64 kLogCategory = Logger::MakeCategoryId("AssetCooker");

        void* RequestSupport(void* _entry, eastl::function<void()> _workFunction);
        void FinalizeSupportRequest(void* _request);

        [[nodiscard]] bool ShouldCancelCook(void* _entry);

    private:
        Database* m_database;
        KryneEngine::LightweightMutex m_mutex;
        eastl::vector<IAssetPipeline*> m_pipelines;
        eastl::vector_map<KryneEngine::u32, IAssetPipeline*> m_pipelineIds;
        eastl::unique_ptr<ArchivePipeline> m_archiver;
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
            bool m_shouldCancel = false;

            bool operator==(const QueueEntry& _other) const
            {
                return m_assetId == _other.m_assetId;
            }

            bool operator<(const QueueEntry& _other) const
            {
                return m_assetId < _other.m_assetId;
            }
        };

        struct SupportWork
        {
            QueueEntry* m_entry;
            eastl::function<void()> m_workFunction;
            std::atomic<int> m_supporterCount = 0;
            std::condition_variable m_supporterCondition;
        };

        std::mutex m_queueMutex;
        std::condition_variable m_queueCondition;
        eastl::queue<QueueEntry> m_updateQueue;
        eastl::vector_set<QueueEntry> m_archiveQueue;
        eastl::vector_map<std::filesystem::path, KryneEngine::u32> m_cookingAssets;
        eastl::vector_map<KryneEngine::u32, QueueEntry*> m_cookingArchives;
        eastl::vector<SupportWork*> m_supportWorkRequests;

        eastl::vector<std::filesystem::path> m_cookingAssetPerThread;
        KryneEngine::SpinLock m_cookedAssetsLock;
        eastl::vector<std::filesystem::path> m_cookedAssets;

        KryneEngine::DynamicArray<std::thread> m_workThreads;
        volatile bool m_stopWork = false;

        void ProbeDirectory(
            const std::filesystem::path& _path);

        void ProbeInputAsset(const std::filesystem::path& _asset, const std::filesystem::path& _assetDirectory = {});

        void ProbeOutputAsset(const std::filesystem::path& _asset);

        void OnInputAssetRenamed(const std::filesystem::path& _oldPath, const std::filesystem::path& _newPath) const;

        void OnInputAssetDeleted(const std::filesystem::path& _asset);

        IAssetPipeline* FindPipeline(const std::filesystem::path& _asset);

        void ThreadMain(KryneEngine::u32 _index);

        void Enqueue(QueueEntry&& _entry);
    };
}
