/**
 * @file
 * @author Max Godefroy
 * @date 10/10/2022.
 */

#pragma once

#include <EASTL/unique_ptr.h>
#include <Threads/FibersManager.hpp>
#include <Files/FileWatcher.hpp>

namespace KryneEngine::Tools
{
    class DataCooker
    {
    public:
        explicit DataCooker(const eastl::string_view& _path, u16 _workerCount = 6);

    private:
        FibersManager m_fibersManager;

        eastl::unique_ptr<FileWatcher> m_fileWatcher;
    };
}