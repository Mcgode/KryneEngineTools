/**
 * @file
 * @author Max Godefroy
 * @date 20/03/2026.
 */

#pragma once

#include "ProjectManager/IUiWindow.hpp"

namespace ProjectManager
{
    class AssetCooker;

    class AssetCookerWindow: public IUiWindow
    {
    public:
        explicit AssetCookerWindow(AssetCooker* _assetCooker);
        ~AssetCookerWindow() override = default;

        void Render() override;

    private:
        AssetCooker* m_assetCooker;
    };
} // ProjectManager