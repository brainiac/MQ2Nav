
#include "pch.h"
#include "ToolsPanel.h"

#include "meshgen/Editor.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/ZoneContext.h"
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
	auto& meshTool = m_editor->GetMeshTool();

	if (ImGui::Begin(panelName.c_str(), p_open))
	{
		if (m_zoneContext && m_zoneContext->IsZoneLoaded())
		{
			if (meshTool.IsBuildingTiles())
			{
				int tw, th;
				meshTool.GetTileStatistics(tw, th);
				int tt = tw * th;

				float percent = (float)meshTool.GetTilesBuilt() / (float)tt;

				char szProgress[256];
				sprintf_s(szProgress, "%d of %d (%.2f%%)", meshTool.GetTilesBuilt(), tt, percent * 100);

				ImGui::ProgressBar(percent, ImVec2(-1, 0), szProgress);

				if (ImGui::Button((const char*)ICON_MD_CANCEL " Stop"))
					meshTool.CancelBuildAllTiles();
			}
			else
			{
				meshTool.handleTools();
			}
		}
		else
		{
			ImGui::Text("No zone is loaded");
		}
	}

	ImGui::End();
}

void ToolsPanel::SetZoneContext(const std::shared_ptr<ZoneContext>& context)
{
	m_zoneContext = context;
}
