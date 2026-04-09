/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2026.
 */

#include "ProjectManager/AssetCooker/AssetCooker.hpp"

#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Memory/Containers/FlatHashMap.inl>

#include "AssetCooker/ArchivePipeline.hpp"
#include "AssetCooker/DirectoryMonitor.hpp"
#include "EASTL/fixed_vector.h"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"
#include "ProjectManager/Utils.hpp"
#include "ProjectManager/AssetCooker/IAssetPipeline.hpp"
#include "ProjectManager/Database/Database.hpp"

namespace ProjectManager
{
    AssetCooker::AssetCooker(Database* _database)
        : m_database(_database)
        , m_pipelineMap(KryneEngine::AllocatorInstance {})
    {
        Logger::GetInstance()->RegisterCategory(kLogCategory, "AssetCooker");

        m_archiver = eastl::make_unique<ArchivePipeline>();
        m_archiver->m_assetCooker = this;

        Logger::GetInstance()->Log(LogSeverity::Verbose, kLogCategory, "Setting up AssetCooker database tables");

        if (!m_database->TableExists("assets"))
        {
            Logger::GetInstance()->Log(LogSeverity::Info, kLogCategory, "Creating AssetCooker 'assets' database table");
            const char* sql = "CREATE TABLE assets (id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT NOT NULL)";
            const int result = m_database->Execute(sql);
            KE_ASSERT(result == SQLITE_OK);
        }

        if (!m_database->TableExists("assetPipelines"))
        {
            Logger::GetInstance()->Log(LogSeverity::Info, kLogCategory, "Creating AssetCooker 'assetPipelines' database table");
            const char* sql = "CREATE TABLE assetPipelines (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, version BIGINT NOT NULL)";
            const int result = m_database->Execute(sql);
            KE_ASSERT(result == SQLITE_OK);
        }

        if (!m_database->TableExists("cookedAssets"))
        {
            Logger::GetInstance()->Log(LogSeverity::Info, kLogCategory, "Creating AssetCooker 'cookedAssets' database table");
            const char* sql = "CREATE TABLE cookedAssets (sourceId INTEGER, path TEXT NOT NULL, pipeline INTEGER NOT NULL, pipelineVersion BIGINT NOT NULL, writeDate BIGINT NOT NULL)";
            const int result = m_database->Execute(sql);
            KE_ASSERT(result == SQLITE_OK);
        }

        Logger::GetInstance()->Log(LogSeverity::Verbose, kLogCategory, "AssetCooker initialized");

        m_workThreads.Resize(eastl::max(1u, std::thread::hardware_concurrency() - 1));

        RegisterPipeline(m_archiver.get());
    }

    AssetCooker::~AssetCooker()
    {
        m_stopWork = true;
        m_queueCondition.notify_all();
        for (auto& workThread: m_workThreads)
        {
            if (workThread.joinable())
                workThread.join();
        }

        if (m_probingThread.joinable())
        {
            m_probingThread.join();
        }
    }

    void AssetCooker::RegisterPipeline(IAssetPipeline* _pipeline)
    {
        const auto lock = m_mutex.AutoLock();

        KE_ASSERT(!m_running);
        KE_ASSERT(_pipeline != nullptr);
        KE_ASSERT(eastl::find(m_pipelines.begin(), m_pipelines.end(), _pipeline) == m_pipelines.end());

        m_pipelines.push_back(_pipeline);

        for (const char* extension: _pipeline->GetHandledAssetFileExtensions())
        {
            KryneEngine::StringHash extensionHash { extension };

            KE_VERIFY(m_pipelineMap.Insert({ extensionHash,  _pipeline }).second);
        }
    }

    void AssetCooker::SetOutputDirectory(const eastl::string_view _directory)
    {
        const auto lock = m_mutex.AutoLock();
        KE_ASSERT(!m_running);

        m_outputDirectory = std::filesystem::canonical(_directory.data());

        const std::filesystem::path directory { m_outputDirectory.c_str() };
        if (!std::filesystem::exists(directory))
        {
            std::filesystem::create_directories(directory);
        }
    }

    bool AssetCooker::AddRawAssetDirectory(const eastl::string_view _directory)
    {
        const auto lock = m_mutex.AutoLock();

        KE_ASSERT(!m_running);

        if (!std::filesystem::exists(std::filesystem::path(_directory.data())))
            return false;

        const std::filesystem::path directory = std::filesystem::canonical(_directory.data());
        if (eastl::find(m_rawAssetDirectories.begin(), m_rawAssetDirectories.end(), directory) != m_rawAssetDirectories.end())
            return false;
        m_rawAssetDirectories.emplace_back(directory);
        return true;
    }

    void AssetCooker::Run()
    {
        const auto lock = m_mutex.AutoLock();

        KE_ASSERT(!m_outputDirectory.empty());
        KE_ASSERT(!m_rawAssetDirectories.empty());

        m_running = true;
        m_directoryMonitor = eastl::make_unique<DirectoryMonitor>(this);

        {
            char sql[2048];
            sqlite3_stmt* stmt;
            for (IAssetPipeline* pipeline : m_pipelines)
            {
                bool matchedVersions = false;
                snprintf(sql, sizeof(sql), "SELECT version, id FROM assetPipelines WHERE name = '%s'", pipeline->GetName().data());
                KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);
                if (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    KE_ASSERT(sqlite3_column_count(stmt) == 2);
                    KE_ASSERT(sqlite3_column_type(stmt, 0) == SQLITE_INTEGER);
                    const KryneEngine::u64 version = sqlite3_column_int64(stmt, 0);
                    if (pipeline->GetVersion() == version)
                    {
                        const KryneEngine::u32 id = sqlite3_column_int(stmt, 1);
                        m_pipelineIds.insert({ id, pipeline });
                        matchedVersions = true;
                    }
                    else
                    {
                        Logger::GetInstance()->LogFormatted(
                            LogSeverity::Info, kLogCategory,
                            "Pipeline '%s' version mismatch, recooking", pipeline->GetName());
                    }
                }
                else
                {
                    Logger::GetInstance()->LogFormatted(
                        LogSeverity::Info, kLogCategory,
                        "No record for pipeline '%s', assume new", pipeline->GetName());
                }
                sqlite3_finalize(stmt);

                if (matchedVersions)
                    continue;

                snprintf(
                    sql,
                    sizeof(sql),
                    "INSERT INTO assetPipelines (name, version) VALUES ('%s', %llu) RETURNING id",
                    pipeline->GetName().data(),
                    pipeline->GetVersion());
                KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);
                KE_VERIFY(sqlite3_step(stmt) == SQLITE_ROW);
                const KryneEngine::u32 id = sqlite3_column_int(stmt, 0);
                m_pipelineIds.insert({ id, pipeline });
                sqlite3_finalize(stmt);
            }
        }

        m_probingThread = std::thread([this]
        {
            KE_ZoneScoped("Initial asset directories probing");

            Logger::GetInstance()->LogFormatted(LogSeverity::Verbose, kLogCategory, "Probing asset directories");
            const auto start = std::chrono::high_resolution_clock::now();
            for (const auto& directory: m_rawAssetDirectories)
            {
                ProbeDirectory(directory);
            }
            const auto end = std::chrono::high_resolution_clock::now();
            Logger::GetInstance()->LogFormatted(
                LogSeverity::Info, kLogCategory,
                "Probing asset directories took %.3f ms",
                static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 1000.f);
        });

        for (auto i = 0; i < m_workThreads.Size(); i++)
            m_workThreads.Init(i, [this] (const KryneEngine::u32 _index) { this->ThreadMain(_index); }, i);
        m_cookingAssetPerThread.resize(m_workThreads.Size(), {});
    }

    void* AssetCooker::RequestSupport(void* _entry, eastl::function<void()> _workFunction)
    {
        const std::unique_lock lock { m_queueMutex };
        auto* request = new SupportWork { static_cast<QueueEntry*>(_entry), eastl::move(_workFunction) };
        m_supportWorkRequests.emplace_back(request);
        m_queueCondition.notify_all();
        return request;
    }

    void AssetCooker::FinalizeSupportRequest(void* _request)
    {
        std::unique_lock lock { m_queueMutex };
        auto* request = static_cast<SupportWork*>(_request);

        auto it = eastl::find(m_supportWorkRequests.begin(), m_supportWorkRequests.end(), request);
        KE_ASSERT(it != m_supportWorkRequests.end());
        m_supportWorkRequests.erase(it);

        while (request->m_supporterCount.load(std::memory_order::acquire) > 0)
        {
            request->m_supporterCondition.wait(lock);
        }
        delete request;
    }

    bool AssetCooker::ShouldCancelCook(void* _entry)
    {
        std::unique_lock lock { m_queueMutex };
        return static_cast<QueueEntry*>(_entry)->m_shouldCancel;
    }

    void AssetCooker::ProbeDirectory(const std::filesystem::path& _path)
    {

        for (std::filesystem::recursive_directory_iterator it(_path); it != std::filesystem::recursive_directory_iterator(); ++it)
        {
            const std::filesystem::directory_entry& entry = *it;
            if (!entry.is_regular_file())
                continue;

            const std::filesystem::path path = canonical(entry.path());

            ProbeInputAsset(path, _path);
        }
    }

    void AssetCooker::ProbeInputAsset(const std::filesystem::path& _asset, const std::filesystem::path& _assetDirectory)
    {
        char sql[2048];
        sqlite3_stmt* stmt;
        std::filesystem::path parent;
        if (!_assetDirectory.empty())
            parent = _assetDirectory;
        else
        {
            for (const auto& directory: m_rawAssetDirectories)
            {
                if (Utils::IsChildDirectory(_asset, directory))
                {
                    parent = directory;
                    break;
                }
            }
            KE_ASSERT(!parent.empty());
        }

        snprintf(sql, sizeof(sql), "SELECT id FROM assets WHERE path = '%s'", _asset.c_str());
        KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);
        if (sqlite3_step(stmt) != SQLITE_ROW)
        {
            sqlite3_finalize(stmt);

            snprintf(sql, sizeof(sql), "INSERT INTO assets (path) VALUES ('%s') RETURNING id", _asset.c_str());
            KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);
            KE_VERIFY(sqlite3_step(stmt) == SQLITE_ROW);
        }
        const KryneEngine::u32 assetId = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        IAssetPipeline* pipeline = FindPipeline(_asset);
        if (pipeline == nullptr)
        {
            // No pipeline for the asset file, clean up all related cooked files, if there are any.

            snprintf(sql, sizeof(sql), "SELECT path FROM cookedAssets WHERE sourceId = %d", assetId);
            KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                std::filesystem::path cookedOutputPath {
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)) + sqlite3_column_bytes(stmt, 0) };
                if (std::filesystem::exists(cookedOutputPath))
                {
                    std::filesystem::remove(cookedOutputPath);
                }
            }
            sqlite3_finalize(stmt);

            snprintf(sql, sizeof(sql), "DELETE FROM cookedAssets WHERE sourceId = %d", assetId);
            KE_VERIFY(m_database->Execute(sql) == SQLITE_OK);

            return;
        }

        const auto assetWriteTime = std::filesystem::last_write_time(_asset);

        snprintf(sql, sizeof(sql), "SELECT id FROM assetPipelines WHERE name = '%s'", pipeline->GetName().data());
        KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);
        KE_VERIFY(sqlite3_step(stmt) == SQLITE_ROW);
        const KryneEngine::u32 pipelineId = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        snprintf(
            sql,
            sizeof(sql),
            "SELECT path, pipeline, pipelineVersion, writeDate FROM cookedAssets WHERE sourceId = %d",
            assetId);
        KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);
        bool upToDate = false, mismatchedPipelineFiles = false;
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            upToDate = true;
            do
            {
                std::filesystem::path cookedOutputPath {
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)) + sqlite3_column_bytes(stmt, 0)
                };

                if (pipelineId != sqlite3_column_int(stmt, 1))
                {
                    mismatchedPipelineFiles = true;
                    upToDate = false;

                    Logger::GetInstance()->LogFormatted(
                        LogSeverity::Verbose, kLogCategory,
                        "Input asset '%s' was cooked with another pipline, will recook",
                        std::filesystem::relative(_asset, parent).c_str());

                    if (std::filesystem::exists(cookedOutputPath))
                        std::filesystem::remove(cookedOutputPath);
                    continue;
                }

                const KryneEngine::u64 pipelineVersion = sqlite3_column_int64(stmt, 2);
                const KryneEngine::u64 cookedAssetExpectedWriteTime = sqlite3_column_int64(stmt, 3);
                if (pipelineVersion != pipeline->GetVersion())
                {
                    upToDate = false;

                    Logger::GetInstance()->LogFormatted(
                        LogSeverity::Verbose, kLogCategory,
                        "Input asset '%s' was cooked with a previous pipeline version, will recook",
                        std::filesystem::relative(_asset, parent).c_str());

                    if (std::filesystem::exists(cookedOutputPath))
                        std::filesystem::remove(cookedOutputPath);
                }
                else if (!std::filesystem::exists(cookedOutputPath))
                {
                    upToDate = false;

                    Logger::GetInstance()->LogFormatted(
                        LogSeverity::Verbose, kLogCategory,
                        "Output asset '%s' is missing, will recook",
                        std::filesystem::relative(cookedOutputPath, m_outputDirectory).c_str());
                }
                else if (std::filesystem::last_write_time(cookedOutputPath) < assetWriteTime)
                {
                    upToDate = false;

                    Logger::GetInstance()->LogFormatted(
                        LogSeverity::Verbose, kLogCategory,
                        "Input asset '%s' modified after last write date of output '%s', will recook",
                        std::filesystem::relative(_asset, parent).c_str(),
                        std::filesystem::relative(cookedOutputPath, m_outputDirectory).c_str());
                }
                else if (std::filesystem::last_write_time(cookedOutputPath).time_since_epoch().count() != cookedAssetExpectedWriteTime)
                {
                    upToDate = false;

                    Logger::GetInstance()->LogFormatted(
                        LogSeverity::Verbose, kLogCategory,
                        "Output asset '%s' write date mismatch, probably modified, will recook",
                        std::filesystem::relative(cookedOutputPath, m_outputDirectory).c_str());
                }
            }
            while (sqlite3_step(stmt) == SQLITE_ROW);
        }
        sqlite3_finalize(stmt);

        if (mismatchedPipelineFiles)
        {
            snprintf(
                sql,
                sizeof(sql),
                "DELETE FROM cookedAssets WHERE sourceId = %d AND pipeline != %d",
                assetId,
                pipelineId);
            KE_VERIFY(m_database->Execute(sql) == SQLITE_OK);
        }

        if (!upToDate)
        {
            if (pipeline == m_archiver.get())
            {
                std::unique_lock lock { m_queueMutex };
                m_archiver->m_pipelineId = pipelineId;
                m_archiver->LoadArchive(
                    parent,
                    std::filesystem::relative(_asset, parent),
                    m_outputDirectory,
                    assetId,
                    true);
            }

            Enqueue(QueueEntry {
                .m_asset = _asset,
                .m_assetDirectory = parent,
                .m_pipeline = pipeline,
                .m_assetId = assetId,
                .m_pipelineId = pipelineId,
            });
        }
    }

    void AssetCooker::ProbeOutputAsset(const std::filesystem::path& _asset)
    {
        {
            std::unique_lock lock { m_queueMutex };
            if (m_cookingAssets.find(_asset) != m_cookingAssets.end())
            {
                return;
            }
        }

        char sql[2048];
        sqlite3_stmt* stmt;
        snprintf(sql, sizeof(sql),
            "SELECT assets.path, cookedAssets.writeDate, assets.id, cookedAssets.pipeline FROM cookedAssets JOIN assets ON assets.id = cookedAssets.sourceId WHERE cookedAssets.path = '%s'",
            _asset.c_str());

        KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const KryneEngine::u64 writeDate = sqlite3_column_int64(stmt, 1);
            if (!std::filesystem::exists(_asset)
                || writeDate != std::filesystem::last_write_time(_asset).time_since_epoch().count())
            {
                const std::filesystem::path input {
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)) + sqlite3_column_bytes(stmt, 0)
                };

                for (const auto& directory: m_rawAssetDirectories)
                {
                    if (std::filesystem::relative(input, directory).begin()->string() == "..")
                        continue;

                    IAssetPipeline* pipeline = FindPipeline(input);
                    KE_ASSERT(pipeline != nullptr);

                    Enqueue(QueueEntry {
                        .m_asset = input,
                        .m_assetDirectory = directory,
                        .m_pipeline = pipeline,
                        .m_assetId = static_cast<KryneEngine::u32>(sqlite3_column_int(stmt, 2)),
                        .m_pipelineId = static_cast<KryneEngine::u32>(sqlite3_column_int(stmt, 3)),
                    });
                }
            }
        }
        sqlite3_finalize(stmt);
    }

    void AssetCooker::OnInputAssetRenamed(
        const std::filesystem::path& _oldPath,
        const std::filesystem::path& _newPath) const
    {
        char sql[2048];
        snprintf(sql, sizeof(sql), "UPDATE assets SET path = '%s' WHERE path = '%s'", _newPath.c_str(), _oldPath.c_str());
        KE_VERIFY(m_database->Execute(sql) == SQLITE_OK);
    }

    void AssetCooker::OnInputAssetDeleted(const std::filesystem::path& _asset)
    {
        KE_ASSERT(!std::filesystem::exists(_asset));

        char sql[2048];
        sqlite3_stmt* stmt;

        snprintf(sql, sizeof(sql), "SELECT id FROM assets WHERE path = '%s'", _asset.c_str());
        KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);

        if (sqlite3_step(stmt) != SQLITE_ROW)
        {
            sqlite3_finalize(stmt);
            return;
        }
        const KryneEngine::u32 assetId = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        snprintf(sql, sizeof(sql), "DELETE FROM cookedAssets WHERE sourceId = %d", assetId);
        KE_VERIFY(m_database->Execute(sql) == SQLITE_OK);

        snprintf(sql, sizeof(sql), "DELETE FROM assets WHERE id = %d", assetId);
        KE_VERIFY(m_database->Execute(sql) == SQLITE_OK);

        if (FindPipeline(_asset) == m_archiver.get())
        {
            const std::unique_lock lock(m_queueMutex);
            m_archiver->m_archiveMap.erase(_asset);
        }
    }

    IAssetPipeline* AssetCooker::FindPipeline(const std::filesystem::path& _asset)
    {
        eastl::string filename = _asset.filename().c_str();
        size_t pos = filename.find_first_of('.');
        while (pos != eastl::string::npos)
        {
            const eastl::string extension = filename.substr(pos);
            const auto pipelineIt = m_pipelineMap.Find(KryneEngine::StringHash(extension));
            if (pipelineIt != m_pipelineMap.end())
            {
                return pipelineIt->second;
            }
            pos = filename.find_first_of('.', pos + 1);
        }
        return nullptr;
    }

    void AssetCooker::ThreadMain(const KryneEngine::u32 _index)
    {
        QueueEntry entry;
        char sql[2048];
        sqlite3_stmt* stmt;

        eastl::fixed_vector<SupportWork*, 16> completedSupportWorkRequests;

        while (!m_stopWork)
        {
            {
                auto queueLock = std::unique_lock(m_queueMutex);
                m_queueCondition.wait(queueLock, [this]
                {
                    return !m_supportWorkRequests.empty() || !m_updateQueue.empty() || !m_archiveQueue.empty() || m_stopWork;
                });
                if (m_stopWork)
                    break;

                if (!m_supportWorkRequests.empty())
                {
                    SupportWork* work = nullptr;

                    for (auto i = 0; i < m_supportWorkRequests.size(); ++i)
                    {
                        // Clean up completed work requests
                        while (completedSupportWorkRequests.size() > i && m_supportWorkRequests[i] != completedSupportWorkRequests[i])
                            completedSupportWorkRequests.erase(completedSupportWorkRequests.begin() + i);

                        if (completedSupportWorkRequests.size() > i && m_supportWorkRequests[i] == completedSupportWorkRequests[i])
                            continue;
                        work = m_supportWorkRequests[i];
                        break;
                    }

                    if (work)
                    {
                        work->m_supporterCount.fetch_add(1, std::memory_order::acq_rel);
                        queueLock.unlock();

                        m_cookingAssetPerThread[_index] = std::filesystem::relative(work->m_entry->m_asset, work->m_entry->m_assetDirectory);
                        work->m_workFunction();
                        m_cookingAssetPerThread[_index] = std::filesystem::path();
                        completedSupportWorkRequests.push_back(work);

                        queueLock.lock();
                        work->m_supporterCount.fetch_sub(1, std::memory_order::acq_rel);
                        work->m_supporterCondition.notify_all();
                        continue;
                    }

                    if (m_updateQueue.empty() && m_archiveQueue.empty())
                        continue;
                }

                if (!m_updateQueue.empty())
                {
                    entry = m_updateQueue.front();
                    m_updateQueue.pop();

                    // If the asset is already cooking, notify the cooker indirectly that the asset will need another
                    // re-cook.
                    // It will force the re-cook, which is not ideal if it was a double request for the same change, but
                    // these are expected to be rare.
                    const auto it = m_cookingAssets.find(entry.m_asset);
                    if (it != m_cookingAssets.end())
                    {
                        it->second++;
                        continue;
                    }
                    m_cookingAssets.emplace(entry.m_asset, 0);
                }
                else
                {
                    // Wait for all assets to be done cooking before launching archive work.
                    if (!m_cookingAssets.empty())
                        continue;

                    entry = m_archiveQueue.back();
                    m_archiveQueue.erase(m_archiveQueue.end() - 1);

                    entry.m_forceCook = true;

                    auto it = m_cookingArchives.find(entry.m_assetId);
                    if (it != m_cookingArchives.end())
                    {
                        it->second->m_shouldCancel = true;
                        it->second = &entry;
                    }
                    else
                    {
                        m_cookingArchives.emplace(entry.m_assetId, &entry);
                    }
                }
            }

            bool upToDate = false;
            if (!entry.m_forceCook)
            {
                const auto assetWriteTime = std::filesystem::last_write_time(entry.m_asset);
                snprintf(
                    sql, sizeof(sql),
                    "SELECT cookedAssets.path, writeDate FROM cookedAssets JOIN assets ON assets.id = cookedAssets.sourceId WHERE assets.path = '%s'",
                    entry.m_asset.c_str());
                KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);
                if (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    upToDate = true;
                    do
                    {
                        const std::filesystem::path cookedOutputPath {
                            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)) + sqlite3_column_bytes(stmt, 0) };

                        if (!std::filesystem::exists(cookedOutputPath))
                        {
                            upToDate = false;
                            continue;
                        }

                        if (std::filesystem::last_write_time(cookedOutputPath) < assetWriteTime)
                        {
                            upToDate = false;
                        }
                        else
                        {
                            const KryneEngine::u64 cookedWriteTime = sqlite3_column_int64(stmt, 1);
                           if (std::filesystem::last_write_time(cookedOutputPath).time_since_epoch().count() != cookedWriteTime)
                           {
                               upToDate = false;
                           }
                        }
                    }
                    while (sqlite3_step(stmt) == SQLITE_ROW);
                }
            }

            const std::filesystem::path relativePath = entry.m_asset.lexically_relative(entry.m_assetDirectory);
            KE_ZoneScopedF("Processing '%s'", relativePath.c_str());

            m_cookingAssetPerThread[_index] = relativePath;

            bool shouldRecook = false;
            do
            {
                IAssetPipeline::CookResult result {};
                if (!upToDate)
                {
                    result = entry.m_pipeline->CookAsset(
                        this,
                        &entry,
                       relativePath.c_str(),
                       entry.m_assetDirectory.c_str(),
                       m_outputDirectory.c_str());
                    if (!result.success)
                    {
                        Logger::GetInstance()->LogFormatted(LogSeverity::Error, kLogCategory,
                           "Failed to cook '%s'", relativePath.c_str());
                        continue;
                    }
                    Logger::GetInstance()->LogFormatted(LogSeverity::Info, kLogCategory,
                    "Cooked asset '%s', with an output of %lld files",
                        relativePath.c_str(), result.m_resultingFiles.size());


                    KE_VERIFY(!result.m_resultingFiles.empty());

                    snprintf(sql, sizeof(sql), "DELETE FROM cookedAssets WHERE sourceId = %d", entry.m_assetId);
                    KE_VERIFY(m_database->Execute(sql) == SQLITE_OK);

                    const KryneEngine::u64 pipelineVersion = entry.m_pipeline->GetVersion();
                    for (const auto& file: result.m_resultingFiles)
                    {
                        const std::filesystem::path cookedOutputPath { m_outputDirectory / file };
                        const KryneEngine::u64 cookedWriteTime = std::filesystem::last_write_time(cookedOutputPath).time_since_epoch().count();
                        snprintf(sql, sizeof(sql), "INSERT INTO cookedAssets (sourceId, path, pipeline, pipelineVersion, writeDate) VALUES (%d, '%s', %d, %llu, %llu)",
                            entry.m_assetId,
                            cookedOutputPath.c_str(),
                            entry.m_pipelineId,
                            pipelineVersion,
                            cookedWriteTime);
                        KE_ASSERT(m_database->Execute(sql) == SQLITE_OK);
                    }
                }

                {
                    const std::unique_lock recookLock(m_queueMutex);

                    if (entry.m_pipeline == m_archiver.get())
                    {
                        auto it = m_cookingArchives.find(entry.m_assetId);
                        if (it->second == &entry)
                            m_cookingArchives.erase(it);
                        continue;
                    }

                    auto it = m_cookingAssets.find(entry.m_asset);

                    if (it->second > 0)
                    {
                        it->second--;
                        shouldRecook = true;
                        upToDate = false;
                    }
                    else
                    {
                        m_cookingAssets.erase(it);
                        shouldRecook = false;

                        if (result.success && !result.m_resultingFiles.empty())
                        {
                            for (auto& file: result.m_resultingFiles)
                            {
                                for (auto& pair: m_archiver->m_archiveMap)
                                {
                                    if (pair.second.BelongsTo(file))
                                    {
                                        m_archiveQueue.insert(QueueEntry {
                                            .m_asset = pair.first,
                                            .m_assetDirectory = pair.second.m_assetDirectory,
                                            .m_pipeline = m_archiver.get(),
                                            .m_assetId = pair.second.m_assetId,
                                            .m_pipelineId = m_archiver->m_pipelineId,
                                            .m_forceCook = true,
                                        });
                                        break;
                                    }
                                }
                            }
                        }

                        if (m_cookingAssets.empty() && !m_archiveQueue.empty())
                        {
                            m_queueCondition.notify_all();
                        }
                    }
                }
            }
            while (shouldRecook);

            m_cookingAssetPerThread[_index] = std::filesystem::path {};
            const auto cookedLock = m_cookedAssetsLock.AutoLock();
            m_cookedAssets.push_back(relativePath);
        }
    }

    void AssetCooker::Enqueue(QueueEntry&& _entry)
    {
        std::unique_lock lock { m_queueMutex };
        if (_entry.m_pipeline == m_archiver.get())
            m_archiveQueue.insert(eastl::move(_entry));
        else
            m_updateQueue.emplace(eastl::move(_entry));
        m_queueCondition.notify_all();
    }
}
