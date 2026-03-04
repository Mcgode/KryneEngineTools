/**
 * @file
 * @author Max Godefroy
 * @date 04/03/2026.
 */

#include "LogWindow.hpp"

#include <imgui.h>
#include <EASTL/sort.h>

#include "CoreCategory.hpp"
#include "ProjectManager/Logger/Logger.hpp"

namespace ProjectManager
{
    LogWindow::LogWindow(const KryneEngine::AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_logFilter(_allocator)
    {
        m_logFilter.m_categoryWhiteList.emplace(kCoreLogCategory);
    }

    void LogWindow::Render()
    {
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
                            const bool isChecked = m_logFilter.IsSeverityIncluded(severity);
                            if (ImGui::MenuItem(LogSeverityToString(severity), nullptr, isChecked))
                            {
                                if (isChecked)
                                    m_logFilter.ExcludeSeverity(severity);
                                else
                                    m_logFilter.IncludeSeverity(severity);
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
                                m_logFilter.m_categoryWhiteList.emplace(category.first);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Deselect all"))
                        {
                            m_logFilter.m_categoryWhiteList.clear();
                        }

                        for (auto& category : categoriesList)
                        {
                            const bool enabled = m_logFilter.m_categoryWhiteList.find(category.first) != m_logFilter.m_categoryWhiteList.end();
                            if (ImGui::MenuItem(category.second.data(), nullptr, enabled))
                            {
                                if (enabled)
                                    m_logFilter.m_categoryWhiteList.erase(category.first);
                                else
                                    m_logFilter.m_categoryWhiteList.emplace(category.first);
                            }
                        }

                        ImGui::EndMenu();
                    }

                    ImGui::EndMenuBar();
                }

                eastl::vector<Logger::MessageView> messages = Logger::GetInstance()->GetMessageViews(m_logFilter, m_allocator);

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
    }
}
