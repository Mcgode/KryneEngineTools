/**
 * @file
 * @author Max Godefroy
 * @date 04/03/2026.
 */

#include "LogWindow.hpp"

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

    void LogWindow::OnImGuiContextStarted()
    {
        IUiWindow::OnImGuiContextStarted();

        m_settingsHandler.TypeName = "LogWindow";
        m_settingsHandler.TypeHash = ImHashStr("LogWindow");
        m_settingsHandler.ClearAllFn = [](ImGuiContext*, ImGuiSettingsHandler* _handler)
        {
            auto* window = static_cast<LogWindow*>(_handler->UserData);

            window->m_logFilter = LogFilter(window->m_allocator);
            for (const auto& category : Logger::GetInstance()->GetRegisteredCategories(window->m_allocator))
            {
                window->m_logFilter.m_categoryWhiteList.emplace(category.first);
            }

            window->m_showDate = true;
            window->m_showMilliseconds = true;
        };
        m_settingsHandler.ReadInitFn = nullptr;
        m_settingsHandler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* _handler, const char*)
        {
            auto* window = static_cast<LogWindow*>(_handler->UserData);
            window->m_logFilter = LogFilter(window->m_allocator);
            return _handler->UserData;
        };
        m_settingsHandler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler* _handler, void*, const char* _line)
        {
            auto* window = static_cast<LogWindow*>(_handler->UserData);

            KryneEngine::u64 input64;
            if (sscanf(_line, "LogFilter.SeverityWhitelist=0x%llX", &input64)) { window->m_logFilter.m_severityWhiteList = input64; }
            else if (strcmp(_line, "LogFilter.CategoryWhitelist=None") == 0) {}
            else if (sscanf(_line, "LogFilter.CategoryWhitelist=0x%llX,", &input64))
            {
                window->m_logFilter.m_categoryWhiteList.emplace(input64);
                const char* ptr = _line;
                while (*ptr != '\0' && *ptr != '\n')
                {
                    if (*ptr == ',')
                    {
                        ptr++;
                        if (sscanf(ptr, "0x%llX,", &input64))
                        {
                            window->m_logFilter.m_categoryWhiteList.emplace(input64);
                            ptr += 3;
                        }
                    }
                    else ptr++;
                }
            }
            else if (strcmp(_line, "TimePoint.ShowDate=True") == 0)
                window->m_showDate = true;
            else if (strcmp(_line, "TimePoint.ShowDate=False") == 0)
                window->m_showDate = false;
            else if (strcmp(_line, "TimePoint.ShowMilliseconds=True") == 0)
                window->m_showMilliseconds = true;
            else if (strcmp(_line, "TimePoint.ShowMilliseconds=False") == 0)
                window->m_showMilliseconds = false;
        };
        m_settingsHandler.ApplyAllFn = nullptr;
        m_settingsHandler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* _handler, ImGuiTextBuffer* _buf)
        {
            auto* window = static_cast<LogWindow*>(_handler->UserData);

            const size_t reserveSize = sizeof("[LogWindow][Settings]\n")
                + sizeof("LogFilter.SeverityWhitelist=0x") + 10u
                + sizeof("LogFilter.CategoryWhitelist=None") + 12u * window->m_logFilter.m_categoryWhiteList.size()
                + sizeof("TimePoint.ShowDate=False\n")
                + sizeof("TimePoint.ShowMilliseconds=False\n");
            _buf->reserve(static_cast<int>(reserveSize));

            _buf->append("[LogWindow][Settings]\n");
            _buf->appendf("LogFilter.SeverityWhitelist=0x%llX\n", window->m_logFilter.m_severityWhiteList);
            _buf->appendf("LogFilter.CategoryWhitelist=");
            if (window->m_logFilter.m_categoryWhiteList.empty())
                _buf->append("None");
            else
            {
                for (const auto& category : window->m_logFilter.m_categoryWhiteList)
                {
                    _buf->appendf("0x%llX,", category);
                }
            }
            _buf->append("\n");
            _buf->appendf("TimePoint.ShowDate=%s\n", window->m_showDate ? "True" : "False");
            _buf->appendf("TimePoint.ShowMilliseconds=%s\n", window->m_showMilliseconds ? "True" : "False");
        };
        m_settingsHandler.UserData = this;
        ImGui::AddSettingsHandler(&m_settingsHandler);
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

                    if (ImGui::BeginMenu("Options"))
                    {
                        if (ImGui::MenuItem("Show date", nullptr, m_showDate))
                            m_showDate = !m_showDate;
                        if (ImGui::MenuItem("Show milliseconds", nullptr, m_showMilliseconds))
                            m_showMilliseconds = !m_showMilliseconds;
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
                            if (m_showDate)
                                std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localTime);
                            else
                                std::strftime(buffer, sizeof(buffer), "%H:%M:%S", localTime);
                            if (m_showMilliseconds)
                                ImGui::Text("%s.%03lld", buffer, std::chrono::duration_cast<std::chrono::milliseconds>(message.m_time.time_since_epoch()).count() % 1000);
                            else
                                ImGui::Text("%s", buffer);

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
