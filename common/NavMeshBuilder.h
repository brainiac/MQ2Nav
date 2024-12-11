
#pragma once

#include <agents.h>

#include "NavMeshData.h"

#include <taskflow/taskflow.hpp>
#include <memory>

#include "DetourNavMesh.h"
#include "NavMesh.h"
#include "Utilities.h"

class RecastContext;
struct rcConfig;
struct rcCompactHeightfield;
class NavMesh;
class ZoneContext;

class NavMeshBuildData
{
public:
	std::shared_ptr<OffMeshConnectionBuffer> offMeshConnections;
	std::shared_ptr<RecastContext> context;
};

class NavMeshBuilder
{
public:
	NavMeshBuilder(const std::shared_ptr<ZoneContext>& zoneContext);
	~NavMeshBuilder();

	void SetNavMesh(const std::shared_ptr<NavMesh>& navMesh);
	void SetConfig(const NavMeshConfig& config);

	float GetTotalBuildTimeMS() const { return m_totalBuildTimeMs; }
	bool IsBuildingTiles() const { return m_buildingTiles; }
	int GetTilesBuilt() const { return m_tilesBuilt; }

	void Update();

	using BuildCallback = std::function<void(bool result)>;

	bool BuildNavMesh(BuildCallback&& callback);

	void CancelBuild(bool wait);

	void BuildTile(const glm::vec3& pos);
	void BuildAllTiles(
		const std::shared_ptr<dtNavMesh>& navMesh, bool async);

	void RebuildTiles(const std::vector<dtTileRef>& tiles);

private:
	void UpdateTileSizes();
	std::shared_ptr<NavMeshBuildData> CreateBuildData() const;

	void RebuildTile(
		const std::shared_ptr<NavMeshBuildData>& buildData,
		dtTileRef tileRef);

	unsigned char* BuildTileMesh(
		int tx,
		int ty,
		const float* bmin,
		const float* bmax,
		const std::shared_ptr<NavMeshBuildData>& buildData,
		int& dataSize) const;

	deleting_unique_ptr<rcCompactHeightfield> RasterizeGeometry(
		std::shared_ptr<NavMeshBuildData> buildData,
		rcConfig& cfg) const;

	struct TileData
	{
		unsigned char* data = nullptr;
		int length = 0;
		int x = 0;
		int y = 0;
	};

	using TileDataPtr = std::shared_ptr<TileData>;
	Concurrency::unbounded_buffer<TileDataPtr> m_builtTileData;


	std::shared_ptr<ZoneContext> m_zoneContext;
	std::shared_ptr<NavMesh> m_navMesh;
	NavMeshConfig m_config;
	int m_tilesWidth = 0;
	int m_tilesHeight = 0;
	int m_tilesCount = 0;
	int m_maxTiles = 0;
	int m_maxPolysPerTile = 0;
	float m_totalBuildTimeMs = 0.f;

	std::atomic<int> m_tilesBuilt = 0;
	std::atomic<bool> m_buildingTiles = false;
	std::atomic<bool> m_cancelTiles = false;
	std::thread m_buildThread;
	std::shared_ptr<dtNavMesh> m_oldMesh;
};
