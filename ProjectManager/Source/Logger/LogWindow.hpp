/**
 * @file
 * @author Max Godefroy
 * @date 04/03/2026.
 */

#pragma once

#include <imgui_internal.h>
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

        void OnImGuiContextStarted() override;

        void Render() override;

    private:
        KryneEngine::AllocatorInstance m_allocator;
        LogFilter m_logFilter;
        bool m_showDate = true;
        bool m_showMilliseconds = true;
        ImGuiSettingsHandler m_settingsHandler;
    };
}
