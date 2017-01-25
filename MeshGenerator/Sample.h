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

#pragma once

#include "Recast.h"
#include "SampleInterfaces.h"

#include <glm/glm.hpp>
#include <functional>
#include <map>
#include <memory>

class BuildContext;
class InputGeom;
class dtNavMesh;
class dtNavMeshQuery;
class dtCrowd;

template <typename T>
using deleting_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

// Tool types.
enum struct ToolType : uint32_t
{
	NONE = 0,
	TILE_EDIT,
	TILE_HIGHLIGHT,
	TEMP_OBSTACLE,
	NAVMESH_TESTER,
	NAVMESH_PRUNE,
	OFFMESH_CONNECTION,
	CONVEX_VOLUME,
	MAX_TOOLS
};

enum struct PartitionType : uint32_t
{
	WATERSHED,
	MONOTONE,
	LAYERS,
};

struct Tool
{
	virtual ~Tool() {}
	virtual ToolType type() const = 0;
	virtual void init(class Sample* sample) = 0;
	virtual void reset() = 0;
	virtual void handleMenu() = 0;
	virtual void handleClick(const glm::vec3& s, const glm::vec3& p, bool shift) = 0;
	virtual void handleRender() = 0;
	virtual void handleRenderOverlay(const glm::mat4& proj,
		const glm::mat4& model, const glm::ivec4& view) = 0;
	virtual void handleToggle() = 0;
	virtual void handleStep() = 0;
	virtual void handleUpdate(float dt) = 0;
};

struct ToolState
{
	virtual ~ToolState() {}
	virtual void init(class Sample* sample) = 0;
	virtual void reset() = 0;
	virtual void handleRender() = 0;
	virtual void handleRenderOverlay(const glm::mat4& proj,
		const glm::mat4& model, const glm::ivec4& view) = 0;
	virtual void handleUpdate(const float dt) = 0;
};

class Sample
{
public:
	Sample();
	virtual ~Sample();

	void setContext(BuildContext* ctx) { m_ctx = ctx; }

	void setTool(Tool* tool);
	ToolState* getToolState(ToolType type) const;
	void setToolState(ToolType type, ToolState* s);

	uint8_t getNavMeshDrawFlags() const { return m_navMeshDrawFlags; }
	void setNavMeshDrawFlags(uint8_t flags) { m_navMeshDrawFlags = flags; }

	InputGeom* getInputGeom() { return m_geom; }
	dtNavMesh* getNavMesh() { return m_navMesh.get(); }
	dtNavMeshQuery* getNavMeshQuery() { return m_navQuery.get(); }
	dtCrowd* getCrowd() { return m_crowd.get(); }

	void setNavMesh(dtNavMesh* mesh);

	float getAgentRadius() { return m_agentRadius; }
	float getAgentHeight() { return m_agentHeight; }
	float getAgentClimb() { return m_agentMaxClimb; }

	void resetCommonSettings();
	void handleCommonSettings();

	void updateToolStates(float dt);
	void initToolStates(Sample* sample);
	void resetToolStates();
	void renderToolStates();
	void renderOverlayToolStates(const glm::mat4& proj, const glm::mat4& model, const glm::ivec4& view);

	virtual void handleSettings();
	virtual void handleTools();
	virtual void handleDebugMode();
	virtual void handleClick(const glm::vec3& s, const glm::vec3& p, bool shift);
	virtual void handleToggle();
	virtual void handleStep();
	virtual void handleRender();
	virtual void handleRenderOverlay(const glm::mat4& proj, const glm::mat4& model, const glm::ivec4& view);
	virtual void handleMeshChanged(class InputGeom* geom);
	virtual bool handleBuild();
	virtual void handleUpdate(float dt);

protected:
	InputGeom* m_geom = nullptr;
	deleting_unique_ptr<dtNavMesh> m_navMesh;
	deleting_unique_ptr<dtNavMeshQuery> m_navQuery;
	deleting_unique_ptr<dtCrowd> m_crowd;

	std::unique_ptr<Tool> m_tool;
	std::map<ToolType, std::unique_ptr<ToolState>> m_toolStates;

	// we don't own this
	BuildContext* m_ctx = nullptr;

	uint8_t m_navMeshDrawFlags = 0;
	float m_cellSize;
	float m_cellHeight;
	float m_agentHeight;
	float m_agentRadius;
	float m_agentMaxClimb;
	float m_agentMaxSlope;
	float m_regionMinSize;
	float m_regionMergeSize;
	float m_edgeMaxLen;
	float m_edgeMaxError;
	float m_vertsPerPoly;
	float m_detailSampleDist;
	float m_detailSampleMaxError;
	PartitionType m_partitionType;
};
