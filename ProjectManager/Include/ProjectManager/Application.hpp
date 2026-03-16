/**
 * @file
 * @author Max Godefroy
 * @date 01/03/2026.
 */

#pragma once

#include <EASTL/unique_ptr.h>
#include <KryneEngine/Core/Graphics/GraphicsCommon.hpp>
#include <KryneEngine/Core/Memory/DynamicArray.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>

namespace KryneEngine
{
    struct RenderTargetViewHandle;
    struct RenderPassHandle;
    class Window;

    namespace Modules::ImGui
    {
        class Context;
    }
}

namespace ProjectManager
{
    class LogWindow;
    class IUiWindow;
    class Logger;

    class Application
    {
    public:
        explicit Application(KryneEngine::AllocatorInstance _allocator = {});
        ~Application();

        void SetName(const eastl::string_view _name) { m_applicationInfo.m_applicationName = _name; }

        void RegisterUiWindow(IUiWindow* _window);

        /**
         * @brief Runs the application. Call this once you are done with the setup.
         */
        void Run();

    private:
        KryneEngine::AllocatorInstance m_allocator;
        eastl::unique_ptr<Logger> m_logger;
        KryneEngine::GraphicsCommon::ApplicationInfo m_applicationInfo {};
        KryneEngine::DynamicArray<KryneEngine::RenderTargetViewHandle> m_rtvs;
        KryneEngine::DynamicArray<KryneEngine::RenderPassHandle> m_renderPasses;
        eastl::unique_ptr<KryneEngine::Window> m_window {};
        eastl::unique_ptr<KryneEngine::Modules::ImGui::Context> m_imguiContext {};

        eastl::vector<IUiWindow*> m_uiWindows;
        eastl::unique_ptr<LogWindow> m_logWindow;
    };
}
