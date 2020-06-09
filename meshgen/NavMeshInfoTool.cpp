//
// NavMeshInfoTool.cpp
//

#include "meshgen/NavMeshInfoTool.h"

#include <imgui/imgui.h>

NavMeshInfoTool::NavMeshInfoTool()
{
}

NavMeshInfoTool::~NavMeshInfoTool()
{
}

void NavMeshInfoTool::init(NavMeshTool* meshTool)
{
	m_meshTool = meshTool;
}

void NavMeshInfoTool::reset()
{
}

void NavMeshInfoTool::handleMenu()
{
	ImGui::Text("Mesh Info");
	ImGui::Separator();
	ImGui::Spacing();

	auto navMesh = m_meshTool->GetNavMesh();

	if (!navMesh || !navMesh->IsNavMeshLoaded())
	{
		ImGui::TextColored(ImColor(.5f, .5f, .5f), "No Mesh Loaded");
		return;
	}

	ImGui::Text("Compatibility Version: v%d", navMesh->GetHeaderVersion());
}

void NavMeshInfoTool::handleClick(const glm::vec3& s, const glm::vec3& p, bool shift)
{
}

void NavMeshInfoTool::handleUpdate(float /*dt*/)
{
}

void NavMeshInfoTool::handleRender()
{
}

void NavMeshInfoTool::handleRenderOverlay(const glm::mat4& proj,
	const glm::mat4& model, const glm::ivec4& view)
{
}
