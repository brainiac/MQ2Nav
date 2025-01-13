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
