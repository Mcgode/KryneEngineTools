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

    constexpr const char* LogSeverityToString(const LogSeverity _severity)
    {
        switch (_severity)
        {
        case LogSeverity::Debug:
            return "Debug";
        case LogSeverity::Verbose:
            return "Verbose";
        case LogSeverity::Info:
            return "Info";
        case LogSeverity::Warning:
            return "Warning";
        case LogSeverity::Error:
            return "Error";
        case LogSeverity::Fatal:
            return "Fatal";
        default:
            return "";
        }
    }
}