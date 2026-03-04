/**
 * @file
 * @author Max Godefroy
 * @date 04/03/2026.
 */

#pragma once

#include <EASTL/vector_map.h>
#include <KryneEngine/Core/Math/Hashing.hpp>
#include <KryneEngine/Core/Memory/Allocators/Allocator.hpp>
#include <KryneEngine/Core/Threads/LightweightMutex.hpp>

#include "ProjectManager/Logger/LogSeverity.hpp"

namespace ProjectManager
{
    struct LogFilter;

    class Logger
    {
    public:
        static Logger* GetInstance() { return s_instance; }

        explicit Logger(KryneEngine::AllocatorInstance _allocator);

        struct MessageView
        {
            LogSeverity m_severity;
            std::chrono::system_clock::time_point m_time;
            KryneEngine::u64 m_category;
            eastl::string_view m_shortMessage;
            eastl::string_view m_longMessage;
        };

        template <size_t N>
        static constexpr KryneEngine::u64 MakeCategoryId(const char (&_name)[N])
        {
            constexpr size_t len = (N == 0 ? 0 : N - 1); // N is >=1 for string literal
            return KryneEngine::Hashing::Murmur2::Murmur2Hash64(_name, len);
        }

        bool RegisterCategory(KryneEngine::u64 _id, const eastl::string_view& _category);

        void Log(
            LogSeverity _severity,
            KryneEngine::u64 _category,
            eastl::string_view _shortMessage,
            eastl::string_view _longMessage = {});

        eastl::vector<MessageView> GetMessageViews(
            const LogFilter& _filter,
            KryneEngine::AllocatorInstance _allocator) const;

        void UpdateMessageViews(eastl::vector<MessageView>& _bundle, const LogFilter& _filter) const;

    private:
        struct Message
        {
            LogSeverity m_severity;
            std::chrono::system_clock::time_point m_time;
            KryneEngine::u64 m_category;
            eastl::string m_shortMessage;
            eastl::string m_longMessage;
        };

        static Logger* s_instance;

        KryneEngine::AllocatorInstance m_allocator;
        mutable KryneEngine::LightweightMutex m_mutex;
        eastl::vector_map<KryneEngine::u64, eastl::string> m_categories;
        eastl::vector<Message> m_messages;
    };
}
