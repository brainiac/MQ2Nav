
#include "NavMeshBuilder.h"
#include "common/NavMesh.h"
#include "common/Utilities.h"
#include "meshgen/ChunkyTriMesh.h"
#include "meshgen/MapGeometryLoader.h"
#include "meshgen/RecastContext.h"
#include "meshgen/ZoneProject.h"

#include "DetourNavMeshBuilder.h"
#include "Recast.h"

#include <ppl.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>


NavMeshBuilder::NavMeshBuilder(const std::shared_ptr<NavMeshProject>& navMeshProj)
	: m_navMeshProj(navMeshProj)
{
}

NavMeshBuilder::~NavMeshBuilder()
{
	CancelBuild(true);
}

std::shared_ptr<NavMeshBuildData> NavMeshBuilder::CreateBuildData() const
{
	auto buildData = std::make_shared<NavMeshBuildData>();
	buildData->context = std::make_shared<RecastContext>();
	buildData->offMeshConnections = m_navMesh->CreateOffMeshConnectionBuffer();
	return buildData;
}

void NavMeshBuilder::Update()
{
	if (!m_navMesh)
		return;

	TileDataPtr data;
	while (concurrency::try_receive(m_builtTileData, data))
	{
		m_navMesh->AddTile(data->data, data->length, data->x, data->y);
	}
}

bool NavMeshBuilder::Prepare()
{
	auto navMeshProj = GetNavMeshProject();
	if (!navMeshProj)
	{
		SPDLOG_ERROR("NavMeshBuilder::Prepare: Navmesh project went away...");
		return false;
	}

	auto zoneProj = navMeshProj->GetZoneProject();
	if (!zoneProj || !zoneProj->GetMeshLoader())
	{
		SPDLOG_ERROR("NavMeshBuilder::Prepare: No vertices and triangles.");
		return false;
	}

	m_navMesh = navMeshProj->GetNavMesh();
	if (!m_navMesh)
	{
		SPDLOG_ERROR("NavMeshBuilder::Prepare: No navmesh!");
		return false;
	}

	m_config = navMeshProj->GetNavMeshConfig();
	UpdateTileSizes();

	return true;
}

void NavMeshBuilder::UpdateTileSizes()
{
	glm::vec3 bmin, bmax;
	m_navMesh->GetNavMeshBounds(bmin, bmax);

	int gw = 0, gh = 0;
	rcCalcGridSize(&bmin[0], &bmax[0], m_config.cellSize, &gw, &gh);

	const int ts = static_cast<int>(m_config.tileSize);
	m_tilesWidth = (gw + ts - 1) / ts;
	m_tilesHeight = (gh + ts - 1) / ts;
	m_tilesCount = m_tilesWidth * m_tilesHeight;

	int tileBits = DT_TILE_BITS;
	int polyBits = DT_POLY_BITS;
	m_maxTiles = 1 << tileBits;
	m_maxPolysPerTile = 1 << polyBits;
}


bool NavMeshBuilder::BuildNavMesh(BuildCallback callback)
{
	if (!Prepare())
		return false;

	dtNavMeshParams params;
	glm::vec3 boundsMin = m_navMesh->GetNavMeshBoundsMin();
	rcVcopy(params.orig, glm::value_ptr(boundsMin));
	params.tileWidth = m_config.tileSize * m_config.cellSize;
	params.tileHeight = m_config.tileSize * m_config.cellSize;
	params.maxTiles = m_tilesWidth * m_tilesHeight;
	params.maxPolys = m_maxPolysPerTile * params.maxTiles;

	std::shared_ptr<dtNavMesh> navMesh(dtAllocNavMesh(), [](dtNavMesh* ptr) { dtFreeNavMesh(ptr); });
	dtStatus status = navMesh->init(&params);
	if (dtStatusFailed(status))
	{
		SPDLOG_ERROR("NavMeshBuilder::BuildNavMesh: Could not init navmesh.");
		return false;
	}

	m_oldMesh = m_navMesh->GetNavMesh();
	m_navMesh->SetNavMesh(navMesh, false);

	BuildAllTiles(navMesh, true, std::move(callback));

	return true;
}

bool NavMeshBuilder::BuildTile(const glm::vec3& pos)
{
	if (!Prepare())
		return false;

	const glm::vec3& bmin = m_navMesh->GetNavMeshBoundsMin();
	const glm::vec3& bmax = m_navMesh->GetNavMeshBoundsMax();

	const float ts = m_config.tileSize * m_config.cellSize;
	const int tx = (int)((pos[0] - bmin[0]) / ts);
	const int ty = (int)((pos[2] - bmin[2]) / ts);

	glm::vec3 tileBmin, tileBmax;
	tileBmin[0] = bmin[0] + tx * ts;
	tileBmin[1] = bmin[1];
	tileBmin[2] = bmin[2] + ty * ts;

	tileBmax[0] = bmin[0] + (tx + 1) * ts;
	tileBmax[1] = bmax[1];
	tileBmax[2] = bmin[2] + (ty + 1) * ts;

	auto buildData = CreateBuildData();

	std::shared_ptr<dtNavMesh> navMesh = m_navMesh->GetNavMesh();
	if (!navMesh)
	{
		navMesh = std::shared_ptr<dtNavMesh>(dtAllocNavMesh(), [](dtNavMesh* ptr) { dtFreeNavMesh(ptr); });

		m_navMesh->SetNavMesh(navMesh, false);

		dtNavMeshParams params;
		rcVcopy(params.orig, glm::value_ptr(bmin));
		params.tileWidth = m_config.tileSize * m_config.cellSize;
		params.tileHeight = m_config.tileSize * m_config.cellSize;
		params.maxTiles = m_tilesWidth * m_tilesHeight;
		params.maxPolys = m_maxPolysPerTile * params.maxTiles;

		dtStatus status;

		status = navMesh->init(&params);
		if (dtStatusFailed(status))
		{
			SPDLOG_ERROR("NavMeshBuilder::BuildTile: Could not init navmesh.");
			return false;
		}
	}

	int dataSize = 0;
	unsigned char* data = BuildTileMesh(tx, ty, glm::value_ptr(tileBmin),
		glm::value_ptr(tileBmax), buildData, dataSize);

	m_navMesh->AddTile(data, dataSize, tx, ty);

	SPDLOG_DEBUG("Build Tile: ({}, {})", tx, ty);
	return true;
}

bool NavMeshBuilder::BuildAllTiles(const std::shared_ptr<dtNavMesh>& navMesh, bool async, BuildCallback callback)
{
	if (m_buildingTiles) return false;
	if (!navMesh) return false;

	// if async, invoke on a new thread
	if (async)
	{
		if (m_buildThread.joinable())
			m_buildThread.join();

		m_buildThread = std::thread([this, navMesh, callback = std::move(callback)]()
			{
				BuildAllTiles(navMesh, false, std::move(callback));
			});
	
		return true;
	}

	m_buildingTiles = true;
	m_cancelTiles = false;

	const glm::vec3& bmin = m_navMesh->GetNavMeshBoundsMin();
	const glm::vec3& bmax = m_navMesh->GetNavMeshBoundsMax();
	int gw = 0, gh = 0;
	rcCalcGridSize(&bmin[0], &bmax[0], m_config.cellSize, &gw, &gh);
	const int ts = (int)m_config.tileSize;
	const int tw = (gw + ts - 1) / ts;
	const int th = (gh + ts - 1) / ts;
	const float tcs = m_config.tileSize * m_config.cellSize;

	m_tilesBuilt = 0;

	auto buildData = CreateBuildData();
	auto& ctx = *buildData->context;

	// Start the build process.
	ctx.startTimer(RC_TIMER_TEMP);

	concurrency::task_group tasks;
	for (int x = 0; x < tw; x++)
	{
		for (int y = 0; y < th; y++)
		{
			tasks.run([this, x, y, &bmin, &bmax, tcs, buildData]()
				{
					if (m_cancelTiles)
						return;

					++m_tilesBuilt;

					glm::vec3 tileBmin, tileBmax;
					tileBmin[0] = bmin[0] + x * tcs;
					tileBmin[1] = bmin[1];
					tileBmin[2] = bmin[2] + y * tcs;

					tileBmax[0] = bmin[0] + (x + 1) * tcs;
					tileBmax[1] = bmax[1];
					tileBmax[2] = bmin[2] + (y + 1) * tcs;

					int dataSize = 0;
					uint8_t* data = BuildTileMesh(x, y, glm::value_ptr(tileBmin),
						glm::value_ptr(tileBmax), buildData, dataSize);

					if (data)
					{
						TileDataPtr tileData = std::make_shared<TileData>();
						tileData->data = data;
						tileData->length = dataSize;
						tileData->x = x;
						tileData->y = y;

						asend(m_builtTileData, tileData);
					}
				});
		}
	}

	tasks.wait();

	// Start the build process.
	ctx.stopTimer(RC_TIMER_TEMP);

	int totalTime = ctx.getAccumulatedTime(RC_TIMER_TEMP);
	m_totalBuildTimeMs = static_cast<float>(totalTime) / 1000.0f;

	ctx.LogBuildTimes(totalTime);

	m_buildingTiles = false;
	m_oldMesh.reset();

	if (callback)
		callback(true, m_totalBuildTimeMs);

	return true;
}

bool NavMeshBuilder::RebuildTile(const std::shared_ptr<NavMeshBuildData>& buildData, dtTileRef tileRef)
{
	if (!Prepare())
		return false;

	auto navMesh = m_navMesh->GetNavMesh();
	const dtMeshTile* tile = navMesh->getTileByRef(tileRef);
	if (!tile || !tile->header) return false;

	auto bmin = tile->header->bmin;
	auto bmax = tile->header->bmax;
	int tx = tile->header->x;
	int ty = tile->header->y;

	int dataSize = 0;
	unsigned char* data = BuildTileMesh(tx, ty, bmin, bmax, buildData, dataSize);

	m_navMesh->AddTile(data, dataSize, tx, ty, tile->header->layer);

	SPDLOG_DEBUG("Rebuild Tile ({}, {}):", tx, ty);
	return true;
}

bool NavMeshBuilder::RebuildTiles(const std::vector<dtTileRef>& tiles)
{
	if (!Prepare())
		return false;

	auto buildData = CreateBuildData();
	bool result = true;

	for (dtTileRef tileRef : tiles)
	{
		if (!RebuildTile(buildData, tileRef))
			result = false;
	}

	return result;
}

void NavMeshBuilder::CancelBuild(bool wait)
{
	if (m_buildingTiles)
		m_cancelTiles = true;

	if (wait && m_buildThread.joinable())
		m_buildThread.join();

	if (m_oldMesh && m_navMesh)
	{
		m_navMesh->SetNavMesh(m_oldMesh, false);
		m_oldMesh.reset();
	}
}

deleting_unique_ptr<rcCompactHeightfield> NavMeshBuilder::RasterizeGeometry(
	std::shared_ptr<NavMeshBuildData> buildData,
	rcConfig& cfg) const
{
	auto navMeshProj = GetNavMeshProject();
	if (!navMeshProj)
	{
		SPDLOG_ERROR("NavMeshBuilder::RasterizeGeometry: No navmesh project went away...");
		return nullptr;
	}

	auto zoneProj = navMeshProj->GetZoneProject();
	if (!zoneProj || !zoneProj->GetMeshLoader())
	{
		SPDLOG_ERROR("NavMeshBuilder::RasterizeGeometry: No input geometry");
		return nullptr;
	}

	// Allocate voxel heightfield where we rasterize our input data to.
	deleting_unique_ptr<rcHeightfield> solid(rcAllocHeightfield(),
		[](rcHeightfield* hf) { rcFreeHeightField(hf); });

	auto& ctx = *buildData->context;

	if (!rcCreateHeightfield(&ctx, *solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
	{
		SPDLOG_ERROR("NavMeshBuilder::RasterizeGeometry: Could not create solid heightfield.");
		return nullptr;
	}

	MapGeometryLoader* loader = zoneProj->GetMeshLoader();

	const float* verts = loader->getVerts();
	const int nverts = loader->getVertCount();
	const rcChunkyTriMesh* chunkyMesh = zoneProj->GetChunkyMesh();

	// Allocate array that can hold triangle flags.
	// If you have multiple meshes you need to process, allocate
	// and array which can hold the max number of triangles you need to process.

	std::unique_ptr<unsigned char[]> triareas(new unsigned char[chunkyMesh->maxTrisPerChunk]);

	float tbmin[2], tbmax[2];
	tbmin[0] = cfg.bmin[0];
	tbmin[1] = cfg.bmin[2];
	tbmax[0] = cfg.bmax[0];
	tbmax[1] = cfg.bmax[2];
	int cid[512];// TODO: Make grow when returning too many items.
	const int ncid = rcGetChunksOverlappingRect(chunkyMesh, tbmin, tbmax, cid, 512);
	if (!ncid)
		return 0;

	for (int i = 0; i < ncid; ++i)
	{
		const rcChunkyTriMeshNode& node = chunkyMesh->nodes[cid[i]];
		const int* ctris = &chunkyMesh->tris[node.i * 3];
		const int nctris = node.n;

		memset(triareas.get(), 0, nctris * sizeof(unsigned char));
		rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle,
			verts, nverts, ctris, nctris, triareas.get());

		rcRasterizeTriangles(&ctx, verts, nverts, ctris, triareas.get(), nctris, *solid, cfg.walkableClimb);
	}

	// Once all geometry is rasterized, we do initial pass of filtering to
	// remove unwanted overhangs caused by the conservative rasterization
	// as well as filter spans where the character cannot possibly stand.
	rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *solid);
	rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
	rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *solid);

	// Compact the heightfield so that it is faster to handle from now on.
	// This will result more cache coherent data as well as the neighbours
	// between walkable cells will be calculated.
	deleting_unique_ptr<rcCompactHeightfield> chf(rcAllocCompactHeightfield(),
		[](rcCompactHeightfield* hf) { rcFreeCompactHeightfield(hf); });

	if (!rcBuildCompactHeightfield(&ctx, cfg.walkableHeight, cfg.walkableClimb, *solid, *chf))
	{
		SPDLOG_ERROR("NavMeshBuilder::RasterizeGeometry: Could not build compact data.");
		return nullptr;
	}

	return std::move(chf);
}

unsigned char* NavMeshBuilder::BuildTileMesh(
	const int tx,
	const int ty,
	const float* bmin,
	const float* bmax,
	const std::shared_ptr<NavMeshBuildData>& buildData,
	int& dataSize) const
{
	// Get the input geometry
	auto navMeshProj = GetNavMeshProject();
	if (!navMeshProj)
	{
		SPDLOG_ERROR("NavMeshBuilder::BuildTileMesh: Navmesh project went away...");
		return nullptr;
	}

	auto zoneProj = navMeshProj->GetZoneProject();
	if (!zoneProj || !zoneProj->GetMeshLoader())
	{
		SPDLOG_ERROR("NavMeshBuilder::BuildTileMesh: No input geometry");
		return nullptr;
	}

	// Init build configuration from GUI
	rcConfig cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.cs = m_config.cellSize;
	cfg.ch = m_config.cellHeight;
	cfg.walkableSlopeAngle = m_config.agentMaxSlope;
	cfg.walkableHeight = (int)ceilf(m_config.agentHeight / cfg.ch);
	cfg.walkableClimb = (int)floorf(m_config.agentMaxClimb / cfg.ch);
	cfg.walkableRadius = (int)ceilf(m_config.agentRadius / cfg.cs);
	cfg.maxEdgeLen = (int)(m_config.edgeMaxLen / m_config.cellSize);
	cfg.maxSimplificationError = m_config.edgeMaxError;
	cfg.minRegionArea = (int)rcSqr(m_config.regionMinSize);		// Note: area = size*size
	cfg.mergeRegionArea = (int)rcSqr(m_config.regionMergeSize);	// Note: area = size*size
	cfg.maxVertsPerPoly = (int)m_config.vertsPerPoly;
	cfg.tileSize = (int)m_config.tileSize;
	cfg.borderSize = cfg.walkableRadius + 3; // Reserve enough padding.
	cfg.width = cfg.tileSize + cfg.borderSize * 2;
	cfg.height = cfg.tileSize + cfg.borderSize * 2;
	cfg.detailSampleDist = m_config.detailSampleDist < 0.9f ? 0 : m_config.cellSize * m_config.detailSampleDist;
	cfg.detailSampleMaxError = m_config.cellHeight * m_config.detailSampleMaxError;

	// Expand the heighfield bounding box by border size to find the extents of geometry we need to build this tile.
	//
	// This is done in order to make sure that the navmesh tiles connect correctly at the borders,
	// and the obstacles close to the border work correctly with the dilation process.
	// No polygons (or contours) will be created on the border area.
	//
	// IMPORTANT!
	//
	//   :''''''''':
	//   : +-----+ :
	//   : |     | :
	//   : |     |<--- tile to build
	//   : |     | :
	//   : +-----+ :<-- geometry needed
	//   :.........:
	//
	// You should use this bounding box to query your input geometry.
	//
	// For example if you build a navmesh for terrain, and want the navmesh tiles to match the terrain tile size
	// you will need to pass in data from neighbour terrain tiles too! In a simple case, just pass in all the 8 neighbours,
	// or use the bounding box below to only pass in a sliver of each of the 8 neighbours.
	rcVcopy(cfg.bmin, bmin);
	rcVcopy(cfg.bmax, bmax);
	cfg.bmin[0] -= cfg.borderSize * cfg.cs;
	cfg.bmin[2] -= cfg.borderSize * cfg.cs;
	cfg.bmax[0] += cfg.borderSize * cfg.cs;
	cfg.bmax[2] += cfg.borderSize * cfg.cs;

	auto& ctx = *buildData->context;

	// Reset build times gathering.
	ctx.resetTimers();

	// Start the build process.
	ctx.startTimer(RC_TIMER_TOTAL);

	deleting_unique_ptr<rcCompactHeightfield> chf = RasterizeGeometry(buildData, cfg);
	if (!chf)
	{
		return nullptr;
	}

	// Erode the walkable area by agent radius.
	if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf))
	{
		SPDLOG_ERROR("NavMeshBuilder::BuildTileMesh: Could not erode.");
		return nullptr;
	}

	// Mark areas.
	const auto& volumes = m_navMesh->GetConvexVolumes();
	for (const auto& vol : volumes)
	{
		rcMarkConvexPolyArea(&ctx, glm::value_ptr(vol->verts[0]), static_cast<int>(vol->verts.size()),
			vol->hmin, vol->hmax, static_cast<uint8_t>(vol->areaType), *chf);
	}

	// Mark doors.


	// Partition the heightfield so that we can use simple algorithm later to triangulate the walkable areas.
	// There are 3 martitioning methods, each with some pros and cons:
	// 1) Watershed partitioning
	//   - the classic Recast partitioning
	//   - creates the nicest tessellation
	//   - usually slowest
	//   - partitions the heightfield into nice regions without holes or overlaps
	//   - the are some corner cases where this method creates produces holes and overlaps
	//      - holes may appear when a small obstacles is close to large open area (triangulation can handle this)
	//      - overlaps may occur if you have narrow spiral corridors (i.e stairs), this make triangulation to fail
	//   * generally the best choice if you precompute the nacmesh, use this if you have large open areas
	// 2) Monotone partioning
	//   - fastest
	//   - partitions the heightfield into regions without holes and overlaps (guaranteed)
	//   - creates long thin polygons, which sometimes causes paths with detours
	//   * use this if you want fast navmesh generation
	// 3) Layer partitoining
	//   - quite fast
	//   - partitions the heighfield into non-overlapping regions
	//   - relies on the triangulation code to cope with holes (thus slower than monotone partitioning)
	//   - produces better triangles than monotone partitioning
	//   - does not have the corner cases of watershed partitioning
	//   - can be slow and create a bit ugly tessellation (still better than monotone)
	//     if you have large open areas with small obstacles (not a problem if you use tiles)
	//   * good choice to use for tiled navmesh with medium and small sized tiles

	if (m_config.partitionType == PartitionType::WATERSHED)
	{
		// Prepare for region partitioning, by calculating distance field along the walkable surface.
		if (!rcBuildDistanceField(&ctx, *chf))
		{
			SPDLOG_ERROR("NavMeshBuilder::BuildTileMesh: Could not build distance field.");
			return nullptr;
		}

		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildRegions(&ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
		{
			SPDLOG_ERROR("NavMeshBuilder::BuildTileMesh: Could not build watershed regions.");
			return nullptr;
		}
	}
	else if (m_config.partitionType == PartitionType::MONOTONE)
	{
		// Partition the walkable surface into simple regions without holes.
		// Monotone partitioning does not need distancefield.
		if (!rcBuildRegionsMonotone(&ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
		{
			SPDLOG_ERROR("NavMeshBuilder::BuildTileMesh: Could not build monotone regions.");
			return nullptr;
		}
	}
	else // PartitionType::LAYERS
	{
		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildLayerRegions(&ctx, *chf, cfg.borderSize, cfg.minRegionArea))
		{
			SPDLOG_ERROR("NavMeshBuilder::BuildTileMesh: Could not build layer regions.");
			return nullptr;
		}
	}

	// Create contours.
	deleting_unique_ptr<rcContourSet> cset(rcAllocContourSet(), [](rcContourSet* cs) { rcFreeContourSet(cs); });
	if (!rcBuildContours(&ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset))
	{
		SPDLOG_ERROR("NavMeshBuilder::BuildTileMesh: Could not create contours.");
		return nullptr;
	}

	if (cset->nconts == 0)
	{
		return nullptr;
	}

	// Build polygon navmesh from the contours.
	deleting_unique_ptr<rcPolyMesh> pmesh(rcAllocPolyMesh(), [](rcPolyMesh* pm) { rcFreePolyMesh(pm); });
	if (!rcBuildPolyMesh(&ctx, *cset, cfg.maxVertsPerPoly, *pmesh))
	{
		SPDLOG_ERROR("NavMeshBuilder::BuildTileMesh: Could not triangulate contours.");
		return nullptr;
	}

	// Build detail mesh.
	deleting_unique_ptr<rcPolyMeshDetail> dmesh(rcAllocPolyMeshDetail(), [](rcPolyMeshDetail* pm) { rcFreePolyMeshDetail(pm); });
	if (!rcBuildPolyMeshDetail(&ctx, *pmesh, *chf,
		cfg.detailSampleDist, cfg.detailSampleMaxError,
		*dmesh))
	{
		SPDLOG_ERROR("NavMeshBuilder::BuildTileMesh: Could build polymesh detail.");
		return nullptr;
	}

	chf.reset();
	cset.reset();

	unsigned char* navData = 0;
	int navDataSize = 0;
	if (cfg.maxVertsPerPoly <= DT_VERTS_PER_POLYGON)
	{
		if (pmesh->nverts >= 0xffff)
		{
			// The vertex indices are ushorts, and cannot point to more than 0xffff vertices.
			SPDLOG_ERROR("NavMeshBuilder::BuildTileMesh: Too many vertices per tile {} (max: {:#x}).",
				pmesh->nverts, (uint16_t)0xffff);
			return nullptr;
		}

		// Update poly flags from areas.
		for (int i = 0; i < pmesh->npolys; ++i)
		{
			if (pmesh->areas[i] >= RC_WALKABLE_AREA)
				pmesh->areas[i] = static_cast<uint8_t>(PolyArea::Ground);

			pmesh->flags[i] = m_navMesh->GetPolyArea(pmesh->areas[i]).flags;
		}

		dtNavMeshCreateParams params;
		memset(&params, 0, sizeof(params));
		params.verts = pmesh->verts;
		params.vertCount = pmesh->nverts;
		params.polys = pmesh->polys;
		params.polyAreas = pmesh->areas;
		params.polyFlags = pmesh->flags;
		params.polyCount = pmesh->npolys;
		params.nvp = pmesh->nvp;
		params.detailMeshes = dmesh->meshes;
		params.detailVerts = dmesh->verts;
		params.detailVertsCount = dmesh->nverts;
		params.detailTris = dmesh->tris;
		params.detailTriCount = dmesh->ntris;

		buildData->offMeshConnections->UpdateNavMeshCreateParams(params);
		params.walkableHeight = m_config.agentHeight;
		params.walkableRadius = m_config.agentRadius;
		params.walkableClimb = m_config.agentMaxClimb;
		params.tileX = tx;
		params.tileY = ty;
		params.tileLayer = 0;
		rcVcopy(params.bmin, pmesh->bmin);
		rcVcopy(params.bmax, pmesh->bmax);
		params.cs = cfg.cs;
		params.ch = cfg.ch;
		params.buildBvTree = true;

		if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
		{
			SPDLOG_ERROR("NavMeshBuilder::BuildTileMesh: Could not build Detour navmesh.");
			return nullptr;
		}
	}

	ctx.stopTimer(RC_TIMER_TOTAL);

	dataSize = navDataSize;
	return navData;
}