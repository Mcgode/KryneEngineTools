/**
 * @file
 * @author Max Godefroy
 * @date 04/03/2026.
 */

#pragma once

#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>

#include "ProjectManager/IUiWindow.hpp"
#include "ProjectManager/Logger/LogFilter.hpp"

namespace ProjectManager
{
    class LogWindow: public IUiWindow
    {
    public:
        explicit LogWindow(KryneEngine::AllocatorInstance _allocator);
        ~LogWindow() override = default;

        void Render() override;

    private:
        KryneEngine::AllocatorInstance m_allocator;
        LogFilter m_logFilter;
    };
}
