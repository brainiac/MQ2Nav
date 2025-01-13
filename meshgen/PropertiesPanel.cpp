
#include "pch.h"
#include "PropertiesPanel.h"

#include "common/MathUtil.h"
#include "meshgen/Editor.h"
#include "meshgen/ZoneProject.h"
#include "meshgen/ZoneResourceManager.h"
#include "meshgen/ZoneCollisionMesh.h"
#include "imgui/fonts/IconsFontAwesome.h"

#include <glm/gtc/type_ptr.hpp>

PropertiesPanel::PropertiesPanel(Editor* editor)
	: PanelWindow("Properties", "PropertiesPane")
	, m_editor(editor)
{
}

PropertiesPanel::~PropertiesPanel()
{
}

void PropertiesPanel::OnImGuiRender(bool* p_open)
{
	ImGui::SetNextWindowPos(ImVec2(20, 21 + 20), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(panelName.c_str(), p_open))
	{
		auto project = m_editor->GetProject();

		// Show zone name or loading progress... (maybe convert to dialog?)
		if (project->IsZoneLoading())
		{
			ProgressState state = project->GetProgress();

			if (state.display.value_or(false))
			{
				ImGui::TextColored(ImColor(255, 255, 0), "%s", state.text.value_or("").c_str());
				ImGui::ProgressBar(state.value.value_or(0.0f));
			}
		}
		else if (!project->IsZoneLoaded())
		{
			ImGui::Text("No zone loaded");
		}
	}

	ImGui::End();
}
