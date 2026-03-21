/**
 * @file
 * @author Max Godefroy
 * @date 19/03/2026.
 */

#pragma once

#include <EASTL/string.h>
#include <EASTL/vector.h>

namespace ProjectManager
{
    class IAssetPipeline
    {
    public:
        virtual ~IAssetPipeline() = default;

        [[nodiscard]] virtual eastl::string_view GetName() const = 0;

        [[nodiscard]] virtual KryneEngine::u64 GetVersion() const = 0;

        [[nodiscard]] virtual eastl::span<const char*> GetHandledAssetFileExtensions() const = 0;

        struct CookResult
        {
            bool success;
            eastl::vector<std::filesystem::path> m_resultingFiles;
        };

        [[nodiscard]] virtual CookResult CookAsset(
            eastl::string_view _assetRelativePath,
            eastl::string_view _assetDirectory,
            eastl::string_view _outputDir) = 0;
    };
}
