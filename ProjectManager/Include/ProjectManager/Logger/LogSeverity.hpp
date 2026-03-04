/**
 * @file
 * @author Max Godefroy
 * @date 04/03/2026.
 */

#pragma once

namespace ProjectManager
{
    enum class LogSeverity: unsigned char
    {
        Debug = 0,
        Verbose,
        Info,
        Warning,
        Error,
        Fatal,
        COUNT
    };
}