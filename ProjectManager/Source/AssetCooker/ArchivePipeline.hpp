/**
 * @file
 * @author Max Godefroy
 * @date 07/04/2026.
 */

#pragma once

#include <filesystem>
#include <EASTL/span.h>
#include <EASTL/vector_map.h>
#include <KryneEngine/Core/Common/Types.hpp>
#include <KryneEngine/Modules/FileSystem/Archive.hpp>

#include "ProjectManager/AssetCooker/IAssetPipeline.hpp"

namespace ProjectManager
{
    class Database;

    class ArchivePipeline final: public IAssetPipeline
    {
        friend AssetCooker;

    public:
        [[nodiscard]] eastl::string_view GetName() const override
        {
            return "ArchivePipeline";
        }

        [[nodiscard]] KryneEngine::u64 GetVersion() const override
        {
            return static_cast<KryneEngine::u64>(KryneEngine::Modules::FileSystem::Archive::kVersion);
        }

        [[nodiscard]] eastl::span<const char*> GetHandledAssetFileExtensions() const override
        {
            static const char* extensions[] = { ".archive.json" };
            return extensions;
        }

        [[nodiscard]] ArchivingOptions RetrieveOutputFileArchingOptions(const std::filesystem::path& _outputFile) const override
        {
            return { false, false };
        }

        [[nodiscard]] CookResult CookAsset(
            AssetCooker* _assetCooker,
            void* _entry,
            eastl::string_view _assetRelativePath,
            eastl::string_view _assetDirectory,
            eastl::string_view _outputDir) override;

    private:
        struct Archive
        {
            std::filesystem::path m_mountPoint;
            std::filesystem::path m_assetDirectory;
            KryneEngine::u32 m_assetId;
            eastl::vector<std::filesystem::path> m_exclusions;

            [[nodiscard]] bool BelongsTo(const std::filesystem::path& _path) const;
        };

        eastl::vector_map<std::filesystem::path, Archive> m_archiveMap;
        KryneEngine::u32 m_pipelineId;
        AssetCooker* m_assetCooker;

        void LoadArchive(
            const std::filesystem::path& _parent,
            const std::filesystem::path& _relativePath,
            const std::filesystem::path& _outputDirectory,
            KryneEngine::u32 _assetId,
            bool _force);
    };
}
