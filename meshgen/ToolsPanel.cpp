
#include "pch.h"
#include "ToolsPanel.h"

#include "meshgen/Application.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/ZoneContext.h"
#include "imgui/fonts/IconsMaterialDesign.h"


ToolsPanel::ToolsPanel(Application* app)
	: PanelWindow("Tools", "ToolsPanel")
	, m_app(app)
{
}

ToolsPanel::~ToolsPanel()
{
}

void ToolsPanel::OnImGuiRender(bool* p_open)
{
	if (ImGui::Begin(panelName.c_str(), p_open))
	{
		if (m_zoneContext && m_zoneContext->IsZoneLoaded())
		{
			if (m_app->m_meshTool->isBuildingTiles())
			{
				int tw, th, tm;
				m_app->m_meshTool->getTileStatistics(tw, th, tm);
				int tt = tw * th;

				float percent = (float)m_app->m_meshTool->getTilesBuilt() / (float)tt;

				char szProgress[256];
				sprintf_s(szProgress, "%d of %d (%.2f%%)", m_app->m_meshTool->getTilesBuilt(), tt, percent * 100);

				ImGui::ProgressBar(percent, ImVec2(-1, 0), szProgress);

				if (ImGui::Button((const char*)ICON_MD_CANCEL " Stop"))
					m_app->m_meshTool->CancelBuildAllTiles();
			}
			else
			{
				m_app->m_meshTool->handleTools();
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
