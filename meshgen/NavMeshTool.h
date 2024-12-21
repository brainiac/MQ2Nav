//
// NavMeshTool.h
//

#pragma once

#include "common/NavMesh.h"
#include "common/NavMeshData.h"
#include "common/Utilities.h"
#include "meshgen/NavMeshBuilder.h"

#include <DebugDraw.h>
#include <DetourNavMesh.h>
#include <Recast.h>

#include <glm/glm.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <thread>
#include <agents.h>


class dtNavMesh;
class dtNavMeshQuery;
class NavMeshLoader;
class NavMeshProject;
class NavMeshTool;
class rcContext;
class RecastContext;
class ZoneProject;

namespace spdlog {
	class logger;
}

//----------------------------------------------------------------------------

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
	WAYPOINTS,
	INFO,
	MAX_TOOLS
};

class Tool
{
public:
	virtual ~Tool() {}
	virtual ToolType type() const = 0;
	virtual void init(NavMeshTool* meshTool) = 0;
	virtual void reset() = 0;
	virtual void handleMenu() = 0;
	virtual void handleClick(const glm::vec3& p, bool shift) = 0;
	virtual void handleRender() = 0;
	virtual void handleRenderOverlay() = 0;
	virtual void handleUpdate(float dt) = 0;
};

struct ToolState
{
	virtual ~ToolState() {}
	virtual void init(NavMeshTool* meshTool) = 0;
	virtual void reset() = 0;
	virtual void handleRender() = 0;
	virtual void handleRenderOverlay() = 0;
	virtual void handleUpdate(float dt) = 0;
};

//----------------------------------------------------------------------------

class NavMeshTool
{
public:
	NavMeshTool();
	virtual ~NavMeshTool();

	void OnProjectChanged(const std::shared_ptr<ZoneProject>& project);
	void OnNavMeshProjectChanged(const std::shared_ptr<NavMeshProject>& navMeshCtx);

	void Reset();

	void handleDebug();
	void handleTools();
	void handleRender(const glm::mat4& viewModelProjMtx, const glm::ivec4& viewport);
	void handleUpdate(float dt);
	void handleRenderOverlay();
	void handleClick(const glm::vec3& p, bool shift);

	void GetTilePos(const glm::vec3& pos, int& tx, int& ty);

	bool BuildMesh();
	void BuildTile(const glm::vec3& pos);
	void RebuildTiles(const std::vector<dtTileRef>& tiles);

	void RemoveTile(const glm::vec3& pos);
	void RemoveAllTiles();

	void UpdateTileSizes();

	float GetLastBuildTime() const;

	std::shared_ptr<NavMesh> GetNavMesh() const { return m_navMesh; }

	void setTool(Tool* tool);
	ToolState* getToolState(ToolType type) const;
	void setToolState(ToolType type, ToolState* s);

	unsigned int GetColorForPoly(const dtPoly* poly);

	glm::ivec2 Project(const glm::vec3& point) const;
	const glm::ivec4& GetViewport() const { return m_viewport; }
	const glm::mat4& GetViewModelProjMtx() const { return m_viewModelProjMtx; }

private:
	void initToolStates();
	void resetToolStates();
	void renderToolStates();

	void NavMeshUpdated();
	void drawConvexVolumes(duDebugDraw* dd);

private:
	glm::mat4 m_viewModelProjMtx;
	glm::ivec4 m_viewport;

	mq::Signal<>::ScopedConnection m_navMeshConn;

	std::unique_ptr<Tool> m_tool;
	std::map<ToolType, std::unique_ptr<ToolState>> m_toolStates;

	std::shared_ptr<ZoneProject> m_zoneProj;
	std::shared_ptr<NavMeshProject> m_navMeshProj;
	std::shared_ptr<NavMesh> m_navMesh;

	int m_tilesWidth = 0;
	int m_tilesHeight = 0;
	int m_tilesCount = 0;
	float m_lastBuildTime = 0.0f;

	bool m_drawInputGeometry = true;
	bool m_drawNavMeshBVTree = false;
	bool m_drawNavMeshNodes = false;
	bool m_drawNavMeshPortals = false;
	bool m_drawGrid = true;
};
