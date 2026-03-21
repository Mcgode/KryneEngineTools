/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2026.
 */

#include "ProjectManager/AssetCooker/AssetCooker.hpp"

#include <KryneEngine/Core/Common/Assert.hpp>
#include <KryneEngine/Core/Memory/Containers/FlatHashMap.inl>

#include "AssetCooker/DirectoryMonitor.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"
#include "ProjectManager/AssetCooker/IAssetPipeline.hpp"
#include "ProjectManager/Database/Database.hpp"

namespace ProjectManager
{
    AssetCooker::AssetCooker(Database* _database)
        : m_database(_database)
        , m_pipelineMap(KryneEngine::AllocatorInstance {})
    {
        Logger::GetInstance()->RegisterCategory(kLogCategory, "AssetCooker");

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
            const char* sql = "CREATE TABLE cookedAssets (sourceId INTEGER, path TEXT NOT NULL, pipeline INTEGER NOT NULL, pipelineVersion INTEGER NOT NULL)";
            const int result = m_database->Execute(sql);
            KE_ASSERT(result == SQLITE_OK);
        }

        Logger::GetInstance()->Log(LogSeverity::Verbose, kLogCategory, "AssetCooker initialized");

        m_workThreads.Resize(eastl::max(1u, std::thread::hardware_concurrency() - 1));
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
        m_directoryMonitor = eastl::make_unique<DirectoryMonitor>(m_rawAssetDirectories);

        const auto timepoint = std::filesystem::file_time_type::clock::now();
        const KryneEngine::u64 timepointMs = std::chrono::duration_cast<std::chrono::milliseconds>(timepoint.time_since_epoch()).count();

        {
            char sql[2048];
            sqlite3_stmt* stmt;
            for (IAssetPipeline* pipeline : m_pipelines)
            {
                bool matchedVersions = false;
                snprintf(sql, sizeof(sql), "SELECT version FROM assetPipelines WHERE name = '%s'", pipeline->GetName().data());
                KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);
                if (sqlite3_step(stmt) == SQLITE_ROW)
                {
                    KE_ASSERT(sqlite3_column_count(stmt) == 1);
                    KE_ASSERT(sqlite3_column_type(stmt, 0) == SQLITE_INTEGER);
                    const KryneEngine::u64 version = sqlite3_column_int64(stmt, 0);
                    if (pipeline->GetVersion() == version)
                    {
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
                    "INSERT INTO assetPipelines (name, version) VALUES ('%s', %llu)",
                    pipeline->GetName().data(),
                    pipeline->GetVersion());
                KE_VERIFY(m_database->Execute(sql) == SQLITE_OK);
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

        m_workThreads.InitAll([this]
        {
            QueueEntry entry;
            while (!m_stopWork)
            {
                {
                    auto queueLock = std::unique_lock(m_queueMutex);
                    m_queueCondition.wait(queueLock, [this] { return !m_updateQueue.empty() || m_stopWork; });
                    if (m_stopWork)
                        break;

                    entry = m_updateQueue.front();
                    m_updateQueue.pop();
                }

                const std::filesystem::path relativePath = entry.m_asset.lexically_relative(entry.m_assetDirectory);
                KE_ZoneScopedF("Processing '%s'", relativePath.c_str());

                const IAssetPipeline::CookResult result = entry.m_pipeline->CookAsset(
                    relativePath.c_str(),
                    entry.m_assetDirectory.c_str(),
                    m_outputDirectory.c_str());

                if (!result.success)
                    Logger::GetInstance()->LogFormatted(LogSeverity::Error, kLogCategory,
                        "Failed to cook '%s'", relativePath.c_str());
            }
        });
    }

    void AssetCooker::ProbeDirectory(const std::filesystem::path& _path)
    {
        char sql[2048];
        sqlite3_stmt* stmt;

        for (std::filesystem::recursive_directory_iterator it(_path); it != std::filesystem::recursive_directory_iterator(); ++it)
        {
            const std::filesystem::directory_entry& entry = *it;
            if (!entry.is_regular_file())
                continue;

            const std::filesystem::path path = canonical(entry.path());

            snprintf(sql, sizeof(sql), "SELECT id FROM assets WHERE path = '%s'", path.c_str());
            KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);
            if (sqlite3_step(stmt) != SQLITE_ROW)
            {
                sqlite3_finalize(stmt);
                snprintf(
                    sql,
                    sizeof(sql),
                    "INSERT INTO assets (path) VALUES ('%s')",
                    path.c_str());
                KE_VERIFY(m_database->Execute(sql) == SQLITE_OK);
            }
            else
            {
                sqlite3_finalize(stmt);
            }

            IAssetPipeline* pipeline = nullptr;
            {
                eastl::string filename = entry.path().filename().c_str();
                size_t pos = filename.find_first_of('.');
                while (pos != eastl::string::npos)
                {
                    const eastl::string extension = filename.substr(pos);
                    const auto pipelineIt = m_pipelineMap.Find(KryneEngine::StringHash(extension));
                    if (pipelineIt != m_pipelineMap.end())
                    {
                        pipeline = pipelineIt->second;
                        break;
                    }
                    pos = filename.find_first_of('.', pos + 1);
                }
            }
            if (pipeline == nullptr)
            {
                // No pipeline for the asset file, clean up all related cooked files, if there are any.

                snprintf(
                    sql,
                    sizeof(sql),
                    "SELECT cookedAssets.path FROM assets JOIN cookedAssets ON assets.id = cookedAssets.sourceId WHERE assets.path = '%s'",
                    path.c_str());
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

                snprintf(
                    sql, sizeof(sql),
                    "DELETE FROM cookedAssets WHERE sourceId = (SELECT id FROM assets WHERE path = '%s')", path.c_str());
                KE_VERIFY(m_database->Execute(sql) == SQLITE_OK);

                continue;
            }

            {
                auto lock = std::unique_lock(m_queueMutex);
                m_updateQueue.emplace(QueueEntry {
                    path,
                    _path,
                    pipeline,
                });
                m_queueCondition.notify_all();
            }
        }
    }
}
