
#include "pch.h"
#include "PropertiesPanel.h"

#include "meshgen/Application.h"
#include "meshgen/MapGeometryLoader.h"
#include "meshgen/RenderManager.h"
#include "meshgen/ZoneContext.h"

#include <glm/gtc/type_ptr.hpp>

PropertiesPanel::PropertiesPanel(Application* app)
	: PanelWindow("Properties", "PropertiesPane")
	, m_app(app)
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
		// show zone name
		if (!m_zoneContext || !m_zoneContext->IsZoneLoaded())
			ImGui::TextColored(ImColor(255, 255, 0), "No zone loaded (Ctrl+O to open zone)");
		else
			ImGui::TextColored(ImColor(0, 255, 0), "%s", m_zoneContext->GetDisplayName().c_str());

		ImGui::Separator();

		Camera* camera = g_render->GetCamera();

		glm::vec3 cameraPos = to_eq_coord(camera->GetPosition());
		if (ImGui::DragFloat3("Position", glm::value_ptr(cameraPos), 0.01f))
		{
			camera->SetPosition(from_eq_coord(cameraPos));
		}

		float angle[2] = { camera->GetHorizontalAngle(), camera->GetVerticalAngle() };
		if (ImGui::DragFloat2("Camera Angle", angle, 0.1f))
		{
			camera->SetHorizontalAngle(angle[0]);
			camera->SetVerticalAngle(angle[1]);
		}

		float fov = camera->GetFieldOfView();
		if (ImGui::DragFloat("FOV", &fov))
		{
			camera->SetFieldOfView(fov);
		}

		float ratio = camera->GetAspectRatio();
		if (ImGui::DragFloat("Aspect Ratio", &ratio, 0.1f))
		{
			camera->SetAspectRatio(ratio);
		}

		if (m_zoneContext)
		{
			auto* loader = m_zoneContext->GetMeshLoader();

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

			if (m_app->m_navMesh->IsNavMeshLoaded())
			{
				ImGui::Separator();
				if (ImGui::Button((const char*)ICON_FA_FLOPPY_O " Save"))
					m_app->SaveMesh();
			}
		}
	}

	ImGui::End();
}

void PropertiesPanel::SetZoneContext(const std::shared_ptr<ZoneContext>& context)
{
	if (m_zoneContext == context)
		return;

	m_zoneContext = context;
}
