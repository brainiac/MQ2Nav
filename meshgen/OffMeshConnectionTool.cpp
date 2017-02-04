//
// OffMeshConnectionTool.cpp
//

#include "OffMeshConnectionTool.h"
#include "InputGeom.h"
#include "NavMeshTool.h"

#include "common/NavMeshData.h"

#include <Recast.h>
#include <RecastDebugDraw.h>
#include <DetourDebugDraw.h>

#include <imgui/imgui.h>
#include <imgui/imgui_custom/imgui_user.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

OffMeshConnectionTool::OffMeshConnectionTool()
{
}

OffMeshConnectionTool::~OffMeshConnectionTool()
{
	if (m_meshTool)
	{
		m_meshTool->setNavMeshDrawFlags(m_oldFlags);
	}
}

void OffMeshConnectionTool::init(NavMeshTool* meshTool)
{
	if (m_meshTool != meshTool)
	{
		m_meshTool = meshTool;
		m_oldFlags = m_meshTool->getNavMeshDrawFlags();
		m_meshTool->setNavMeshDrawFlags(m_oldFlags & ~DU_DRAWNAVMESH_OFFMESHCONS);
	}
}

void OffMeshConnectionTool::reset()
{
	m_hitPosSet = false;
}

void OffMeshConnectionTool::handleMenu()
{
	if (ImGui::RadioButton("One Way", !m_bidir))
		m_bidir = false;
	if (ImGui::RadioButton("Bidirectional", m_bidir))
		m_bidir = true;
}

void OffMeshConnectionTool::handleClick(const glm::vec3& s, const glm::vec3& p, bool shift)
{
	if (!m_meshTool) return;
	InputGeom* geom = m_meshTool->getInputGeom();
	if (!geom) return;

	if (shift)
	{
		// Delete
		// Find nearest link end-point
		float nearestDist = FLT_MAX;
		int nearestIndex = -1;
		const float* verts = geom->getOffMeshConnectionVerts();
		for (int i = 0; i < geom->getOffMeshConnectionCount()*2; ++i)
		{
			const float* v = &verts[i*3];
			float d = rcVdistSqr(glm::value_ptr(p), v);
			if (d < nearestDist)
			{
				nearestDist = d;
				nearestIndex = i/2; // Each link has two vertices.
			}
		}
		// If end point close enough, delete it.
		if (nearestIndex != -1 &&
			sqrtf(nearestDist) < m_meshTool->GetNavMesh()->GetNavMeshConfig().agentRadius)
		{
			geom->deleteOffMeshConnection(nearestIndex);
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
			const uint8_t area = static_cast<uint8_t>(PolyArea::Jump);
			const uint16_t flags = static_cast<uint16_t>(PolyFlags::Jump);
			geom->addOffMeshConnection(glm::value_ptr(m_hitPos), glm::value_ptr(p),
				m_meshTool->GetNavMesh()->GetNavMeshConfig().agentRadius, m_bidir ? 1 : 0, area, flags);
			m_hitPosSet = false;
		}
	}
}

void OffMeshConnectionTool::handleToggle()
{
}

void OffMeshConnectionTool::handleStep()
{
}

void OffMeshConnectionTool::handleUpdate(float /*dt*/)
{
}

void OffMeshConnectionTool::handleRender()
{
	DebugDrawGL dd;
	const float s = m_meshTool->GetNavMesh()->GetNavMeshConfig().agentRadius;

	if (m_hitPosSet)
		duDebugDrawCross(&dd, m_hitPos[0],m_hitPos[1]+0.1f,m_hitPos[2], s, duRGBA(0,0,0,128), 2.0f);

	InputGeom* geom = m_meshTool->getInputGeom();
	if (geom)
		geom->drawOffMeshConnections(&dd, true);
}

void OffMeshConnectionTool::handleRenderOverlay(const glm::mat4& proj,
	const glm::mat4& model, const glm::ivec4& view)
{
	// Draw start and end point labels
	if (m_hitPosSet)
	{
		glm::vec3 pos = glm::project(m_hitPos, model, proj, view);

		ImGui::RenderText((int)pos.x + 5, -((int)pos.y - 5), ImVec4(0, 0, 0, 220), "Start");
	}

	// Tool help
	if (!m_hitPosSet)
	{
		ImGui::RenderTextRight(-330, -(view[3] - 40), ImVec4(255, 255, 255, 192),
			"LMB: Create new connection.  SHIFT+LMB: Delete existing connection, click close to start or end point.");
	}
	else
	{
		ImGui::RenderTextRight(-330, -(view[3] - 40), ImVec4(255, 255, 255, 192),
			"LMB: Set connection end point and finish.");
	}
}
