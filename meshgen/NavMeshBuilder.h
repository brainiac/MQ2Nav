
#pragma once

#include "common/NavMesh.h"
#include "common/Utilities.h"
#include "common/NavMeshData.h"
#include "DetourNavMesh.h"

#include <taskflow/taskflow.hpp>
#include <memory>
#include <agents.h>


class NavMesh;
class NavMeshProject;
class RecastContext;
class ZoneProject;
struct rcCompactHeightfield;
struct rcConfig;

class NavMeshBuildData
{
public:
	std::shared_ptr<OffMeshConnectionBuffer> offMeshConnections;
	std::shared_ptr<RecastContext> context;
};

class NavMeshBuilder
{
public:
	NavMeshBuilder(const std::shared_ptr<NavMeshProject>& navMeshProj);
	~NavMeshBuilder();

	bool IsBuildingTiles() const { return m_buildingTiles; }

	float GetLastBuildTimeMS() const { return m_totalBuildTimeMs; }
	int GetTilesBuilt() const { return m_tilesBuilt; }
	int GetTileCount() const { return m_tilesWidth * m_tilesHeight; }

	void Update();

	using BuildCallback = std::function<void(bool result, float elapsedTime)>;

	bool BuildNavMesh(BuildCallback callback);
	bool BuildTile(const glm::vec3& pos);
	bool RebuildTiles(const std::vector<dtTileRef>& tiles);

	void CancelBuild(bool wait);

private:
	bool Prepare();
	void UpdateTileSizes();
	std::shared_ptr<NavMeshBuildData> CreateBuildData() const;
	bool BuildAllTiles(const std::shared_ptr<dtNavMesh>& navMesh, bool async,
		BuildCallback callback);

	std::shared_ptr<NavMeshProject> GetNavMeshProject() const { return m_navMeshProj.lock(); }

	bool RebuildTile(const std::shared_ptr<NavMeshBuildData>& buildData,
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

	std::weak_ptr<NavMeshProject> m_navMeshProj;
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
