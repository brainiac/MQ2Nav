//
// NavMeshTool.h
//

#pragma once

#include "meshgen/DebugDrawImpl.h"
#include "meshgen/ZoneRenderManager.h"
#include "common/NavMesh.h"
#include "common/NavMeshData.h"
#include "common/Utilities.h"

#include <DebugDraw.h>
#include <DetourNavMesh.h>
#include <Recast.h>

#include <glm/glm.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <thread>

class RecastContext;
class InputGeom;
class dtNavMesh;
class dtNavMeshQuery;

class NavMeshTool;
class NavMeshLoader;

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
	virtual void init(class NavMeshTool* meshTool) = 0;
	virtual void reset() = 0;
	virtual void handleMenu() = 0;
	virtual void handleClick(const glm::vec3& s, const glm::vec3& p, bool shift) = 0;
	virtual void handleRender() = 0;
	virtual void handleRenderOverlay(const glm::mat4& proj,
		const glm::mat4& model, const glm::ivec4& view) = 0;
	virtual void handleUpdate(float dt) = 0;
};

struct ToolState
{
	virtual ~ToolState() {}
	virtual void init(NavMeshTool* meshTool) = 0;
	virtual void reset() = 0;
	virtual void handleRender() = 0;
	virtual void handleRenderOverlay(const glm::mat4& proj,
		const glm::mat4& model, const glm::ivec4& view) = 0;
	virtual void handleUpdate(const float dt) = 0;
};

class NavMeshDebugDraw : public DebugDrawImpl
{
public:
	NavMeshDebugDraw(NavMeshTool* tool) : m_tool(tool) {}

	virtual unsigned int polyToCol(const dtPoly* poly) override;

private:
	NavMeshTool* m_tool;
};

//----------------------------------------------------------------------------

class NavMeshTool
{
public:
	NavMeshTool(const std::shared_ptr<NavMesh>& navMesh);
	virtual ~NavMeshTool();

	void setContext(RecastContext* ctx) { m_ctx = ctx; }

	void handleDebug();
	void handleTools();
	void handleRender();
	void handleUpdate(float dt);
	void handleRenderOverlay(const glm::mat4& proj, const glm::mat4& model, const glm::ivec4& view);
	void handleGeometryChanged(InputGeom* geom);

	bool handleBuild();
	void handleClick(const glm::vec3& s, const glm::vec3& p, bool shift);

	void GetTilePos(const glm::vec3& pos, int& tx, int& ty);

	void BuildTile(const glm::vec3& pos);
	void RemoveTile(const glm::vec3& pos);
	void RemoveAllTiles();

	void BuildAllTiles(const std::shared_ptr<dtNavMesh>& navMesh, bool async = true);
	void CancelBuildAllTiles(bool wait = true);
	void UpdateTileSizes();
	void RebuildTiles(const std::vector<dtTileRef>& tiles);

	void SaveNavMesh();

	bool isBuildingTiles() const { return m_buildingTiles; }

	void getTileStatistics(int& width, int& height, int& maxTiles) const;
	int getTilesBuilt() const { return m_tilesBuilt; }
	float getTotalBuildTimeMS() const { return m_totalBuildTimeMs; }

	void setOutputPath(const char* output_path);

	InputGeom* getInputGeom() { return m_geom; }

	std::shared_ptr<NavMesh> GetNavMesh() const { return m_navMesh; }

	void setTool(Tool* tool);
	ToolState* getToolState(ToolType type) const;
	void setToolState(ToolType type, ToolState* s);

	unsigned int GetColorForPoly(const dtPoly* poly);

	duDebugDraw& getDebugDraw() { return m_dd; }
	
	ZoneRenderManager* GetRenderManager() { return m_renderManager.get(); }

private:
	deleting_unique_ptr<rcCompactHeightfield> rasterizeGeometry(rcConfig& cfg) const;

	void resetCommonSettings();

	void initToolStates();
	void resetToolStates();
	void renderToolStates();
	void renderOverlayToolStates(const glm::mat4& proj, const glm::mat4& model, const glm::ivec4& view);

	void RebuildTile(
		const std::shared_ptr<OffMeshConnectionBuffer> connBuffer,
		dtTileRef tileRef);

	unsigned char* buildTileMesh(
		const int tx,
		const int ty,
		const float* bmin,
		const float* bmax,
		const std::shared_ptr<OffMeshConnectionBuffer>& connBuffer,
		int& dataSize) const;

	void NavMeshUpdated();

	void drawConvexVolumes(duDebugDraw* dd);

private:
	InputGeom* m_geom = nullptr;
	std::unique_ptr<ZoneRenderManager> m_renderManager;

	std::shared_ptr<NavMesh> m_navMesh;
	mq::Signal<>::ScopedConnection m_navMeshConn;

	std::unique_ptr<Tool> m_tool;
	std::map<ToolType, std::unique_ptr<ToolState>> m_toolStates;

	// we don't own this
	RecastContext* m_ctx = nullptr;

	std::shared_ptr<spdlog::logger> m_logger;

	int m_maxTiles = 0;
	int m_maxPolysPerTile = 0;

	char* m_outputPath = nullptr;
	float m_totalBuildTimeMs = 0.f;

	int m_tilesWidth = 0;
	int m_tilesHeight = 0;
	int m_tilesCount = 0;
	std::atomic<int> m_tilesBuilt = 0;
	std::atomic<bool> m_buildingTiles = false;
	std::atomic<bool> m_cancelTiles = false;
	std::thread m_buildThread;

	uint8_t m_navMeshDrawFlags = 0;
	NavMeshConfig m_config;

	bool m_drawInputGeometry = true;
	bool m_drawNavMeshTiles = true;
	bool m_drawNavMeshDisabledTiles = true;
	bool m_drawNavMeshBVTree = false;
	bool m_drawNavMeshNodes = false;
	bool m_drawNavMeshPortals = false;

	NavMeshDebugDraw m_dd{ this };
};
