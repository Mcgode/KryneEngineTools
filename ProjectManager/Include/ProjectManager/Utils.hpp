/**
 * @file
 * @author Max Godefroy
 * @date 07/04/2026.
 */

#pragma once

#include <filesystem>

namespace ProjectManager::Utils
{
    [[nodiscard]] inline bool IsChildDirectory(const std::filesystem::path& _child, const std::filesystem::path& _parent)
    {
        return std::filesystem::relative(_child, _parent).begin()->string() != "..";
    }

}