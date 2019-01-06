//
// WaypointsTool.cpp
//

#include "meshgen/WaypointsTool.h"

#include <imgui/imgui.h>

WaypointsTool::WaypointsTool()
{
}

WaypointsTool::~WaypointsTool()
{
}

void WaypointsTool::init(NavMeshTool* meshTool)
{
}

void WaypointsTool::reset()
{
}

void WaypointsTool::handleMenu()
{
	ImGui::Text("Coming soon...");
}

void WaypointsTool::handleClick(const glm::vec3& s, const glm::vec3& p, bool shift)
{
}

void WaypointsTool::handleUpdate(float /*dt*/)
{
}

void WaypointsTool::handleRender()
{
}

void WaypointsTool::handleRenderOverlay(const glm::mat4& proj,
	const glm::mat4& model, const glm::ivec4& view)
{
}
