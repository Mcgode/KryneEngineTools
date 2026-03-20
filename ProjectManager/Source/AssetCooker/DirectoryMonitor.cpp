/**
 * @file
 * @author Max Godefroy
 * @date 20/03/2026.
 */

#include "DirectoryMonitor.hpp"

#include "KryneEngine/Core/Memory/DynamicArray.hpp"

namespace ProjectManager
{
    DirectoryMonitor::DirectoryMonitor(eastl::span<eastl::string> _directories)
    {
        KryneEngine::DynamicArray<eastl::string_view> directories(_directories.size());
        for (auto i = 0u; i < _directories.size(); ++i)
            directories[i] = _directories[i];

        const KryneEngine::Platform::DirectoryMonitorCreateInfo createInfo {
            .m_directories = { directories.Data(), directories.Size() },
            .m_threadName = "AssetCookerDirectoryMonitor"
        };

        m_handle = KryneEngine::Platform::CreateDirectoryMonitor(createInfo, {});
    }

    DirectoryMonitor::~DirectoryMonitor()
    {
        KryneEngine::Platform::DestroyDirectoryMonitor(m_handle, {});
    }
} // ProjectManager