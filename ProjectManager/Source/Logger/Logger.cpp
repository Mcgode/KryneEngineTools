/**
 * @file
 * @author Max Godefroy
 * @date 04/03/2026.
 */

#include "ProjectManager/Logger/Logger.hpp"

#include <KryneEngine/Core/Common/Assert.hpp>

#include "ProjectManager/Logger/LogFilter.hpp"

namespace ProjectManager
{
    Logger* Logger::s_instance = nullptr;

    Logger::Logger(KryneEngine::AllocatorInstance _allocator)
        : m_allocator(_allocator)
        , m_categories(_allocator)
        , m_messages(_allocator)
    {
        KE_ASSERT_FATAL(s_instance == nullptr);
        s_instance = this;
    }


    bool Logger::RegisterCategory(KryneEngine::u64 _id, const eastl::string_view& _category)
    {
        const auto lock = m_mutex.AutoLock();

        const auto it = m_categories.find(_id);
        if (it != m_categories.end())
        {
            KE_ASSERT_MSG(_category == it->second, "Conflicting category IDs");
            return false;
        }
        m_categories.emplace(_id, eastl::string { _category.data(), _category.size(), m_allocator });
        return true;
    }

    eastl::string_view Logger::GetCategoryName(const KryneEngine::u64 _id) const
    {
        const auto lock = m_mutex.AutoLock();

        if (_id == 0)
        {
            return "Default";
        }

        const auto it = m_categories.find(_id);
        if (it == m_categories.end())
            return { nullptr, 0 };
        return it->second;
    }

    void Logger::Log(
        const LogSeverity _severity,
        const KryneEngine::u64 _category,
        const eastl::string_view _shortMessage,
        const eastl::string_view _longMessage)
    {
        const auto lock = m_mutex.AutoLock();

        m_messages.emplace_back(Message {
            _severity,
            std::chrono::system_clock::now(),
            _category,
            eastl::string { _shortMessage.data(), _shortMessage.size(), m_allocator },
            eastl::string { _longMessage.data(), _longMessage.size(), m_allocator },
        });
    }

    eastl::vector<Logger::MessageView> Logger::GetMessageViews(
        const LogFilter& _filter,
       const KryneEngine::AllocatorInstance _allocator) const
    {
        const auto lock = m_mutex.AutoLock();

        eastl::vector<MessageView> bundle;
        for (const auto& message : m_messages)
        {
            if (_filter.FilterMessage(message.m_severity, message.m_category))
            {
                bundle.push_back(MessageView {
                    message.m_severity,
                    message.m_time,
                    message.m_category,
                    message.m_shortMessage,
                    message.m_longMessage
                });
            }
        }

        return bundle;
    }

    void Logger::UpdateMessageViews(eastl::vector<MessageView>& _bundle, const LogFilter& _filter) const
    {
        const auto lastRetrievedMessage = m_messages.back();

        const auto lock = m_mutex.AutoLock();

        auto it = eastl::lower_bound(
            m_messages.begin(),
            m_messages.end(),
            lastRetrievedMessage.m_time,
            [](const Message& _message, const std::chrono::system_clock::time_point& _time)
            {
                return _message.m_time < _time;
            });

        // While unlikely, some messages may have been written at the same time, so we need to handle this edge case.
        while (it != m_messages.end() && it->m_time == lastRetrievedMessage.m_time)
        {
            if (it->m_shortMessage.data() == lastRetrievedMessage.m_shortMessage.data())
            {
                ++it;
                break;
            }
            ++it;
        }

        for (; it != m_messages.end(); ++it)
        {
            if (_filter.FilterMessage(it->m_severity, it->m_category))
            {
                _bundle.push_back(MessageView {
                    it->m_severity,
                    it->m_time,
                    it->m_category,
                    it->m_shortMessage,
                    it->m_longMessage
                });
            }
        }
    }
}
