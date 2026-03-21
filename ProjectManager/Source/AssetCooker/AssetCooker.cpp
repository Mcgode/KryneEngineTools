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
            const char* sql = "CREATE TABLE assets (id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT NOT NULL, lastUpdate BIGINT NOT NULL)";
            const int result = m_database->Execute(sql);
            KE_ASSERT(result == SQLITE_OK);
        }

        if (!m_database->TableExists("assetPipelines"))
        {
            Logger::GetInstance()->Log(LogSeverity::Info, kLogCategory, "Creating AssetCooker 'assetPipelines' database table");
            const char* sql = "CREATE TABLE assetPipelines (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, lastUpdate BIGINT NOT NULL, version BIGINT NOT NULL)";
            const int result = m_database->Execute(sql);
            KE_ASSERT(result == SQLITE_OK);
        }

        Logger::GetInstance()->Log(LogSeverity::Verbose, kLogCategory, "AssetCooker initialized");
    }

    AssetCooker::~AssetCooker()
    {
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

        KE_ASSERT(!m_running);

        if (!std::filesystem::exists(std::filesystem::path(_directory.data())))
            return false;

        const eastl::string directory { _directory };
        if (eastl::find(m_rawAssetDirectories.begin(), m_rawAssetDirectories.end(), directory) != m_rawAssetDirectories.end())
            return false;
        m_rawAssetDirectories.emplace_back(eastl::move(directory));
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

        eastl::vector_set<IAssetPipeline*> dirtyPipelines;
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

                dirtyPipelines.emplace(pipeline);

                snprintf(
                    sql,
                    sizeof(sql),
                    "INSERT INTO assetPipelines (name, lastUpdate, version) VALUES ('%s', %llu, %llu)",
                    pipeline->GetName().data(),
                    timepointMs,
                    pipeline->GetVersion());
                KE_VERIFY(m_database->Execute(sql) == SQLITE_OK);
            }
        }

        m_probingThread = std::thread([this](eastl::vector_set<IAssetPipeline*>&& _dirtyPipelines, const KryneEngine::u64 _timepointMs)
        {
            KE_ZoneScoped("Initial asset directories probing");

            Logger::GetInstance()->LogFormatted(LogSeverity::Verbose, kLogCategory, "Probing asset directories");
            const auto start = std::chrono::high_resolution_clock::now();
            for (auto directory: m_rawAssetDirectories)
            {
                ProbeDirectory({ directory.data() }, _dirtyPipelines, _timepointMs);
            }
            const auto end = std::chrono::high_resolution_clock::now();
            Logger::GetInstance()->LogFormatted(
                LogSeverity::Info, kLogCategory,
                "Probing asset directories took %.3f ms",
                static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 1000.f);
        }, eastl::move(dirtyPipelines), timepointMs);
    }

    void AssetCooker::ProbeDirectory(
        const std::filesystem::path& _path,
        const eastl::vector_set<IAssetPipeline*>& _dirtyPipelines,
        const KryneEngine::u64 _timepointMs)
    {
        char sql[2048];
        sqlite3_stmt* stmt;

        for (std::filesystem::recursive_directory_iterator it(_path); it != std::filesystem::recursive_directory_iterator(); ++it)
        {
            const std::filesystem::directory_entry& entry = *it;
            if (!entry.is_regular_file())
                continue;

            const std::filesystem::path path = canonical(entry.path());

            KryneEngine::u64 lastUpdate = 0;
            snprintf(sql, sizeof(sql), "SELECT lastUpdate FROM assets WHERE path = '%s'", path.c_str());
            KE_VERIFY(m_database->Prepare(sql, &stmt) == SQLITE_OK);
            if (sqlite3_step(stmt) != SQLITE_ROW)
            {
                sqlite3_finalize(stmt);
                snprintf(
                    sql,
                    sizeof(sql),
                    "INSERT INTO assets (path, lastUpdate) VALUES ('%s', %llu)",
                    path.c_str(),
                    _timepointMs);
                KE_VERIFY(m_database->Execute(sql) == SQLITE_OK);
            }
            else
            {
                lastUpdate = sqlite3_column_int64(stmt, 0);
                sqlite3_finalize(stmt);
            }

            const eastl::string extension = path.extension().c_str();
            const KryneEngine::StringHash extensionHash { extension.c_str() };
            const auto pipelineIt = m_pipelineMap.Find(extensionHash);
            if (pipelineIt == m_pipelineMap.end())
                continue;

            const KryneEngine::u64 timePoint = std::chrono::duration_cast<std::chrono::milliseconds>(entry.last_write_time().time_since_epoch()).count();
            const auto dirtyIt = _dirtyPipelines.find(pipelineIt->second);

            if (dirtyIt == _dirtyPipelines.end() && lastUpdate < timePoint)
                continue;

            m_updateQueue.emplace(path.c_str(), pipelineIt->second);
        }
    }
}
