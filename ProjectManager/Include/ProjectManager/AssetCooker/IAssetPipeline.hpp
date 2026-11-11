/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2026.
 */

#pragma once

#include <filesystem>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/span.h>
#include <KryneEngine/Core/Common/Types.hpp>

namespace ProjectManager
{
    class AssetCooker;

    class IAssetPipeline
    {
    public:
        virtual ~IAssetPipeline() = default;

        [[nodiscard]] virtual eastl::string_view GetName() const = 0;

        [[nodiscard]] virtual KryneEngine::u64 GetVersion() const = 0;

        [[nodiscard]] virtual eastl::span<const char*> GetHandledAssetFileExtensions() const = 0;

        struct ArchivingOptions
        {
            bool m_archivable = true;
            bool m_compress = true;
        };

        [[nodiscard]] virtual ArchivingOptions RetrieveOutputFileArchingOptions(const std::filesystem::path& _outputFile) const = 0;

        struct CookResult
        {
            bool success;
            eastl::vector<std::filesystem::path> m_resultingFiles;

            /**
             * @brief Absolute paths of the files other than the asset itself that the cook read.
             * @details A change to one of them recooks the asset, so does a missing one. They are recorded after a
             * successful cook, and after a failed one that reports any, so fixing a dependency recooks a failed asset.
             * A dependency may be outside of the raw asset directories, but then only startup probing notices its
             * changes.
             */
            eastl::vector<std::filesystem::path> m_dependencies;
        };

        [[nodiscard]] virtual CookResult CookAsset(
            AssetCooker* _assetCooker,
            void* _entry,
            eastl::string_view _assetRelativePath,
            eastl::string_view _assetDirectory,
            eastl::string_view _outputDir) = 0;
    };
}
