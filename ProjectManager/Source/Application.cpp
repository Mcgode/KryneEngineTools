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

#include "EASTL/sort.h"
#include "Logger/CoreCategory.hpp"
#include "ProjectManager/Logger/LogFilter.hpp"
#include "ProjectManager/Logger/Logger.hpp"

namespace ProjectManager
{
    Application::Application(const KryneEngine::AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_renderPasses(_allocator)
    {
        m_logger = eastl::make_unique<Logger>(_allocator);
        m_applicationInfo.m_applicationName.set_allocator(_allocator);

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

    void Application::Run()
    {
        Logger::GetInstance()->Log(LogSeverity::Debug, kCoreLogCategory, "Begin finalizing setup");

        m_window = eastl::make_unique<KryneEngine::Window>(m_applicationInfo, m_allocator);
        KryneEngine::GraphicsContext* graphicsContext = m_window->GetGraphicsContext();

        m_renderPasses.Resize(graphicsContext->GetFrameContextCount());
        for (auto i = 0u; i < graphicsContext->GetFrameContextCount(); i++)
        {
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

        Logger::GetInstance()->Log(LogSeverity::Verbose, kCoreLogCategory, "Setup finalized");

        for (auto i = 0; i < 100; i++)
        {
            char buffer[40];
            snprintf(buffer, sizeof(buffer), "Hello world %d", i);
            Logger::GetInstance()->Log(LogSeverity::Debug, kCoreLogCategory, buffer);
        }

        LogFilter logFilter(m_allocator);
        logFilter.m_categoryWhiteList.emplace(kCoreLogCategory);

        do
        {
            m_imguiContext->NewFrame(m_window.get());

            KryneEngine::CommandListHandle transfer = graphicsContext->BeginGraphicsCommandList();
            KryneEngine::CommandListHandle graphics = graphicsContext->BeginGraphicsCommandList();

            const KryneEngine::RenderPassHandle renderPass = m_renderPasses[graphicsContext->GetCurrentPresentImageIndex()];

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

            if (ImGui::Begin("Log", nullptr, ImGuiWindowFlags_MenuBar))
            {
                auto categories = Logger::GetInstance()->GetRegisteredCategories(m_allocator);

                if (ImGui::BeginMenuBar())
                {
                    if (ImGui::BeginMenu("Severity"))
                    {
                        for (auto i = 0; i < static_cast<unsigned>(LogSeverity::COUNT); i++)
                        {
                            const auto severity = static_cast<LogSeverity>(i);
                            const bool isChecked = logFilter.IsSeverityIncluded(severity);
                            if (ImGui::MenuItem(LogSeverityToString(severity), nullptr, isChecked))
                            {
                                if (isChecked)
                                    logFilter.ExcludeSeverity(severity);
                                else
                                    logFilter.IncludeSeverity(severity);
                            }
                        }

                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Categories"))
                    {
                        eastl::vector categoriesList = categories;
                        eastl::sort(categoriesList.begin(), categoriesList.end(), [](const auto& _a, const auto& _b) {
                            return _a.second < _b.second;
                        });

                        if (ImGui::Button("Select all"))
                        {
                            for (const auto& category : categoriesList)
                                logFilter.m_categoryWhiteList.emplace(category.first);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Deselect all"))
                        {
                            logFilter.m_categoryWhiteList.clear();
                        }

                        for (auto& category : categoriesList)
                        {
                            const bool enabled = logFilter.m_categoryWhiteList.find(category.first) != logFilter.m_categoryWhiteList.end();
                            if (ImGui::MenuItem(category.second.data(), nullptr, enabled))
                            {
                                if (enabled)
                                    logFilter.m_categoryWhiteList.erase(category.first);
                                else
                                    logFilter.m_categoryWhiteList.emplace(category.first);
                            }
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::EndMenuBar();
                }

                eastl::vector<Logger::MessageView> messages = m_logger->GetMessageViews(logFilter, m_allocator);

                if (ImGui::BeginTable("logTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
                {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 100);
                    ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 100);
                    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 100);
                    ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(messages.size()));
                    while (clipper.Step())
                    {
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                        {
                            const Logger::MessageView& message = messages[row];
                            ImGui::TableNextRow();

                            ImGui::TableSetColumnIndex(0);
                            const std::time_t absoluteTime = std::chrono::system_clock::to_time_t(message.m_time);
                            const std::tm* localTime = std::localtime(&absoluteTime);
                            char buffer[100];
                            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localTime);
                            ImGui::Text("%s.%lld", buffer, std::chrono::duration_cast<std::chrono::milliseconds>(message.m_time.time_since_epoch()).count() % 1000);

                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%s", LogSeverityToString(message.m_severity));

                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%s", categories.find(message.m_category)->second.data());

                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%s", message.m_shortMessage.data());
                        }
                    }

                    ImGui::EndTable();
                }

                ImGui::End();
            }

            m_imguiContext->PrepareToRenderFrame(graphicsContext, transfer);
            m_imguiContext->RenderFrame(graphicsContext, graphics);

            graphicsContext->EndRenderPass(graphics);

            graphicsContext->EndGraphicsCommandList(transfer);
            graphicsContext->EndGraphicsCommandList(graphics);
        }
        while (graphicsContext->EndFrame());
    }
}
