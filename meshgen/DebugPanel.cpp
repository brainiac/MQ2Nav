
#include "pch.h"
#include "DebugPanel.h"

#include "meshgen/Editor.h"
#include "meshgen/NavMeshTool.h"

DebugPanel::DebugPanel(Editor* editor)
	: PanelWindow("Debug", "DebugPanel")
	, m_editor(editor)
{
}

DebugPanel::~DebugPanel()
{
}

void DebugPanel::OnImGuiRender(bool* p_open)
{
	auto& meshTool = m_editor->GetMeshTool();

	if (ImGui::Begin(panelName.c_str(), p_open))
	{
		meshTool.handleDebug();
	}

	ImGui::End();
}
