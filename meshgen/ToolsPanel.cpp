
#include "pch.h"
#include "ToolsPanel.h"

#include "meshgen/Editor.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/ZoneProject.h"
#include "imgui/fonts/IconsMaterialDesign.h"


ToolsPanel::ToolsPanel(Editor* editor)
	: PanelWindow("Tools", "ToolsPanel")
	, m_editor(editor)
{
}

ToolsPanel::~ToolsPanel()
{
}

void ToolsPanel::OnImGuiRender(bool* p_open)
{
	if (ImGui::Begin(panelName.c_str(), p_open))
	{
		auto proj = m_editor->GetProject();

		if (proj->IsZoneLoaded())
		{
			auto navMeshProj = proj->GetNavMeshProject();
			if (navMeshProj->IsBuilding())
			{
				auto builder = navMeshProj->GetBuilder();

				int tileCount = builder->GetTileCount();
				int tilesBuilt = builder->GetTilesBuilt();

				float percent = static_cast<float>(tilesBuilt) / static_cast<float>(tileCount);

				char szProgress[256];
				sprintf_s(szProgress, "%d of %d (%.2f%%)", tilesBuilt, tileCount, percent * 100);

				ImGui::ProgressBar(percent, ImVec2(-1, 0), szProgress);

				if (ImGui::Button(ICON_MD_CANCEL " Cancel"))
					builder->CancelBuild(false);
			}
			else
			{
				m_editor->GetMeshTool().handleTools();
			}
		}
		else
		{
			ImGui::Text("No zone is loaded");
		}
	}

	ImGui::End();
}
