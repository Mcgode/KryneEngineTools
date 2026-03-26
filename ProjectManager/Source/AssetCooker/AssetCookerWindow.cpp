/**
 * @file
 * @author Max Godefroy
 * @date 20/03/2026.
 */

#include "AssetCookerWindow.hpp"

#include <imgui.h>
#include <KryneEngine/Modules/ImGui/Helpers.hpp>

#include "ProjectManager/AssetCooker/AssetCooker.hpp"

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

        if (ImGui::BeginChild("LeftPanel"))
        {
            for (auto i = 0; i < m_assetCooker->m_workThreads.Size(); i++)
            {
                ImGui::Text("Thread %d:", i);
                ImGui::SameLine();

                const auto path = m_assetCooker->m_cookingAssetPerThread[i];

                if (path.empty())
                {
                    ImGui::Text("Idle");
                }
                else
                {
                    KryneEngine::Modules::ImGui::Helpers::Spinner("##spinner", 4, 2, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
                    ImGui::SameLine();
                    ImGui::Text("%s", path.c_str());
                }
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }
} // ProjectManager