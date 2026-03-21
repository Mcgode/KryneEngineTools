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
    class DirectoryMonitor
    {
    public:
        explicit DirectoryMonitor(eastl::span<std::filesystem::path> _directories);

        ~DirectoryMonitor();

    private:
        KryneEngine::Platform::DirectoryMonitorHandle m_handle { KryneEngine::Platform::OpaqueHandle(nullptr) };
    };
} // ProjectManager