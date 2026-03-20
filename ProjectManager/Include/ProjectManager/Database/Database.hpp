/**
 * @file
 * @author Max Godefroy
 * @date 20/03/2026.
 */

#pragma once

#include <EASTL/string_view.h>
#include <sqlite3.h>

namespace ProjectManager
{
    class Database
    {
    public:
        explicit Database(eastl::string_view _path);
        ~Database();

        [[nodiscard]] bool TableExists(eastl::string_view _tableName) const;

        [[nodiscard]] int Execute(eastl::string_view _sql) const;
        [[nodiscard]] int Prepare(eastl::string_view _sql, sqlite3_stmt** _pStmt) const;

    private:
        sqlite3* m_database { nullptr };
    };
} // ProjectManager