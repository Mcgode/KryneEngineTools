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

        [[nodiscard]] virtual bool CookAsset(eastl::string_view _assetPath) = 0;
    };
}
