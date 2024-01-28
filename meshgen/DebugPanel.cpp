
#include "pch.h"
#include "DebugPanel.h"

#include "meshgen/Application.h"
#include "meshgen/NavMeshTool.h"

DebugPanel::DebugPanel(Application* app)
	: PanelWindow("Debug", "DebugPanel")
	, m_app(app)
{
}

DebugPanel::~DebugPanel()
{
}

void DebugPanel::OnImGuiRender(bool* p_open)
{
	if (ImGui::Begin(panelName.c_str(), p_open))
	{
		m_app->m_meshTool->handleDebug();
	}

	ImGui::End();
}
