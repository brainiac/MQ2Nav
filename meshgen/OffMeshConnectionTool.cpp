//
// OffMeshConnectionTool.cpp
//

#include "meshgen/OffMeshConnectionTool.h"
#include "meshgen/InputGeom.h"
#include "meshgen/NavMeshTool.h"

#include "common/NavMeshData.h"

#include <Recast.h>
#include <RecastDebugDraw.h>
#include <DetourDebugDraw.h>

#include <imgui/imgui.h>
#include <imgui/custom/imgui_user.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

OffMeshConnectionTool::OffMeshConnectionTool()
{
}

OffMeshConnectionTool::~OffMeshConnectionTool()
{
}

void OffMeshConnectionTool::init(NavMeshTool* meshTool)
{
	m_meshTool = meshTool;

	m_state = (OffMeshConnectionToolState*)m_meshTool->getToolState(type());
	if (!m_state)
	{
		m_state = new OffMeshConnectionToolState();
		m_meshTool->setToolState(type(), m_state);
	}

	m_state->init(m_meshTool);
}

void OffMeshConnectionTool::reset()
{
	m_editing = false;
	m_state->m_hitPosSet = false;
}

void OffMeshConnectionTool::handleMenu()
{
	auto navMesh = m_meshTool->GetNavMesh();

	if (ImGui::RadioButton("One Way", !m_state->m_bidir))
		m_state->m_bidir = false;
	if (ImGui::RadioButton("Bidirectional", m_state->m_bidir))
		m_state->m_bidir = true;
}

void OffMeshConnectionTool::handleClick(const glm::vec3& s, const glm::vec3& p, bool shift)
{
	// if we're not editing a volume right now, switch to edit mode.
	if (m_state->m_currentConnectionId == 0)
		m_editing = true;

	if (!m_editing) return;
	if (!m_meshTool) return;

	auto navMesh = m_meshTool->GetNavMesh();
	std::vector<dtTileRef> modifiedTiles = m_state->handleConnectionClick(p, shift);

	if (!modifiedTiles.empty())
	{
		m_meshTool->RebuildTiles(modifiedTiles);
	}
}

void OffMeshConnectionTool::handleUpdate(float /*dt*/)
{
}

void OffMeshConnectionTool::handleRender()
{
}

void OffMeshConnectionTool::handleRenderOverlay(const glm::mat4& proj,
	const glm::mat4& model, const glm::ivec4& view)
{
	// Draw start and end point labels
	if (m_state->m_hitPosSet)
	{
		glm::vec3 pos = glm::project(m_state->m_hitPos, model, proj, view);

		ImGui::RenderText((int)pos.x + 5, -((int)pos.y - 5), ImVec4(0, 0, 0, 220), "Start");
	}

	// Tool help
	if (!m_state->m_hitPosSet)
	{
		ImGui::TextColored(ImVec4(255, 255, 255, 192),
			"LMB: Create new connection.\nSHIFT+LMB: Click to delete existing connection.");
	}
	else
	{
		ImGui::TextColored(ImVec4(255, 255, 255, 192),
			"LMB: Set connection end point and finish.");
	}
}

//============================================================================

void OffMeshConnectionToolState::init(NavMeshTool* meshTool)
{
	m_meshTool = meshTool;
}

void OffMeshConnectionToolState::reset()
{

}

void OffMeshConnectionToolState::handleRender()
{
	auto navMesh = m_meshTool->GetNavMesh();
	const auto& connections = navMesh->GetConnections();

	DebugDrawGL dd;
	const float s = navMesh->GetNavMeshConfig().agentRadius;

	if (m_hitPosSet)
		duDebugDrawCross(&dd, m_hitPos[0], m_hitPos[1] + 0.1f, m_hitPos[2], s, duRGBA(0, 0, 0, 128), 2.0f);

	unsigned int conColor = duRGBA(192, 0, 128, 192);
	unsigned int baseColor = duRGBA(0, 0, 0, 64);
	dd.depthMask(false);

	dd.begin(DU_DRAW_LINES, 2.0f);

	for (const auto& connection : connections)
	{
		const glm::vec3& from = connection->start;
		const glm::vec3& to = connection->end;

		dd.vertex(glm::value_ptr(from), baseColor);
		dd.vertex(glm::value_ptr(from + glm::vec3(0.0f, 0.2f, 0.0f)), baseColor);

		dd.vertex(glm::value_ptr(to), baseColor);
		dd.vertex(glm::value_ptr(to + glm::vec3(0.0f, 0.2f, 0.0f)), baseColor);

		duAppendCircle(&dd, from.x, from.y + 0.1f, from.z, s, baseColor);
		duAppendCircle(&dd, to.x, to.y + 0.1f, to.z, s, baseColor);

		duAppendArc(&dd, from.x, from.y, from.z, to.x, to.y, to.z, 0.25f,
			connection->bidirectional ? 0.6f : 0.0f, 0.6f, conColor);
	}
	dd.end();
	dd.depthMask(true);
}

void OffMeshConnectionToolState::handleRenderOverlay(const glm::mat4& proj, const glm::mat4& model, const glm::ivec4& view)
{

}

std::vector<dtTileRef> OffMeshConnectionToolState::handleConnectionClick(const glm::vec3& p, bool shift)
{
	std::vector<dtTileRef> modifiedTiles;
	auto navMesh = m_meshTool->GetNavMesh();

	if (shift)
	{
		// Delete
		uint32_t nearestId = 0;
		const auto& connections = navMesh->GetConnections();

		// Find nearest link end-point
		float nearestDistance = FLT_MAX;

		for (const auto& connection : connections)
		{
			float d = glm::distance2(p, connection->start);
			if (d < nearestDistance)
			{
				nearestDistance = d;
				nearestId = connection->id;

				continue; // skip checking end, since it would have same id if closer.
			}

			d = glm::distance2(p, connection->end);
			if (d < nearestDistance)
			{
				nearestDistance = d;
				nearestId = connection->id;
			}
		}

		if (nearestId != 0
			&& glm::sqrt(nearestDistance) < navMesh->GetNavMeshConfig().agentRadius)
		{
			modifiedTiles = navMesh->GetTilesIntersectingConnection(nearestId);

			navMesh->DeleteConnectionById(nearestId);
		}
	}
	else
	{
		// Create
		if (!m_hitPosSet)
		{
			m_hitPos = p;
			m_hitPosSet = true;
		}
		else
		{
			auto connection = std::make_unique<OffMeshConnection>();
			connection->start = m_hitPos;
			connection->end = p;
			connection->type = ConnectionType::Basic;
			connection->areaType = static_cast<uint8_t>(PolyArea::Ground);
			connection->bidirectional = m_bidir;

			auto conn = navMesh->AddConnection(std::move(connection));

			m_hitPosSet = false;
			modifiedTiles = navMesh->GetTilesIntersectingConnection(conn->id);
		}
	}

	return modifiedTiles;
}
