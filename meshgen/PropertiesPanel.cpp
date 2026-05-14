
#include "pch.h"
#include "PropertiesPanel.h"

#include "meshgen/Editor.h"
#include "meshgen/ZoneProject.h"
#include "meshgen/ZoneResourceManager.h"
#include "meshgen/ZoneRenderManager.h"
#include "imgui/fonts/IconsFontAwesome.h"

#include <glm/gtc/type_ptr.hpp>


extern glm::vec4 g_globalAmbient;
extern glm::vec4 g_directionalLightColor;
extern glm::vec4 g_directionalLightNormal;
extern glm::vec4 g_directionalLightBounceColor;



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
		else
		{
			float time_scale = eqg::world_clock::time_scale();
			if (time_scale == 0)
				time_scale = 1.0f;

			ImGui::Text("Global Time Scale");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::SliderFloat("##Global Time Scale", &time_scale, 0.001f, 10.0f, "%.3f"))
			{
				eqg::world_clock::set_time_scale(time_scale);
			}

			ImGui::SeparatorText("Environment");

			static glm::vec3 light(1.0f, 0.0f, 0.0f);

			ImGui::ColorEdit3("Global Ambient", glm::value_ptr(g_globalAmbient));
			glm::vec4 constantAmbient = project->GetRenderManager()->GetConstantAmbientColor();
			if (ImGui::ColorEdit4("Constant Ambient", &constantAmbient.x))
				project->GetRenderManager()->SetConstantAmbientColor(constantAmbient);
			ImGui::DragFloat3("Directional Light Vector", &g_directionalLightNormal.x, 0.01f, -glm::pi<float>(), glm::pi<float>());
			ImGui::ColorEdit4("Directional Light", &g_directionalLightColor.x);
			ImGui::ColorEdit4("Bounce Light", &g_directionalLightBounceColor.x);
		}
	}

	ImGui::End();
}
