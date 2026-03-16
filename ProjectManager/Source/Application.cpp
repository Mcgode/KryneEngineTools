/**
 * @file
 * @author Max Godefroy
 * @date 01/03/2026.
 */

#include "ProjectManager/Application.hpp"

#include <imgui_internal.h>
#include <KryneEngine/Core/Graphics/GraphicsContext.hpp>
#include <KryneEngine/Core/Graphics/RenderPass.hpp>
#include <KryneEngine/Core/Window/Window.hpp>
#include <KryneEngine/Modules/ImGui/Context.hpp>

#include "Logger/CoreCategory.hpp"
#include "Logger/LogWindow.hpp"
#include "ProjectManager/IUiWindow.hpp"
#include "ProjectManager/Logger/LogFilter.hpp"
#include "ProjectManager/Logger/Logger.hpp"

namespace ProjectManager
{
    Application::Application(const KryneEngine::AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_rtvs(_allocator)
        , m_renderPasses(_allocator)
        , m_uiWindows(_allocator)
    {
        m_logger = eastl::make_unique<Logger>(_allocator);
        m_applicationInfo.m_applicationName.set_allocator(_allocator);

        m_applicationInfo.m_applicationVersion = { 0, 1, 0 };
        m_applicationInfo.m_engineVersion = { 0, 1, 0 };

        m_applicationInfo.m_displayOptions.m_resizableWindow = true;

#if defined(KE_GRAPHICS_API_VK)
        m_applicationInfo.m_api = KryneEngine::GraphicsCommon::Api::Vulkan_1_0;
#elif defined(KE_GRAPHICS_API_DX12)
        m_applicationInfo.m_api = KryneEngine::GraphicsCommon::Api::DirectX12_0;
#elif defined(KE_GRAPHICS_API_MTL)
        m_applicationInfo.m_api = KryneEngine::GraphicsCommon::Api::Metal_3;
#endif

        m_logWindow = eastl::make_unique<LogWindow>(_allocator);
        m_uiWindows.push_back(m_logWindow.get());

        Logger::GetInstance()->RegisterCategory(kCoreLogCategory, "ProjectManagerCore");

        Logger::GetInstance()->Log(LogSeverity::Verbose, kCoreLogCategory, "Application setup started");
    }

    Application::~Application()
    {
        Logger::GetInstance()->Log(LogSeverity::Debug, kCoreLogCategory, "Application shutting down");

        m_imguiContext->Shutdown(m_window.get());
        m_imguiContext.reset();

        for (const auto& renderPass : m_renderPasses)
            m_window->GetGraphicsContext()->DestroyRenderPass(renderPass);

        m_window.reset();
    }

    void Application::RegisterUiWindow(IUiWindow* _window)
    {
        m_uiWindows.push_back(_window);
    }

    void Application::Run()
    {
        Logger::GetInstance()->Log(LogSeverity::Debug, kCoreLogCategory, "Begin finalizing setup");

        m_window = eastl::make_unique<KryneEngine::Window>(m_applicationInfo, m_allocator);
        KryneEngine::GraphicsContext* graphicsContext = m_window->GetGraphicsContext();

        m_rtvs.Resize(graphicsContext->GetFrameContextCount());
        m_renderPasses.Resize(graphicsContext->GetFrameContextCount());
        for (auto i = 0u; i < graphicsContext->GetFrameContextCount(); i++)
        {
            m_rtvs[i] = graphicsContext->GetPresentRenderTargetView(i);
            m_renderPasses[i] = graphicsContext->CreateRenderPass({
                .m_colorAttachments = {
                    KryneEngine::RenderPassDesc::Attachment {
                        .m_loadOperation = KryneEngine::RenderPassDesc::Attachment::LoadOperation::Clear,
                        .m_storeOperation = KryneEngine::RenderPassDesc::Attachment::StoreOperation::Store,
                        .m_finalLayout = KryneEngine::TextureLayout::Present,
                        .m_rtv = graphicsContext->GetPresentRenderTargetView(i),
                    }
                },
            });
        }

        m_imguiContext = eastl::make_unique<KryneEngine::Modules::ImGui::Context>(
            m_window.get(),
            m_renderPasses[0],
            m_allocator);
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImFont* baseFont = ImGui::GetIO().Fonts->AddFontDefaultVector();
        ImFont* bigFont = ImGui::GetIO().Fonts->AddFontDefaultVector();
        bigFont->Scale = 3.0f;

        for (auto* uiWindow : m_uiWindows)
            uiWindow->OnImGuiContextStarted();

        Logger::GetInstance()->Log(LogSeverity::Verbose, kCoreLogCategory, "Setup finalized");

        for (auto i = 0; i < 100; i++)
        {
            char buffer[40];
            snprintf(buffer, sizeof(buffer), "Hello world %d", i);
            Logger::GetInstance()->Log(LogSeverity::Debug, kCoreLogCategory, buffer);
        }

        do
        {
            if (m_window->ShouldResizeSwapChain())
            {
                if (m_window->GetGraphicsContext()->ResizeSwapChain(m_window.get()))
                {
                    m_window->NotifySwapChainResized();
                }
            }

            const KryneEngine::u8 swapChainIdx = m_window->GetGraphicsContext()->GetCurrentPresentImageIndex();
            if (m_rtvs[swapChainIdx] != graphicsContext->GetPresentRenderTargetView(swapChainIdx))
            {
                m_rtvs[swapChainIdx] = graphicsContext->GetPresentRenderTargetView(swapChainIdx);
                // TODO: handle delayed destruction
                // graphicsContext->DestroyRenderPass(m_renderPasses[swapChainIdx]);
                m_renderPasses[swapChainIdx] = graphicsContext->CreateRenderPass({
                    .m_colorAttachments = {
                        KryneEngine::RenderPassDesc::Attachment {
                            .m_loadOperation = KryneEngine::RenderPassDesc::Attachment::LoadOperation::Clear,
                            .m_storeOperation = KryneEngine::RenderPassDesc::Attachment::StoreOperation::Store,
                            .m_finalLayout = KryneEngine::TextureLayout::Present,
                            .m_rtv = m_rtvs[swapChainIdx],
                        }
                    },
                });
            }

            m_imguiContext->NewFrame(m_window.get());

            KryneEngine::CommandListHandle transfer = graphicsContext->BeginGraphicsCommandList();
            KryneEngine::CommandListHandle graphics = graphicsContext->BeginGraphicsCommandList();

            const KryneEngine::RenderPassHandle renderPass = m_renderPasses[swapChainIdx];

            graphicsContext->BeginRenderPass(graphics, renderPass);

            const ImGuiID dockSpaceId = ImGui::GetID("DockSpace");
            const ImGuiViewport* viewport = ImGui::GetMainViewport();

            if (ImGui::DockBuilderGetNode(dockSpaceId) == nullptr)
            {
                ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockSpaceId, viewport->Size);

                ImGuiID mainDockId = 0;
                ImGuiID logDockId = 0;
                ImGui::DockBuilderSplitNode(dockSpaceId, ImGuiDir_Up, 0.75f, &mainDockId, &logDockId);

                ImGui::DockBuilderDockWindow("Main", mainDockId);
                ImGui::DockBuilderDockWindow("Log", logDockId);
                ImGui::DockBuilderFinish(dockSpaceId);
            }

            ImGui::DockSpaceOverViewport(dockSpaceId, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

            if (ImGui::Begin("Main"))
            {
                ImGui::Spacing();
                ImGui::PushFont(bigFont);
                ImGui::Text("Hello, world!");
                ImGui::Text("Welcome to Liberators project Manager!");
                ImGui::PopFont();
                ImGui::Spacing();
                ImGui::End();
            }

            for (auto* uiWindow : m_uiWindows)
                uiWindow->Render();

            m_imguiContext->PrepareToRenderFrame(graphicsContext, transfer);
            m_imguiContext->RenderFrame(graphicsContext, graphics);

            graphicsContext->EndRenderPass(graphics);

            graphicsContext->EndGraphicsCommandList(transfer);
            graphicsContext->EndGraphicsCommandList(graphics);
        }
        while (graphicsContext->EndFrame());
    }
}
