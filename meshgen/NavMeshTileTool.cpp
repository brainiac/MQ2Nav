//
// NavMeshTileTool.cpp
//

#include "meshgen/NavMeshTileTool.h"

#include "imgui/ImGuiUtils.h"
#include "imgui/fonts/IconsMaterialDesign.h"

#include <DebugDraw.h>
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void NavMeshTileTool::init(NavMeshTool* meshTool)
{
	m_meshTool = meshTool;
	m_hitPosSet = false;
}

void NavMeshTileTool::reset()
{
	m_hitPosSet = false;
}

void NavMeshTileTool::handleMenu()
{
	if (ImGui::Button((const char*)ICON_MD_BUILD " Build Mesh"))
	{
		if (m_meshTool)
			m_meshTool->handleBuild();
	}

	ImGui::SameLine();

	if (ImGui::Button((const char*)ICON_MD_DELETE_FOREVER "Remove All"))
	{
		if (m_meshTool)
			m_meshTool->RemoveAllTiles();
	}

	float totalBuildTime = m_meshTool->getTotalBuildTimeMS();
	if (totalBuildTime > 0)
		ImGui::Text("Build Time: %.1fms", totalBuildTime);
}

void NavMeshTileTool::handleClick(const glm::vec3& s, const glm::vec3& p, bool shift)
{
	m_hitPosSet = true;
	m_hitPos = p;

	if (m_meshTool)
	{
		if (shift)
			m_meshTool->RemoveTile(m_hitPos);
		else
			m_meshTool->BuildTile(m_hitPos);
	}
}

void NavMeshTileTool::handleRender()
{
	if (m_hitPosSet)
	{
		const float s = m_meshTool->GetNavMesh()->GetNavMeshConfig().agentRadius;

		ZoneRenderDebugDraw dd(g_zoneRenderManager);

		duDebugDrawCross(&dd, m_hitPos.x, m_hitPos.y, m_hitPos.z, s, duRGBA(0, 0, 0, 128), 2.0f);
	}
}

void NavMeshTileTool::handleRenderOverlay(const glm::mat4& proj,
	const glm::mat4& model, const glm::ivec4& view)
{
	if (m_hitPosSet)
	{
		glm::vec3 pos = glm::project(m_hitPos, model, proj, view);
		int tx = 0, ty = 0;

		m_meshTool->GetTilePos(m_hitPos, tx, ty);
		mq::imgui::RenderText((int)pos.x + 5, -((int)pos.y - 5), ImVec4(0, 0, 0, 220),
			"%.2f %.2f %.2f\nTile: (%d,%d)", m_hitPos.z, m_hitPos.x, m_hitPos.y,
			tx, ty);
	}

	// Tool help
	const int h = view[3];

	ImGui::TextColored(ImVec4(255, 255, 255, 192),
		"LMB: Rebuild tile. Shift+LMB: Clear tile.");
}
