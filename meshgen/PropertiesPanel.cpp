
#include "pch.h"
#include "PropertiesPanel.h"

#include "common/MathUtil.h"
#include "meshgen/Editor.h"
#include "meshgen/MapGeometryLoader.h"
#include "meshgen/RenderManager.h"
#include "meshgen/ZoneProject.h"
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
			ImGui::TextColored(ImColor(255, 255, 0), "No zone loaded (Ctrl+O to open zone)");
		else
			ImGui::TextColored(ImColor(0, 255, 0), "%s", project->GetDisplayName().c_str());

		if (project->IsZoneLoaded())
		{
			ImGui::Separator();

			auto* loader = project->GetMeshLoader();

			if (loader->HasDynamicObjects())
				ImGui::TextColored(ImColor(0, 127, 127), "%d zone objects loaded", loader->GetDynamicObjectsCount());
			else
			{
				ImGui::TextColored(ImColor(255, 255, 0), "No zone objects loaded");

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text("Dynamic objects can be loaded after entering\n"
						"a zone in EQ with MQ2Nav loaded. Re-open the zone\n"
						"to refresh the dynamic objects.");
					ImGui::EndTooltip();
				}
			}
			ImGui::Text("Verts: %.1fk Tris: %.1fk", loader->getVertCount() / 1000.0f, loader->getTriCount() / 1000.0f);

			if (project->IsNavMeshReady())
			{
				// TODO: Can remove, have it on the toolbar
				ImGui::Separator();
				if (ImGui::Button(ICON_FA_FLOPPY_O " Save"))
					project->SaveNavMesh();
			}

			ImGui::Separator();

			// TODO: Move these to somewhere else?
			Camera& camera = m_editor->GetCamera();

			glm::vec3 cameraPos = to_eq_coord(camera.GetPosition());
			if (ImGui::DragFloat3("Position", glm::value_ptr(cameraPos), 0.01f))
			{
				camera.SetPosition(from_eq_coord(cameraPos));
			}

			float angle[2] = { camera.GetYaw(), camera.GetPitch() };
			if (ImGui::DragFloat2("Camera Angle", angle, 0.1f))
			{
				camera.SetYaw(angle[0]);
				camera.SetPitch(angle[1]);
			}

			//float fov = camera->GetFieldOfView();
			//if (ImGui::DragFloat("FOV", &fov))
			//{
			//	camera->SetFieldOfView(fov);
			//}

			//float ratio = camera->GetAspectRatio();
			//if (ImGui::DragFloat("Aspect Ratio", &ratio, 0.1f))
			//{
			//	camera->SetAspectRatio(ratio);
			//}

		}
	}

	ImGui::End();
}
