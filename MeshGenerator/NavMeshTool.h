//
// NavMeshTool.h
//

#pragma once

#include "ChunkyTriMesh.h"
#include "NavMeshData.h"
#include "Utilities.h"

// recast/detour headers
#include <DebugDraw.h>
#include <DetourNavMesh.h>
#include <Recast.h>
#include <RecastDump.h>

#include <glm/glm.hpp>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <thread>

class BuildContext;
class InputGeom;
class dtNavMesh;
class dtNavMeshQuery;
class dtCrowd;

class NavMeshTool;
class NavMeshLoader;
class TileBuilder;

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
	MAX_TOOLS
};

struct Tool
{
	virtual ~Tool() {}
	virtual ToolType type() const = 0;
	virtual void init(class NavMeshTool* meshTool) = 0;
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
	virtual void init(NavMeshTool* meshTool) = 0;
	virtual void reset() = 0;
	virtual void handleRender() = 0;
	virtual void handleRenderOverlay(const glm::mat4& proj,
		const glm::mat4& model, const glm::ivec4& view) = 0;
	virtual void handleUpdate(const float dt) = 0;
};

//----------------------------------------------------------------------------

class NavMeshTool
{
public:
	NavMeshTool();
	virtual ~NavMeshTool();

	void setContext(BuildContext* ctx) { m_ctx = ctx; }

	void SaveMesh(const std::string& shortName, const std::string& outputPath);
	bool LoadMesh(const std::string& outputPath);

	void ResetMesh();

	void handleSettings();
	void handleTools();
	void handleRender();
	void handleRenderOverlay(const glm::mat4& proj, const glm::mat4& model, const glm::ivec4& view);
	void handleMeshChanged(InputGeom* geom);

	bool handleBuild();
	void handleClick(const glm::vec3& s, const glm::vec3& p, bool shift);


	void getTilePos(const float* pos, int& tx, int& ty);

	void buildTile(const float* pos);
	void removeTile(const float* pos);
	void removeAllTiles();

	void buildAllTiles(bool async = true);
	void cancelBuildAllTiles(bool wait = true);

	bool isBuildingTiles() const { return m_buildingTiles; }

	void getTileStatistics(int& width, int& height, int& maxTiles) const;
	int getTilesBuilt() const { return m_tilesBuilt; }
	float getTotalBuildTimeMS() const { return m_totalBuildTimeMs; }

	void setOutputPath(const char* output_path);

	void UpdateTileSizes();

	uint8_t getNavMeshDrawFlags() const { return m_navMeshDrawFlags; }
	void setNavMeshDrawFlags(uint8_t flags) { m_navMeshDrawFlags = flags; }

	InputGeom* getInputGeom() { return m_geom; }
	dtNavMesh* getNavMesh() { return m_navMesh.get(); }
	dtNavMeshQuery* getNavMeshQuery() { return m_navQuery.get(); }

	void setNavMesh(dtNavMesh* mesh);

	float getAgentRadius() { return m_config.agentRadius; }
	float getAgentHeight() { return m_config.agentHeight; }
	float getAgentClimb() { return m_config.agentMaxClimb; }

private:
	void setTool(Tool* tool);
	ToolState* getToolState(ToolType type) const;
	void setToolState(ToolType type, ToolState* s);

	deleting_unique_ptr<rcCompactHeightfield> rasterizeGeometry(rcConfig& cfg) const;

	void resetCommonSettings();

	void initToolStates();
	void resetToolStates();
	void renderToolStates();
	void renderOverlayToolStates(const glm::mat4& proj, const glm::mat4& model, const glm::ivec4& view);

	void handleToggle();
	void handleStep();

	void handleUpdate(float dt);

	unsigned char* buildTileMesh(const int tx, const int ty, const float* bmin, const float* bmax, int& dataSize) const;

	void saveAll(const char* path, const dtNavMesh* mesh);
	dtNavMesh* loadAll(const char* path);

private:
	const int MAX_NODES = 1024 * 1024;

	InputGeom* m_geom = nullptr;
	deleting_unique_ptr<dtNavMesh> m_navMesh;
	deleting_unique_ptr<dtNavMeshQuery> m_navQuery;

	std::unique_ptr<Tool> m_tool;
	std::map<ToolType, std::unique_ptr<ToolState>> m_toolStates;

	std::shared_ptr<TileBuilder> m_tileBuilder;

	// we don't own this
	BuildContext* m_ctx = nullptr;

	int m_maxTiles = 0;
	int m_maxPolysPerTile = 0;

	uint32_t m_tileCol = duRGBA(0, 0, 0, 32);
	glm::vec3 m_tileBmin, m_tileBmax;
	char* m_outputPath = nullptr;

	float m_totalBuildTimeMs = 0.f;
	float m_tileBuildTime = 0.f;
	float m_tileMemUsage = 0;
	int m_tileTriCount = 0;

	int m_tilesWidth = 0;
	int m_tilesHeight = 0;
	int m_tilesCount = 0;
	std::atomic<int> m_tilesBuilt = 0;
	std::atomic<bool> m_buildingTiles = false;
	std::atomic<bool> m_cancelTiles = false;
	std::thread m_buildThread;

	NavMeshConfig m_config;
	uint8_t m_navMeshDrawFlags = 0;

	struct DrawMode { enum Enum {
		NAVMESH,
		NAVMESH_TRANS,
		NAVMESH_BVTREE,
		NAVMESH_NODES,
		NAVMESH_PORTALS,
		NAVMESH_INVIS,
		MESH,
		VOXELS,
		VOXELS_WALKABLE,
		COMPACT,
		COMPACT_DISTANCE,
		COMPACT_REGIONS,
		REGION_CONNECTIONS,
		RAW_CONTOURS,
		BOTH_CONTOURS,
		CONTOURS,
		POLYMESH,
		POLYMESH_DETAIL,
		MAX
	};};
	DrawMode::Enum m_drawMode = DrawMode::NAVMESH;
};
