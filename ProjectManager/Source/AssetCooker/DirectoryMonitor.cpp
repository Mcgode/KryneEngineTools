/**
 * @file
 * @author Max Godefroy
 * @date 20/03/2026.
 */

#include "DirectoryMonitor.hpp"

#include <KryneEngine/Core/Memory/DynamicArray.hpp>

#include "ProjectManager/AssetCooker/AssetCooker.hpp"
#include "ProjectManager/Logger/Logger.hpp"

namespace ProjectManager
{
    DirectoryMonitor::DirectoryMonitor(const eastl::span<std::filesystem::path> _directories)
    {
        KryneEngine::DynamicArray<eastl::string_view> directories(_directories.size());
        for (auto i = 0u; i < _directories.size(); ++i)
            directories[i] = _directories[i].c_str();

        const KryneEngine::Platform::DirectoryMonitorCreateInfo createInfo {
            .m_directories = { directories.Data(), directories.Size() },
            .m_threadName = "AssetCookerDirectoryMonitor",
            .m_fileCreatedCallback = [](const eastl::string_view _path)
            {
                char msgBuffer[1024];
                snprintf(msgBuffer, sizeof(msgBuffer), "File created: %s", _path.data());
                Logger::GetInstance()->Log(LogSeverity::Verbose, AssetCooker::kLogCategory, msgBuffer);
            },
            .m_fileModifiedCallback = [](const eastl::string_view _path)
            {
                char msgBuffer[1024];
                snprintf(msgBuffer, sizeof(msgBuffer), "File modified: %s", _path.data());
                Logger::GetInstance()->Log(LogSeverity::Verbose, AssetCooker::kLogCategory, msgBuffer);
            },
            .m_fileRenamedCallback = [](const eastl::string_view _oldPath, const eastl::string_view _newPath)
            {
                char msgBuffer[2048];
                snprintf(msgBuffer, sizeof(msgBuffer), "File renamed: from '%s' to '%s'", _oldPath.data(), _newPath.data());
                Logger::GetInstance()->Log(LogSeverity::Verbose, AssetCooker::kLogCategory, msgBuffer);
            },
            .m_fileDeletedCallback = [](const eastl::string_view _path)
            {
                char msgBuffer[1024];
                snprintf(msgBuffer, sizeof(msgBuffer), "File deleted: %s", _path.data());
                Logger::GetInstance()->Log(LogSeverity::Verbose, AssetCooker::kLogCategory, msgBuffer);
            },
        };

        m_handle = KryneEngine::Platform::CreateDirectoryMonitor(createInfo, {});
    }

    DirectoryMonitor::~DirectoryMonitor()
    {
        KryneEngine::Platform::DestroyDirectoryMonitor(m_handle, {});
    }
} // ProjectManager