/**
 * @file
 * @author Max Godefroy
 * @date 20/03/2026.
 */

#include "DirectoryMonitor.hpp"

#include <KryneEngine/Core/Memory/DynamicArray.hpp>

#include "ProjectManager/Utils.hpp"
#include "ProjectManager/AssetCooker/AssetCooker.hpp"
#include "ProjectManager/Logger/Logger.hpp"

namespace ProjectManager
{
    DirectoryMonitor::DirectoryMonitor(AssetCooker* _assetCooker)
        : m_assetCooker(_assetCooker)
    {
        KryneEngine::DynamicArray<eastl::string_view> directories(_assetCooker->m_rawAssetDirectories.size() + 1);
        for (auto i = 0u; i < m_assetCooker->m_rawAssetDirectories.size(); ++i)
            directories[i] = m_assetCooker->m_rawAssetDirectories[i].c_str();
        directories[directories.Size() - 1] = m_assetCooker->m_outputDirectory.c_str();

        const KryneEngine::Platform::DirectoryMonitorCreateInfo createInfo {
            .m_directories = { directories.Data(), directories.Size() },
            .m_threadName = "AssetCookerDirectoryMonitor",
            .m_fileCreatedCallback = [this](const eastl::string_view _path)
            {
                char msgBuffer[1024];
                snprintf(msgBuffer, sizeof(msgBuffer), "File created: %s", _path.data());
                Logger::GetInstance()->Log(LogSeverity::Verbose, AssetCooker::kLogCategory, msgBuffer);

                const std::filesystem::path path { _path.data() };
                // Ignore created files in output directory
                if (!IsOutputFile(path))
                {
                    m_assetCooker->ProbeInputAsset(path);
                }
            },
            .m_fileModifiedCallback = [this](const eastl::string_view _path)
            {
                char msgBuffer[1024];
                snprintf(msgBuffer, sizeof(msgBuffer), "File modified: '%s'", _path.data());
                Logger::GetInstance()->Log(LogSeverity::Verbose, AssetCooker::kLogCategory, msgBuffer);

                const std::filesystem::path path { _path.data() };
                if (IsOutputFile(path))
                {
                    m_assetCooker->ProbeOutputAsset(path);
                }
                else
                {
                    m_assetCooker->ProbeInputAsset(path);
                }
            },
            .m_fileRenamedCallback = [this](const eastl::string_view _oldPath, const eastl::string_view _newPath)
            {
                char msgBuffer[2048];
                snprintf(msgBuffer, sizeof(msgBuffer), "File renamed: from '%s' to '%s'", _oldPath.data(), _newPath.data());
                Logger::GetInstance()->Log(LogSeverity::Verbose, AssetCooker::kLogCategory, msgBuffer);

                const std::filesystem::path oldPath { _oldPath.data() };
                const std::filesystem::path newPath { _newPath.data() };

                if (IsOutputFile(oldPath))
                {
                    m_assetCooker->ProbeOutputAsset(oldPath);
                }
                else
                {
                    if (oldPath.parent_path() == newPath.parent_path())
                    {
                        m_assetCooker->OnInputAssetRenamed(oldPath, newPath);
                    }
                    else
                    {
                        m_assetCooker->OnInputAssetDeleted(oldPath);
                        m_assetCooker->ProbeInputAsset(newPath);
                    }
                }
            },
            .m_fileDeletedCallback = [this](const eastl::string_view _path)
            {
                char msgBuffer[1024];
                snprintf(msgBuffer, sizeof(msgBuffer), "File deleted: %s", _path.data());
                Logger::GetInstance()->Log(LogSeverity::Verbose, AssetCooker::kLogCategory, msgBuffer);

                const std::filesystem::path path { _path.data() };
                if (IsOutputFile(path))
                {
                    m_assetCooker->ProbeOutputAsset(path);
                }
                else
                {
                    m_assetCooker->OnInputAssetDeleted(path);
                }
            },
        };

        m_handle = KryneEngine::Platform::CreateDirectoryMonitor(createInfo, {});
    }

    DirectoryMonitor::~DirectoryMonitor()
    {
        KryneEngine::Platform::DestroyDirectoryMonitor(m_handle, {});
    }

    bool DirectoryMonitor::IsOutputFile(const std::filesystem::path& _path) const
    {
        return Utils::IsChildDirectory(_path, m_assetCooker->m_outputDirectory);
    }
} // ProjectManager