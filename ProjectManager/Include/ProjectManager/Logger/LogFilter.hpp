/**
 * @file
 * @author Max Godefroy
 * @date 04/03/2026.
 */

#pragma once

#include <EASTL/vector_set.h>
#include <KryneEngine/Core/Common/BitUtils.hpp>
#include <KryneEngine/Core/Common/Types.hpp>

#include "ProjectManager/Logger/LogSeverity.hpp"

namespace ProjectManager
{
    static_assert(static_cast<unsigned char>(LogSeverity::COUNT) < 8);

    struct LogFilter
    {
        explicit LogFilter(const KryneEngine::AllocatorInstance _allocator): m_categoryWhiteList(_allocator) {}

        KryneEngine::u64 m_severityWhiteList = KryneEngine::BitUtils::BitMask<KryneEngine::u64>(static_cast<KryneEngine::u8>(LogSeverity::COUNT));
        eastl::vector_set<KryneEngine::u64> m_categoryWhiteList;

        void IncludeSeverity(const LogSeverity _severity)
        {
            m_severityWhiteList |= 1 << static_cast<KryneEngine::u8>(_severity);
        }

        void ExcludeSeverity(const LogSeverity _severity)
        {
            m_severityWhiteList &= ~(1 << static_cast<KryneEngine::u8>(_severity));
        }

        [[nodiscard]] bool IsSeverityIncluded(const LogSeverity _severity) const
        {
            return (1 << static_cast<unsigned char>(_severity) & m_severityWhiteList) != 0;
        }

        [[nodiscard]] bool FilterMessage(const LogSeverity _severity, const KryneEngine::u64 _category) const
        {
            if ((1ull << static_cast<unsigned char>(_severity) & m_severityWhiteList) == 0)
            {
                return false;
            }
            return m_categoryWhiteList.find(_category) != m_categoryWhiteList.end();
        }
    };
}
