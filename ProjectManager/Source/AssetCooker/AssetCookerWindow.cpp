/**
 * @file
 * @author Max Godefroy
 * @date 20/03/2026.
 */

#include "AssetCookerWindow.hpp"

#include <imgui.h>

#include "ProjectManager/Logger/Logger.hpp"

namespace ProjectManager
{
    AssetCookerWindow::AssetCookerWindow(AssetCooker* _assetCooker)
        : m_assetCooker(_assetCooker)
    {

    }

    void AssetCookerWindow::Render()
    {
        if (!ImGui::Begin("AssetCooker"))
        {
            ImGui::End();
            return;
        }

        ImGui::End();
    }
} // ProjectManager