/**
 * @file
 * @author Max Godefroy
 * @date 04/03/2026.
 */

#pragma once

namespace ProjectManager
{
    class IUiWindow
    {
    public:
        virtual ~IUiWindow() = default;

        virtual void OnImGuiContextStarted() {}

        virtual void Render() = 0;
    };
}