/**
 * @file
 * @author Max Godefroy
 * @date 20/03/2026.
 */

#include "ProjectManager/Database/Database.hpp"

#include <KryneEngine/Core/Common/Assert.hpp>

namespace ProjectManager
{
    Database::Database(const eastl::string_view _path)
    {
        const int result = sqlite3_open_v2(_path.data(), &m_database, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, nullptr);

        KE_ASSERT(result == SQLITE_OK && m_database != nullptr);
    }

    Database::~Database()
    {
        if (m_database != nullptr)
            sqlite3_close_v2(m_database);
    }

    bool Database::TableExists(const eastl::string_view _tableName) const
    {
        char testSql[256];
        snprintf(testSql, sizeof(testSql), "PRAGMA table_info(%s)", _tableName.data());

        bool exists = false;
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(m_database, testSql, -1, &stmt, nullptr) == SQLITE_OK)
        {
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                exists = true;
            }
            sqlite3_finalize(stmt);
        }

        return exists;
    }

    int Database::Execute(const eastl::string_view _sql, sqlite3_stmt** _pStmt) const
    {
        return sqlite3_prepare_v2(m_database, _sql.data(), static_cast<int>(_sql.size()), _pStmt, nullptr);
    }
} // ProjectManager