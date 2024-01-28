
#include "pch.h"
#include "PropertiesPanel.h"

#include "meshgen/Application.h"
#include "meshgen/RenderManager.h"
#include "meshgen/InputGeom.h"

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
		if (!m_app->m_zoneLoaded)
			ImGui::TextColored(ImColor(255, 255, 0), "No zone loaded (Ctrl+O to open zone)");
		else
			ImGui::TextColored(ImColor(0, 255, 0), m_app->m_zoneDisplayName.c_str());

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

		if (m_app->m_geom)
		{
			auto* loader = m_app->m_geom->getMeshLoader();

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

		ImGuiContext* ctx = ImGui::GetCurrentContext();

		ImGui::DragInt4("Window Rect", &m_app->m_windowRect.x);

		glm::ivec4 rect = g_render->GetViewport();
		ImGui::DragInt4("Viewport", &rect.x);

		ImGui::DragInt2("Mouse Pos", &m_app->m_mousePos.x);

		glm::ivec2 viewportMouse = g_render->GetMousePos();
		ImGui::DragInt2("Viewport Mouse", &viewportMouse.x);


		//if (ImGui::BeginTabItem("Scene"))
		//{
		//	float planes[2] = { m_nearPlane, m_farPlane };
		//	if (ImGui::DragFloat2("Near/Far Plane", planes))
		//	{
		//		m_nearPlane = planes[0];
		//		m_farPlane = planes[1];
		//	}

		//	ImGui::InputInt4("Viewport", glm::value_ptr(m_viewport));
		//	ImGui::InputFloat3("Ray Start", glm::value_ptr(m_rayStart));
		//	ImGui::InputFloat3("Ray End", glm::value_ptr(m_rayEnd));

		//	if (ImGui::CollapsingHeader("ViewModel Matrix"))
		//	{
		//		ImGui::InputFloat4("[0]", glm::value_ptr(m_viewModelMtx[0]));
		//		ImGui::InputFloat4("[1]", glm::value_ptr(m_viewModelMtx[1]));
		//		ImGui::InputFloat4("[2]", glm::value_ptr(m_viewModelMtx[2]));
		//		ImGui::InputFloat4("[3]", glm::value_ptr(m_viewModelMtx[3]));
		//	}

		//	if (ImGui::CollapsingHeader("Projection Matrix"))
		//	{
		//		ImGui::InputFloat4("[0]", glm::value_ptr(m_projMtx[0]));
		//		ImGui::InputFloat4("[1]", glm::value_ptr(m_projMtx[1]));
		//		ImGui::InputFloat4("[2]", glm::value_ptr(m_projMtx[2]));
		//		ImGui::InputFloat4("[3]", glm::value_ptr(m_projMtx[3]));
		//	}

		//	ImGui::DragFloat("FOV", &m_fov, 0.1f, 30.0f, 150.0f);

		//	ImGui::InputInt2("Mouse", glm::value_ptr(m_mousePos));

		//	ImGui::Separator();
		//	ImGui::SliderFloat("Cam Speed", &m_camMoveSpeed, 10, 350);

		//	ImGui::EndTabItem();
		//}

		//if (ImGui::BeginTabItem("Im3d"))
		//{
		//	Im3d::AppData& appData = Im3d::GetAppData();
		//	static float size = 2.0f;

		//	//for (int i = 0; i < Im3d::Key_Count; ++i)
		//	//{
		//	//	ImGui::Text("Key %d: %d", i, appData.m_keyDown[i]);
		//	//}
		//	ImGui::DragFloat("Size", &size);
		//	ImGui::InputFloat3("Cursor Ray Origin", &appData.m_cursorRayOrigin.x);
		//	ImGui::InputFloat3("Cursor Ray Direction", &appData.m_cursorRayDirection.x);
		//	Im3d::SetSize(size);

		//	static Im3d::Mat4 transform(1.0f);
		//	//transform.setTranslation(Im3d::Vec3(200, -300, 200));

		//	Im3d::Text(Im3d::Vec3(10, 0, 0), 1.0, Im3d::Color(1.0, 0.0, 0.0), 0, "Hello");

		//	if (Im3d::Gizmo("GizmoUnified", transform))
		//	{
		//		// if Gizmo() returns true, the transform was modified
		//		switch (Im3d::GetContext().m_gizmoMode)
		//		{
		//		case Im3d::GizmoMode_Translation:
		//		{
		//			Im3d::Vec3 pos = transform.getTranslation();
		//			ImGui::Text("Position: %.3f, %.3f, %.3f", pos.x, pos.y, pos.z);
		//			break;
		//		}
		//		case Im3d::GizmoMode_Rotation:
		//		{
		//			Im3d::Vec3 euler = Im3d::ToEulerXYZ(transform.getRotation());
		//			ImGui::Text("Rotation: %.3f, %.3f, %.3f", Im3d::Degrees(euler.x), Im3d::Degrees(euler.y), Im3d::Degrees(euler.z));
		//			break;
		//		}
		//		case Im3d::GizmoMode_Scale:
		//		{
		//			Im3d::Vec3 scale = transform.getScale();
		//			ImGui::Text("Scale: %.3f, %.3f, %.3f", scale.x, scale.y, scale.z);
		//			break;
		//		}
		//		default: break;
		//		};

		//	}

		//	ImGui::EndTabItem();
		//}
	}

	ImGui::End();
}
