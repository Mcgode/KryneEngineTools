/**
 * @file
 * @author Max Godefroy
 * @date 20/03/2026.
 */

#pragma once

#include <EASTL/string.h>
#include <KryneEngine/Core/Platform/FileSystem.hpp>

namespace ProjectManager
{
    class AssetCooker;

    class DirectoryMonitor
    {
    public:
        explicit DirectoryMonitor(AssetCooker* _assetCooker);

        ~DirectoryMonitor();

    private:
        AssetCooker* m_assetCooker;
        KryneEngine::Platform::DirectoryMonitorHandle m_handle { KryneEngine::Platform::OpaqueHandle(nullptr) };

        [[nodiscard]] bool IsOutputFile(const std::filesystem::path& _path) const;
    };
} // ProjectManager