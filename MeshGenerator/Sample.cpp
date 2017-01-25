//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include "Sample.h"
#include "Interface.h"
#include "InputGeom.h"
#include "Recast.h"
#include "RecastDebugDraw.h"
#include "DetourDebugDraw.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourCrowd.h"

#include <imgui/imgui.h>

Sample::Sample()
	: m_navMeshDrawFlags(DU_DRAWNAVMESH_OFFMESHCONS | DU_DRAWNAVMESH_CLOSEDLIST)
{
	resetCommonSettings();

	m_navQuery = deleting_unique_ptr<dtNavMeshQuery>(dtAllocNavMeshQuery(),
		[](dtNavMeshQuery* q) { dtFreeNavMeshQuery(q); });
	m_crowd = deleting_unique_ptr<dtCrowd>(dtAllocCrowd(),
		[](dtCrowd* c) { dtFreeCrowd(c); });
}

Sample::~Sample()
{
}

//----------------------------------------------------------------------------

void Sample::setTool(Tool* tool)
{
	m_tool.reset(tool);

	if (tool)
	{
		m_tool->init(this);
	}
}

ToolState* Sample::getToolState(ToolType type) const
{
	auto iter = m_toolStates.find(type);
	if (iter != m_toolStates.end())
		return iter->second.get();
	return nullptr;
}

void Sample::setToolState(ToolType type, ToolState* s)
{
	m_toolStates[type] = std::unique_ptr<ToolState>(s);
}

void Sample::setNavMesh(dtNavMesh* mesh)
{
	m_navMesh = deleting_unique_ptr<dtNavMesh>(mesh,
		[](dtNavMesh* m) { dtFreeNavMesh(m); });
}

void Sample::handleSettings()
{
}

void Sample::handleTools()
{
}

void Sample::handleDebugMode()
{
}

void Sample::handleRender()
{
	if (!m_geom)
		return;

	DebugDrawGL dd;

	// Draw mesh
	duDebugDrawTriMesh(&dd,
		m_geom->getMeshLoader()->getVerts(),
		m_geom->getMeshLoader()->getVertCount(),
		m_geom->getMeshLoader()->getTris(),
		m_geom->getMeshLoader()->getNormals(),
		m_geom->getMeshLoader()->getTriCount(),
		0,   // flags
		1.0f // tex scale
	);

	// Draw bounds
	glm::vec3 bmin = m_geom->getMeshBoundsMin();
	glm::vec3 bmax = m_geom->getMeshBoundsMax();
	duDebugDrawBoxWire(&dd, bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2],
		duRGBA(255, 255, 255, 128), 1.0f);
}

void Sample::handleRenderOverlay(const glm::mat4& /*proj*/,
	const glm::mat4& /*model*/, const glm::ivec4& /*view*/)
{
}

void Sample::handleMeshChanged(InputGeom* geom)
{
	m_geom = geom;
}

void Sample::resetCommonSettings()
{
	m_cellSize = 0.6f;
	m_cellHeight = 0.3f;

	m_agentHeight = 6.0f;
	m_agentRadius = 2.0f;
	m_agentMaxClimb = 4.0f;
	m_agentMaxSlope = 75.0f;

	m_regionMinSize = 8;
	m_regionMergeSize = 20;

	m_edgeMaxLen = 12.0f;
	m_edgeMaxError = 1.3f;

	m_detailSampleDist = 6.0f;
	m_detailSampleMaxError = 1.0f;

	m_vertsPerPoly = 6.0f;

	m_partitionType = PartitionType::WATERSHED;
}

void Sample::handleCommonSettings()
{
}

void Sample::handleClick(const glm::vec3& s, const glm::vec3& p, bool shift)
{
	if (m_tool)
	{
		m_tool->handleClick(s, p, shift);
	}
}

void Sample::handleToggle()
{
	if (m_tool)
	{
		m_tool->handleToggle();
	}
}

void Sample::handleStep()
{
	if (m_tool)
	{
		m_tool->handleStep();
	}
}

bool Sample::handleBuild()
{
	return true;
}

void Sample::handleUpdate(float dt)
{
	if (m_tool)
	{
		m_tool->handleUpdate(dt);
	}

	updateToolStates(dt);
}


void Sample::updateToolStates(float dt)
{
	for (const auto& p : m_toolStates)
	{
		if (p.second)
		{
			p.second->handleUpdate(dt);
		}
	}
}

void Sample::initToolStates(Sample* sample)
{
	for (const auto& p : m_toolStates)
	{
		if (p.second)
		{
			p.second->init(sample);
		}
	}
}

void Sample::resetToolStates()
{
	for (const auto& p : m_toolStates)
	{
		if (p.second)
		{
			p.second->reset();
		}
	}
}

void Sample::renderToolStates()
{
	for (const auto& p : m_toolStates)
	{
		if (p.second)
		{
			p.second->handleRender();
		}
	}
}

void Sample::renderOverlayToolStates(const glm::mat4& proj,
	const glm::mat4& model, const glm::ivec4& view)
{
	for (const auto& p : m_toolStates)
	{
		if (p.second)
		{
			p.second->handleRenderOverlay(proj, model, view);
		}
	}
}
