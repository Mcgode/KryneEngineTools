/**
 * @file
 * @author Max Godefroy
 * @date 07/04/2026.
 */

#include "ArchivePipeline.hpp"

#include <fstream>
#include <yyjson.h>

#include "ProjectManager/Utils.hpp"
#include "ProjectManager/AssetCooker/AssetCooker.hpp"
#include "ProjectManager/Database/Database.hpp"
#include "ProjectManager/Logger/Logger.hpp"

namespace ProjectManager
{
    IAssetPipeline::CookResult ArchivePipeline::CookAsset(
        AssetCooker* _assetCooker,
        void* _entry,
        eastl::string_view _assetRelativePath,
        eastl::string_view _assetDirectory,
        eastl::string_view _outputDir)
    {
        using namespace KryneEngine::Modules::FileSystem;

        const auto it = m_archiveMap.find(std::filesystem::path(_assetDirectory.data()) / _assetRelativePath.data());
        KE_ASSERT(it != m_archiveMap.end());
        const Archive& archive = it->second;

        char sql[2048];
        sqlite3_stmt* stmt;

        snprintf(sql, sizeof(sql), "SELECT path, pipeline FROM cookedAssets WHERE path LIKE '%s%%'", archive.m_mountPoint.c_str());
        if (m_assetCooker->m_database->Prepare(sql, &stmt) != SQLITE_OK)
        {
            Logger::GetInstance()->LogFormatted(LogSeverity::Error, AssetCooker::kLogCategory, "Failed to retrieve cooked asset list for '%s'", _assetRelativePath.data());
            sqlite3_finalize(stmt);
            return { false };
        }

        eastl::vector<eastl::pair<std::filesystem::path, ArchivingOptions>> assets;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const std::filesystem::path path = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), sqlite3_column_bytes(stmt, 0));
            if (archive.BelongsTo(path))
            {
                const KryneEngine::u32 id = sqlite3_column_int(stmt, 1);
                ArchivingOptions options = m_assetCooker->m_pipelineIds[id]->RetrieveOutputFileArchingOptions(path);
                if (options.m_archivable)
                    assets.emplace_back(path, options);
            }
        }
        sqlite3_finalize(stmt);

        std::filesystem::path outputPath = std::filesystem::path(_outputDir.data()) / _assetRelativePath.data();
        outputPath.replace_extension(".ke_zip");
        std::ofstream archiveFile(outputPath.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        KE_ASSERT(archiveFile);

        const auto relativeMount = std::filesystem::relative(archive.m_mountPoint, m_assetCooker->m_outputDirectory);
        ArchiveMaker archiveMaker(
            archiveFile,
            relativeMount.c_str(),
            assets.size());
        for (auto& assetPair : assets)
        {
            std::ifstream assetFile(assetPair.first.c_str(), std::ios::binary | std::ios::in);
            const auto relativeAsset = std::filesystem::relative(assetPair.first, archive.m_mountPoint);
            const FileFlags flags = assetPair.second.m_compress ? FileFlags::ZstdCompressed : FileFlags::None;
            archiveMaker.AddFile(assetFile, relativeAsset.c_str(), flags);
            assetFile.close();
        }
        archiveMaker.Finish();

        return {
            true,
            { outputPath }
        };
    }

    bool ArchivePipeline::Archive::BelongsTo(const std::filesystem::path& _path) const
    {
        if (Utils::IsChildDirectory(_path, m_mountPoint))
        {
            for (const auto& exclusion : m_exclusions)
            {
                if (Utils::IsChildDirectory(_path, exclusion))
                    return false;
            }
            return true;
        }

        return false;
    }

    void ArchivePipeline::LoadArchive(
        const std::filesystem::path& _parent,
        const std::filesystem::path& _relativePath,
        const std::filesystem::path& _outputDirectory,
        const KryneEngine::u32 _assetId,
        const bool _force)
    {
        const std::filesystem::path fullPath = _parent / _relativePath;

        if (!_force && m_archiveMap.find(fullPath) != m_archiveMap.end())
            return;

        yyjson_read_err err;
        yyjson_doc* doc = yyjson_read_file(fullPath.c_str(), 0, nullptr, &err);
        if (doc == nullptr)
        {
            Logger::GetInstance()->LogFormatted(LogSeverity::Error, AssetCooker::kLogCategory,
                "Failed to load JSON '%s': %s", _relativePath.c_str(), err.msg);
            return;
        }

        yyjson_val* root = yyjson_doc_get_root(doc);

        if (yyjson_get_type(root) != YYJSON_TYPE_OBJ)
        {
            Logger::GetInstance()->LogFormatted(LogSeverity::Error, AssetCooker::kLogCategory,
                "JSON root should be an object (%s)", _relativePath.c_str());
            yyjson_doc_free(doc);
            return;
        }

        Archive archive;

        yyjson_val* directory = yyjson_obj_get(root, "directory");
        if (directory == nullptr || yyjson_get_type(directory) != YYJSON_TYPE_STR)
        {
            Logger::GetInstance()->LogFormatted(LogSeverity::Error, AssetCooker::kLogCategory,
                "JSON root should have a 'directory' field of type string (%s)", _relativePath.c_str());
            yyjson_doc_free(doc);
            return;
        }

        archive.m_mountPoint = std::filesystem::canonical(_outputDirectory / _relativePath.parent_path() / yyjson_get_str(directory));
        archive.m_assetId = _assetId;
        archive.m_assetDirectory = _parent;

        yyjson_val* exclusions = yyjson_obj_get(root, "exclusions");
        if (exclusions != nullptr)
        {
            if (yyjson_get_type(exclusions) != YYJSON_TYPE_ARR)
            {
                Logger::GetInstance()->LogFormatted(LogSeverity::Error, AssetCooker::kLogCategory,
                    "JSON 'exclusions' field should be an array (%s)", _relativePath.c_str());
                yyjson_doc_free(doc);
                return;
            }

            size_t idx, max;
            yyjson_val* entry;
            yyjson_arr_foreach(root, idx, max, entry)
            {
                if (yyjson_get_type(entry) != YYJSON_TYPE_STR)
                {
                    Logger::GetInstance()->LogFormatted(LogSeverity::Error, AssetCooker::kLogCategory,
                        "JSON 'exclusions' array should only contain strings (%s)", _relativePath.c_str());
                    yyjson_doc_free(doc);
                    return;
                }
                archive.m_exclusions.emplace_back(fullPath.parent_path() / yyjson_get_str(entry));
            }
        }

        yyjson_doc_free(doc);

        m_archiveMap.emplace(fullPath, archive);
    }
}
