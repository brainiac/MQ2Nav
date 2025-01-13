
#include "pch.h"
#include "ViewPanel.h"

#include "ZoneRenderManager.h"
#include "meshgen/Editor.h"
#include "meshgen/NavMeshTool.h"

ViewPanel::ViewPanel(Editor* editor)
	: PanelWindow("View", "ViewPanel")
	, m_editor(editor)
{
}

ViewPanel::~ViewPanel()
{
}

void ViewPanel::OnImGuiRender(bool* p_open)
{
	if (ImGui::Begin(panelName.c_str(), p_open) && m_project)
	{
		auto renderManager = m_project->GetRenderManager();

		bool drawCollisionMesh = renderManager->GetDrawCollisionMesh();
		if (ImGui::Checkbox("Draw Collision Mesh", &drawCollisionMesh))
			renderManager->SetDrawCollisionMesh(drawCollisionMesh);

		bool drawGrid = renderManager->GetDrawGrid();
		if (ImGui::Checkbox("Draw Grid", &drawGrid))
			renderManager->SetDrawGrid(drawGrid);

		ImGui::SeparatorText("Navigation Mesh Render");
		auto navMeshRender = renderManager->GetNavMeshRender();

		uint32_t flags = navMeshRender->GetFlags();
		if (ImGui::CheckboxFlags("Draw Tiles", &flags, ZoneNavMeshRender::DRAW_TILES))
			navMeshRender->SetFlags(flags);
		if (ImGui::CheckboxFlags("Draw Volumes", &flags, ZoneNavMeshRender::DRAW_VOLUMES))
			navMeshRender->SetFlags(flags);
		if (ImGui::CheckboxFlags("Draw Offmesh Connections", &flags, ZoneNavMeshRender::DRAW_OFFMESH_CONNS))
			navMeshRender->SetFlags(flags);

		ImGui::SeparatorText("Navigation Mesh Render - Details");

		if (ImGui::CheckboxFlags("Draw Tile Boundaries", &flags, ZoneNavMeshRender::DRAW_TILE_BOUNDARIES))
			navMeshRender->SetFlags(flags);
		if (ImGui::CheckboxFlags("Draw Polygon Boundaries", &flags, ZoneNavMeshRender::DRAW_POLY_BOUNDARIES))
			navMeshRender->SetFlags(flags);
		if (ImGui::CheckboxFlags("Draw Polygon Vertices", &flags, ZoneNavMeshRender::DRAW_POLY_VERTICES))
			navMeshRender->SetFlags(flags);

		if (ImGui::CheckboxFlags("Draw Closed List (NYI)", &flags, ZoneNavMeshRender::DRAW_CLOSED_LIST))
			navMeshRender->SetFlags(flags);
		if (ImGui::CheckboxFlags("Draw BV Tree", &flags, ZoneNavMeshRender::DRAW_BV_TREE))
			navMeshRender->SetFlags(flags);
		if (ImGui::CheckboxFlags("Draw Nodes (NYI)", &flags, ZoneNavMeshRender::DRAW_NODES))
			navMeshRender->SetFlags(flags);
		if (ImGui::CheckboxFlags("Draw Portals", &flags, ZoneNavMeshRender::DRAW_PORTALS))
			navMeshRender->SetFlags(flags);

		ImGui::SeparatorText("Renderer Debugging");

		float pointSize = renderManager->GetNavMeshRender()->GetPointSize();
		if (ImGui::DragFloat("PointSize", &pointSize, 0.01f, 0, 10, "%.2f"))
		{
			renderManager->SetPointSize(pointSize);
			renderManager->GetNavMeshRender()->SetPointSize(pointSize);
		}
	}

	ImGui::End();
}

void ViewPanel::OnProjectChanged(const std::shared_ptr<ZoneProject>& zoneProject)
{
	m_project = zoneProject;
}
