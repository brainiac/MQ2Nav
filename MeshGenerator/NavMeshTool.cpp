﻿
#include "NavMeshTool.h"

#include "Application.h"
#include "ConvexVolumeTool.h"
#include "InputGeom.h"
#include "NavMeshData.h"
#include "NavMeshPruneTool.h"
#include "NavMeshTesterTool.h"
#include "NavMeshTileTool.h"
#include "OffMeshConnectionTool.h"
#include "Utilities.h"

#include <Recast.h>
#include <RecastDebugDraw.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourDebugDraw.h>

#include <SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_custom/imgui_user.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <ppl.h>
#include <agents.h>
#include <mutex>

//----------------------------------------------------------------------------

NavMeshTool::NavMeshTool()
{
	resetCommonSettings();

	m_navQuery = deleting_unique_ptr<dtNavMeshQuery>(dtAllocNavMeshQuery(),
		[](dtNavMeshQuery* q) { dtFreeNavMeshQuery(q); });

	m_outputPath = new char[MAX_PATH];
	setTool(new NavMeshTileTool);

	UpdateTileSizes();
}

NavMeshTool::~NavMeshTool()
{
	delete[] m_outputPath;
}

void NavMeshTool::SaveMesh(const std::string& outputPath)
{
	saveAll(outputPath.c_str(), m_navMesh.get());
}

bool NavMeshTool::LoadMesh(const std::string& outputPath)
{
	dtNavMesh* mesh = loadAll(outputPath.c_str());
	if (mesh)
	{
		setNavMesh(mesh);

		dtStatus status = m_navQuery->init(m_navMesh.get(), MAX_NODES);
		if (dtStatusFailed(status))
		{
			m_ctx->log(RC_LOG_ERROR, "buildTiledNavigation: Could not init Detour navmesh query");
			return false;
		}

		if (m_tool)
		{
			m_tool->init(this);
		}
		initToolStates();

		return true;
	}

	return false;
}

static const int NAVMESHSET_MAGIC = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T'; //'MSET';
static const int NAVMESHSET_VERSION = 3;

struct NavMeshSetHeader
{
	int magic;
	int version;
	int numTiles;
	dtNavMeshParams params;
};

struct NavMeshTileHeader
{
	dtTileRef tileRef;
	int dataSize;
};


void NavMeshTool::saveAll(const char* path, const dtNavMesh* mesh)
{
	if (!mesh) return;

	FILE* fp = fopen(path, "wb");
	if (!fp)
		return;

	// Store header.
	NavMeshSetHeader header;
	header.magic = NAVMESHSET_MAGIC;
	header.version = NAVMESHSET_VERSION;
	header.numTiles = 0;
	for (int i = 0; i < mesh->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh->getTile(i);
		if (!tile || !tile->header || !tile->dataSize) continue;
		header.numTiles++;
	}
	memcpy(&header.params, mesh->getParams(), sizeof(dtNavMeshParams));
	fwrite(&header, sizeof(NavMeshSetHeader), 1, fp);

	// Store tiles.
	for (int i = 0; i < mesh->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh->getTile(i);
		if (!tile || !tile->header || !tile->dataSize) continue;

		NavMeshTileHeader tileHeader;
		tileHeader.tileRef = mesh->getTileRef(tile);
		tileHeader.dataSize = tile->dataSize;
		fwrite(&tileHeader, sizeof(tileHeader), 1, fp);

		fwrite(tile->data, tile->dataSize, 1, fp);
	}

	// todo: save params

	// todo: save convex volumes

	fclose(fp);
}


dtNavMesh* NavMeshTool::loadAll(const char* path)
{
	FILE* fp = fopen(path, "rb");
	if (!fp) return 0;

	// Read header.
	NavMeshSetHeader header;
	size_t readLen = fread(&header, sizeof(NavMeshSetHeader), 1, fp);
	if (readLen != 1)
	{
		fclose(fp);
		return 0;
	}
	if (header.magic != NAVMESHSET_MAGIC)
	{
		fclose(fp);
		return 0;
	}
	if (header.version != NAVMESHSET_VERSION)
	{
		fclose(fp);
		return 0;
	}

	dtNavMesh* mesh = dtAllocNavMesh();
	if (!mesh)
	{
		fclose(fp);
		return 0;
	}
	dtStatus status = mesh->init(&header.params);
	if (dtStatusFailed(status))
	{
		fclose(fp);
		return 0;
	}

	// Read tiles.
	for (int i = 0; i < header.numTiles; ++i)
	{
		NavMeshTileHeader tileHeader;
		readLen = fread(&tileHeader, sizeof(tileHeader), 1, fp);
		if (readLen != 1)
			return 0;

		if (!tileHeader.tileRef || !tileHeader.dataSize)
			break;

		unsigned char* data = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
		if (!data) break;
		memset(data, 0, tileHeader.dataSize);
		readLen = fread(data, tileHeader.dataSize, 1, fp);
		if (readLen != 1)
			return 0;

		dtMeshHeader* header = (dtMeshHeader*)data;

		dtStatus status = mesh->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, 0);

		m_ctx->log(RC_LOG_PROGRESS, "Read Tile: %d, %d (%d) = %d\n", header->x,
			header->y, header->layer, status);
	}

	fclose(fp);

	return mesh;
}


void NavMeshTool::ResetMesh()
{
	m_navMesh.reset();
}

void NavMeshTool::getTileStatistics(int& width, int& height, int& maxTiles) const
{
	width = m_tilesWidth;
	height = m_tilesHeight;
	maxTiles = m_maxTiles;
}

void NavMeshTool::UpdateTileSizes()
{
	if (m_geom)
	{
		const glm::vec3& bmin = m_geom->getMeshBoundsMin();
		const glm::vec3& bmax = m_geom->getMeshBoundsMax();
		int gw = 0, gh = 0;
		rcCalcGridSize(&bmin[0], &bmax[0], m_config.cellSize, &gw, &gh);
		const int ts = (int)m_config.tileSize;
		m_tilesWidth = (gw + ts - 1) / ts;
		m_tilesHeight = (gh + ts - 1) / ts;
		m_tilesCount = m_tilesWidth * m_tilesHeight;

#ifdef DT_POLYREF64
		int tileBits = DT_TILE_BITS;
		int polyBits = DT_POLY_BITS;
		m_maxTiles = 1 << tileBits;
		m_maxPolysPerTile = 1 << polyBits;
#else
		// Max tiles and max polys affect how the tile IDs are caculated.
		// There are 22 bits available for identifying a tile and a polygon.
		int tileBits = rcMin((int)ilog2(nextPow2(m_tilesCount)), 14);
		if (tileBits > 14) tileBits = 14;
		int polyBits = 22 - tileBits;
		m_maxTiles = 1 << tileBits;
		m_maxPolysPerTile = 1 << polyBits;
#endif
	}
	else
	{
		m_maxTiles = 0;
		m_maxPolysPerTile = 0;
		m_tilesWidth = 0;
		m_tilesHeight = 0;
		m_tilesCount = 0;
	}
}

void NavMeshTool::handleSettings()
{
	if (ImGui::CollapsingHeader("Mesh Properties", 0, true, true))
	{
		int tt = m_tilesWidth*m_tilesHeight;

		ImVec4 col = ImColor(255, 255, 255);
#ifndef DT_POLYREF64
		if (tt > m_maxTiles) {
			col = ImColor(255, 0, 0);
		}
		ImGui::TextColored(col, "Tile Limit: %d", m_maxTiles);
		ImGui::SameLine();
		col = ImColor(0, 255, 0);
		if (tt > m_maxTiles) {
			col = ImColor(255, 0, 0);
		}
#endif
		ImGui::TextColored(col, "%d Tiles (%d x %d)", tt, m_tilesWidth, m_tilesHeight);


		if (ImGui::TreeNodeEx("Tiling", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen)) {

			ImGui::SliderFloat("TileSize", &m_config.tileSize, 16.0f, 1024.0f, "%.0f");

			UpdateTileSizes();

			ImGui::SliderFloat("Cell Size", &m_config.cellSize, 0.1f, 1.0f);
			ImGui::SliderFloat("Cell Height", &m_config.cellHeight, 0.1f, 1.0f);
		}
		if (ImGui::TreeNodeEx("Agent", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
			ImGui::SliderFloat("Height", &m_config.agentHeight, 0.1f, 15.0f, "%.1f");
			ImGui::SliderFloat("Radius", &m_config.agentRadius, 0.1f, 15.0f, "%.1f");
			ImGui::SliderFloat("Max Climb", &m_config.agentMaxClimb, 0.1f, 15.0f, "%.1f");
			ImGui::SliderFloat("Max Slope", &m_config.agentMaxSlope, 0.0f, 90.0f, "%.1f");
		}

		if (m_geom && ImGui::TreeNodeEx("Bounding Box", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
			glm::vec3 min = m_geom->getMeshBoundsMin();
			glm::vec3 max = m_geom->getMeshBoundsMax();

			ImGui::SliderFloat("Min X", &min.x, m_geom->getRealMeshBoundsMin().x, m_geom->getRealMeshBoundsMax().x, "%.1f");
			ImGui::SliderFloat("Min Y", &min.y, m_geom->getRealMeshBoundsMin().y, m_geom->getRealMeshBoundsMax().y, "%.1f");
			ImGui::SliderFloat("Min Z", &min.z, m_geom->getRealMeshBoundsMin().z, m_geom->getRealMeshBoundsMax().z, "%.1f");
			ImGui::SliderFloat("Max X", &max.x, m_geom->getRealMeshBoundsMin().x, m_geom->getRealMeshBoundsMax().x, "%.1f");
			ImGui::SliderFloat("Max Y", &max.y, m_geom->getRealMeshBoundsMin().y, m_geom->getRealMeshBoundsMax().y, "%.1f");
			ImGui::SliderFloat("Max Z", &max.z, m_geom->getRealMeshBoundsMin().z, m_geom->getRealMeshBoundsMax().z, "%.1f");

			m_geom->setMeshBoundsMin(min);
			m_geom->setMeshBoundsMax(max);

			if (ImGui::Button("Reset Bounding Box"))
			{
				m_geom->resetMeshBounds();
			}
		}

		if (ImGui::CollapsingSubHeader("Advanced Options"))
		{
			// Region
			ImGui::Text("Region");
			ImGui::SameLine();
			static const char* RegionHelp =
				"Min Region Size:\n"
				"  - used to calculate the minimum region area (size * size)\n"
				"  - The minimum number of cells allowed to form isolated island areas.\n\n"
				"Merge Region Area:\n"
				"  - Any regions with a span count smaller than this value will, if possible,\n"
				"    be merged with larger regions.";
			ImGuiEx::HelpMarker(RegionHelp, 600.0f, ImGuiEx::ConsoleFont);

			ImGui::SliderFloat("Min Region Size", &m_config.regionMinSize, 0.0f, 150.0f, "%.0f");
			ImGui::SliderFloat("Merged Region Size", &m_config.regionMergeSize, 0.0f, 150.0f, "%.0f");
			
			// Partitioning
			ImGui::Text("Partitioning");
			ImGui::SameLine();
			static const char* PartitioningHelp =
				"1) Watershed partitioning\n"
				"  - the classic Recast partitioning\n"
				"  - creates the nicest tessellation\n"
				"  - usually slowest\n"
				"  - partitions the heightfield into nice regions without holes or overlaps\n"
				"  - the are some corner cases where this method creates produces holes and overlaps\n"
				"     - holes may appear when a small obstacles is close to large open area\n"
				"       (triangulation can handle this)\n"
				"     - overlaps may occur if you have narrow spiral corridors (i.e stairs), this\n"
				"       make triangulation to fail\n"
				"  * generally the best choice if you precompute the nacmesh, use this if you have\n"
				"    large open areas\n\n"
				"2) Monotone partitioning\n"
				"  - fastest\n"
				"  - partitions the heightfield into regions without holes and overlaps (guaranteed)\n"
				"  - creates long thin polygons, which sometimes causes paths with detours\n"
				"  * use this if you want fast navmesh generation\n\n"
				"3) Layer partitioning\n"
				"  - quite fast\n"
				"  - partitions the heighfield into non-overlapping regions\n"
				"  - relies on the triangulation code to cope with holes (thus slower than monotone\n"
				"    partitioning)\n"
				"  - produces better triangles than monotone partitioning\n"
				"  - does not have the corner cases of watershed partitioning\n"
				"  - can be slow and create a bit ugly tessellation (still better than monotone)\n"
				"    if you have large open areas with small obstacles (not a problem if you use\n"
				"    tiles)\n"
				"  * good choice to use for tiled navmesh with medium and small sized tiles";
			ImGuiEx::HelpMarker(PartitioningHelp, 600.0f, ImGuiEx::ConsoleFont);

			const char *partition_types[] = { "Watershed", "Monotone", "Layers" };
			ImGui::Combo("Partition Type", (int*)&m_config.partitionType, partition_types, 3);

			// Polygonization
			ImGui::Text("Polygonization");
			ImGui::SameLine();
			static const char* PolygonizationHelp =
				"Edge Max Length:\n"
				"  - The maximum allowed length for contour edges along the border of a mesh.\n\n"
				"Max Edge Error:\n"
				"  - The maximum distance a simplified contour's border edges should deviate the\n"
				"    original raw contour.\n\n"
				"Verts Per Poly:\n"
				"  - The maximum number of vertices allowed for polygons generatd during the\n"
				"    contour to polygon conversion process.\n";
			ImGuiEx::HelpMarker(PolygonizationHelp, 600.0f, ImGuiEx::ConsoleFont);

			ImGui::SliderFloat("Max Edge Length", &m_config.edgeMaxLen, 0.0f, 50.0f, "%.0f");
			ImGui::SliderFloat("Max Edge Error", &m_config.edgeMaxError, 0.1f, 3.0f, "%.1f");
			ImGui::SliderFloat("Verts Per Poly", &m_config.vertsPerPoly, 3.0f, 12.0f, "%.0f");

			// Detail Mesh
			ImGui::Text("Detail Mesh");
			ImGui::SameLine();
			static const char* DetailMeshHelp =
				"Sample Distance:\n"
				"  - Sets the sampling distance to use when generating the detail mesh.\n"
				"    (for height detail only).\n\n"
				"Sample Max Error:\n"
				"  - The maximum distance the detail mesh surface should deviate from\n"
				"    heightfield data. (for height detail only).\n";
			ImGuiEx::HelpMarker(DetailMeshHelp, 600.0f, ImGuiEx::ConsoleFont);

			ImGui::SliderFloat("Sample Distance", &m_config.detailSampleDist, 0.0f, 0.9f, "%.2f");
			ImGui::SliderFloat("Max Sample Error", &m_config.detailSampleMaxError, 0.0f, 100.0f, "%.1f");
		}
	}

	if (ImGui::CollapsingHeader("Debug"))
	{
		// Check which modes are valid.
		bool valid[DrawMode::MAX];
		for (int i = 0; i < DrawMode::MAX; ++i)
			valid[i] = false;

		if (m_geom)
		{
			valid[DrawMode::NAVMESH] = m_navMesh != 0;
			valid[DrawMode::NAVMESH_TRANS] = m_navMesh != 0;
			valid[DrawMode::NAVMESH_BVTREE] = m_navMesh != 0;
			valid[DrawMode::NAVMESH_NODES] = m_navQuery != 0;
			valid[DrawMode::NAVMESH_PORTALS] = m_navMesh != 0;
			valid[DrawMode::NAVMESH_INVIS] = m_navMesh != 0;
			valid[DrawMode::MESH] = true;

			//valid[DrawMode::VOXELS] = false; // m_solid != 0;
			//valid[DrawMode::VOXELS_WALKABLE] = false; // = m_solid != 0;
			//valid[DrawMode::COMPACT] = false; // m_chf != 0;
			//valid[DrawMode::COMPACT_DISTANCE] = false; // m_chf != 0;
			//valid[DrawMode::COMPACT_REGIONS] = false; // m_chf != 0;
			//valid[DrawMode::REGION_CONNECTIONS] = false; // m_cset != 0;
			//valid[DrawMode::RAW_CONTOURS] = false; // m_cset != 0;
			//valid[DrawMode::BOTH_CONTOURS] = false; // m_cset != 0;
			//valid[DrawMode::CONTOURS] = false; // m_cset != 0;
			//valid[DrawMode::POLYMESH] = false; // m_pmesh != 0;
			//valid[DrawMode::POLYMESH_DETAIL] = false; // m_dmesh != 0;
		}

		int unavail = 0;
		for (int i = 0; i < DrawMode::MAX; ++i)
		{
			if (!valid[i]) unavail++;
		}

		if (unavail != DrawMode::MAX)
		{
			ImGui::Text("Draw");

			if (valid[DrawMode::MESH] && ImGui::RadioButton("Input Mesh", m_drawMode == DrawMode::MESH))
				m_drawMode = DrawMode::MESH;
			if (valid[DrawMode::NAVMESH] && ImGui::RadioButton("Navmesh", m_drawMode == DrawMode::NAVMESH))
				m_drawMode = DrawMode::NAVMESH;
			if (valid[DrawMode::NAVMESH_INVIS] && ImGui::RadioButton("Navmesh Invis", m_drawMode == DrawMode::NAVMESH_INVIS))
				m_drawMode = DrawMode::NAVMESH_INVIS;
			if (valid[DrawMode::NAVMESH_TRANS] && ImGui::RadioButton("Navmesh Trans", m_drawMode == DrawMode::NAVMESH_TRANS))
				m_drawMode = DrawMode::NAVMESH_TRANS;
			if (valid[DrawMode::NAVMESH_BVTREE] && ImGui::RadioButton("Navmesh BVTree", m_drawMode == DrawMode::NAVMESH_BVTREE))
				m_drawMode = DrawMode::NAVMESH_BVTREE;
			if (valid[DrawMode::NAVMESH_NODES] && ImGui::RadioButton("Navmesh Nodes", m_drawMode == DrawMode::NAVMESH_NODES))
				m_drawMode = DrawMode::NAVMESH_NODES;
			if (valid[DrawMode::NAVMESH_PORTALS] && ImGui::RadioButton("Navmesh Portals", m_drawMode == DrawMode::NAVMESH_PORTALS))
				m_drawMode = DrawMode::NAVMESH_PORTALS;
			//if (valid[DrawMode::VOXELS] && ImGui::RadioButton("Voxels", m_drawMode == DrawMode::VOXELS))
			//	m_drawMode = DrawMode::VOXELS;
			//if (valid[DrawMode::VOXELS_WALKABLE] && ImGui::RadioButton("Walkable Voxels", m_drawMode == DrawMode::VOXELS_WALKABLE))
			//	m_drawMode = DrawMode::VOXELS_WALKABLE;
			//if (valid[DrawMode::COMPACT] && ImGui::RadioButton("Compact", m_drawMode == DrawMode::COMPACT))
			//	m_drawMode = DrawMode::COMPACT;
			//if (valid[DrawMode::COMPACT_DISTANCE] && ImGui::RadioButton("Compact Distance", m_drawMode == DrawMode::COMPACT_DISTANCE))
			//	m_drawMode = DrawMode::COMPACT_DISTANCE;
			//if (valid[DrawMode::COMPACT_REGIONS] && ImGui::RadioButton("Compact Regions", m_drawMode == DrawMode::COMPACT_REGIONS))
			//	m_drawMode = DrawMode::COMPACT_REGIONS;
			//if (valid[DrawMode::REGION_CONNECTIONS] && ImGui::RadioButton("Region Connections", m_drawMode == DrawMode::REGION_CONNECTIONS))
			//	m_drawMode = DrawMode::REGION_CONNECTIONS;
			//if (valid[DrawMode::RAW_CONTOURS] && ImGui::RadioButton("Raw Contours", m_drawMode == DrawMode::RAW_CONTOURS))
			//	m_drawMode = DrawMode::RAW_CONTOURS;
			//if (valid[DrawMode::BOTH_CONTOURS] && ImGui::RadioButton("Both Contours", m_drawMode == DrawMode::BOTH_CONTOURS))
			//	m_drawMode = DrawMode::BOTH_CONTOURS;
			//if (valid[DrawMode::CONTOURS] && ImGui::RadioButton("Contours", m_drawMode == DrawMode::CONTOURS))
			//	m_drawMode = DrawMode::CONTOURS;
			//if (valid[DrawMode::POLYMESH] && ImGui::RadioButton("Poly Mesh", m_drawMode == DrawMode::POLYMESH))
			//	m_drawMode = DrawMode::POLYMESH;
			//if (valid[DrawMode::POLYMESH_DETAIL] && ImGui::RadioButton("Poly Mesh Detail", m_drawMode == DrawMode::POLYMESH_DETAIL))
			//	m_drawMode = DrawMode::POLYMESH_DETAIL;
		}
	}
}

void NavMeshTool::handleTools()
{
	ToolType type = !m_tool ? ToolType::NONE : m_tool->type();

	if (ImGui::RadioButton("Test Navmesh", type == ToolType::NAVMESH_TESTER))
	{
		setTool(new NavMeshTesterTool);
	}
	if (ImGui::RadioButton("Prune Navmesh", type == ToolType::NAVMESH_PRUNE))
	{
		setTool(new NavMeshPruneTool);
	}
	if (ImGui::RadioButton("Create Tiles", type == ToolType::TILE_EDIT))
	{
		setTool(new NavMeshTileTool);
	}
	if (ImGui::RadioButton("Create Off-Mesh Links", type == ToolType::OFFMESH_CONNECTION))
	{
		setTool(new OffMeshConnectionTool);
	}
	if (ImGui::RadioButton("Create Convex Volumes", type == ToolType::CONVEX_VOLUME))
	{
		setTool(new ConvexVolumeTool);
	}

	ImGui::Separator();

	if (m_tool)
	{
		m_tool->handleMenu();
	}
}

void NavMeshTool::handleRender()
{
	if (!m_geom || !m_geom->getMeshLoader())
		return;

	DebugDrawGL dd;

	const float texScale = 1.0f / (m_config.cellSize * 10.0f);

	// Draw mesh
	if (m_drawMode != DrawMode::NAVMESH_TRANS)
	{
		// Draw mesh
		duDebugDrawTriMeshSlope(&dd,
			m_geom->getMeshLoader()->getVerts(),
			m_geom->getMeshLoader()->getVertCount(),
			m_geom->getMeshLoader()->getTris(),
			m_geom->getMeshLoader()->getNormals(),
			m_geom->getMeshLoader()->getTriCount(),
			m_config.agentMaxSlope,
			texScale);
		m_geom->drawOffMeshConnections(&dd);
	}

	dd.depthMask(false);

	// Draw bounds
	const glm::vec3& bmin = m_geom->getMeshBoundsMin();
	const glm::vec3& bmax = m_geom->getMeshBoundsMax();
	duDebugDrawBoxWire(&dd, bmin[0],bmin[1],bmin[2], bmax[0],bmax[1],bmax[2], duRGBA(255,255,255,128), 1.0f);

	// Tiling grid.
	int gw = 0, gh = 0;
	rcCalcGridSize(&bmin[0], &bmax[0], m_config.cellSize, &gw, &gh);
	const int tw = (gw + (int)m_config.tileSize-1) / (int)m_config.tileSize;
	const int th = (gh + (int)m_config.tileSize-1) / (int)m_config.tileSize;
	const float s = m_config.tileSize * m_config.cellSize;
	duDebugDrawGridXZ(&dd, bmin[0],bmin[1],bmin[2], tw,th, s, duRGBA(0,0,0,64), 1.0f);

	// Draw active tile
	duDebugDrawBoxWire(&dd, m_tileBmin[0],m_tileBmin[1],m_tileBmin[2],
					   m_tileBmax[0],m_tileBmax[1],m_tileBmax[2], m_tileCol, 1.0f);

	if (m_navMesh && m_navQuery &&
		(m_drawMode == DrawMode::NAVMESH ||
		 m_drawMode == DrawMode::NAVMESH_TRANS ||
		 m_drawMode == DrawMode::NAVMESH_BVTREE ||
		 m_drawMode == DrawMode::NAVMESH_NODES ||
		 m_drawMode == DrawMode::NAVMESH_PORTALS ||
		 m_drawMode == DrawMode::NAVMESH_INVIS))
	{
		if (m_drawMode != DrawMode::NAVMESH_INVIS)
			duDebugDrawNavMeshWithClosedList(&dd, *m_navMesh, *m_navQuery, m_navMeshDrawFlags);
		if (m_drawMode == DrawMode::NAVMESH_BVTREE)
			duDebugDrawNavMeshBVTree(&dd, *m_navMesh);
		if (m_drawMode == DrawMode::NAVMESH_PORTALS)
			duDebugDrawNavMeshPortals(&dd, *m_navMesh);
		if (m_drawMode == DrawMode::NAVMESH_NODES)
			duDebugDrawNavMeshNodes(&dd, *m_navQuery);
		duDebugDrawNavMeshPolysWithFlags(&dd, *m_navMesh, PolyFlags::Disabled, duRGBA(0,0,0,128));
	}

	m_geom->drawConvexVolumes(&dd);

	if (m_tool)
		m_tool->handleRender();
	renderToolStates();
}

void NavMeshTool::handleRenderOverlay(const glm::mat4& proj,
	const glm::mat4& model, const glm::ivec4& view)
{
	// Draw start and end point labels
	if (m_tileBuildTime > 0.0f)
	{
		glm::vec3 pos = glm::project((m_tileBmin + m_tileBmax) / 2.0f,
			model, proj, view);
		ImGui::RenderText((int)pos.x, -((int)pos.y - 25), ImVec4(0, 0, 0, 220),
			"%.3fms / %dTris / %.1fkB", m_tileBuildTime, m_tileTriCount, m_tileMemUsage);
	}

	if (m_tool)
	{
		m_tool->handleRenderOverlay(proj, model, view);
	}

	renderOverlayToolStates(proj, model, view);
}

void NavMeshTool::handleMeshChanged(class InputGeom* geom)
{
	m_navMesh.reset();
	m_geom = geom;

	if (m_tool)
	{
		m_tool->reset();
		m_tool->init(this);
	}

	resetToolStates();
	initToolStates();
}

bool NavMeshTool::handleBuild()
{
	if (!m_geom || !m_geom->getMeshLoader())
	{
		m_ctx->log(RC_LOG_ERROR, "buildTiledNavigation: No vertices and triangles.");
		return false;
	}

	m_navMesh.reset();

	dtNavMesh* mesh = dtAllocNavMesh();
	if (!mesh)
	{
		m_ctx->log(RC_LOG_ERROR, "buildTiledNavigation: Could not allocate navmesh.");
		return false;
	}
	setNavMesh(mesh);

	dtNavMeshParams params;
	rcVcopy(params.orig, glm::value_ptr(m_geom->getMeshBoundsMin()));
	params.tileWidth = m_config.tileSize * m_config.cellSize;
	params.tileHeight = m_config.tileSize * m_config.cellSize;
	params.maxTiles = m_tilesWidth * m_tilesHeight;
	params.maxPolys = m_maxPolysPerTile * params.maxTiles;

	dtStatus status;

	status = m_navMesh->init(&params);
	if (dtStatusFailed(status))
	{
		m_ctx->log(RC_LOG_ERROR, "buildTiledNavigation: Could not init navmesh.");
		return false;
	}

	status = m_navQuery->init(m_navMesh.get(), MAX_NODES);
	if (dtStatusFailed(status))
	{
		m_ctx->log(RC_LOG_ERROR, "buildTiledNavigation: Could not init Detour navmesh query");
		return false;
	}

	buildAllTiles();

	if (m_tool)
	{
		m_tool->init(this);
	}

	initToolStates();

	return true;
}

void NavMeshTool::getTilePos(const float* pos, int& tx, int& ty)
{
	if (!m_geom) return;

	const glm::vec3& bmin = m_geom->getMeshBoundsMin();

	const float ts = m_config.tileSize * m_config.cellSize;
	tx = (int)((pos[0] - bmin[0]) / ts);
	ty = (int)((pos[2] - bmin[2]) / ts);
}

void NavMeshTool::removeTile(const float* pos)
{
	if (!m_geom) return;
	if (!m_navMesh) return;

	const glm::vec3& bmin = m_geom->getMeshBoundsMin();
	const glm::vec3& bmax = m_geom->getMeshBoundsMax();

	const float ts = m_config.tileSize * m_config.cellSize;
	const int tx = (int)((pos[0] - bmin[0]) / ts);
	const int ty = (int)((pos[2] - bmin[2]) / ts);

	m_tileBmin[0] = bmin[0] + tx*ts;
	m_tileBmin[1] = bmin[1];
	m_tileBmin[2] = bmin[2] + ty*ts;

	m_tileBmax[0] = bmin[0] + (tx+1)*ts;
	m_tileBmax[1] = bmax[1];
	m_tileBmax[2] = bmin[2] + (ty+1)*ts;

	m_tileCol = duRGBA(128,32,16,64);

	m_navMesh->removeTile(m_navMesh->getTileRefAt(tx,ty,0),0,0);
}

void NavMeshTool::removeAllTiles()
{
	const glm::vec3& bmin = m_geom->getMeshBoundsMin();
	const glm::vec3& bmax = m_geom->getMeshBoundsMax();
	int gw = 0, gh = 0;
	rcCalcGridSize(&bmin[0], &bmax[0], m_config.cellSize, &gw, &gh);
	const int ts = (int)m_config.tileSize;
	const int tw = (gw + ts-1) / ts;
	const int th = (gh + ts-1) / ts;

	for (int y = 0; y < th; ++y)
	{
		for (int x = 0; x < tw; ++x)
			m_navMesh->removeTile(m_navMesh->getTileRefAt(x, y, 0), 0, 0);
	}
}

void NavMeshTool::cancelBuildAllTiles(bool wait)
{
	if (m_buildingTiles)
		m_cancelTiles = true;

	if (wait && m_buildThread.joinable())
		m_buildThread.join();
}

void NavMeshTool::setOutputPath(const char* output_path)
{
	strcpy(m_outputPath, output_path);
	char buffer[512];
	sprintf(buffer, "%s\\MQ2Nav", output_path);
	CreateDirectory(buffer, NULL);
}

void NavMeshTool::resetCommonSettings()
{
	m_config = NavMeshConfig{};
	m_navMeshDrawFlags = DU_DRAWNAVMESH_OFFMESHCONS | DU_DRAWNAVMESH_CLOSEDLIST;
}

//----------------------------------------------------------------------------

void NavMeshTool::buildTile(const float* pos)
{
	if (!m_geom) return;
	if (!m_navMesh) return;

	const glm::vec3& bmin = m_geom->getMeshBoundsMin();
	const glm::vec3& bmax = m_geom->getMeshBoundsMax();

	const float ts = m_config.tileSize * m_config.cellSize;
	const int tx = (int)((pos[0] - bmin[0]) / ts);
	const int ty = (int)((pos[2] - bmin[2]) / ts);

	m_tileBmin[0] = bmin[0] + tx*ts;
	m_tileBmin[1] = bmin[1];
	m_tileBmin[2] = bmin[2] + ty*ts;

	m_tileBmax[0] = bmin[0] + (tx + 1)*ts;
	m_tileBmax[1] = bmax[1];
	m_tileBmax[2] = bmin[2] + (ty + 1)*ts;

	m_tileCol = duRGBA(255, 255, 255, 64);

	m_ctx->resetLog();

	int dataSize = 0;
	unsigned char* data = buildTileMesh(tx, ty, glm::value_ptr(m_tileBmin),
		glm::value_ptr(m_tileBmax), dataSize);

	// Remove any previous data (navmesh owns and deletes the data).
	m_navMesh->removeTile(m_navMesh->getTileRefAt(tx, ty, 0), 0, 0);

	// Add tile, or leave the location empty.
	if (data)
	{
		// Let the navmesh own the data.
		dtStatus status = m_navMesh->addTile(data, dataSize, DT_TILE_FREE_DATA, 0, 0);
		if (dtStatusFailed(status))
			dtFree(data);
	}

	m_ctx->dumpLog("Build Tile (%d,%d):", tx, ty);
}


struct TileData
{
	unsigned char* data = 0;
	int length = 0;
	int x = 0;
	int y = 0;
};

typedef std::shared_ptr<TileData> TileDataPtr;

class NavmeshUpdaterAgent : public concurrency::agent
{
public:
	explicit NavmeshUpdaterAgent(concurrency::ISource<TileDataPtr>& dataSource,
		dtNavMesh* navMesh)
		: m_source(dataSource)
		, m_navMesh(navMesh)
	{
	}

protected:
	virtual void run() override
	{
		TileDataPtr data = receive(m_source);
		while (data != nullptr)
		{
			// Remove any previous data (navmesh owns and deletes the data).
			m_navMesh->removeTile(m_navMesh->getTileRefAt(data->x, data->y, 0), 0, 0);

			// Let the navmesh own the data.
			dtStatus status = m_navMesh->addTile(data->data, data->length, DT_TILE_FREE_DATA, 0, 0);
			if (dtStatusFailed(status))
			{
				dtFree(data->data);
			}

			data = receive(m_source);
		}

		done();
	}

private:
	concurrency::ISource<TileDataPtr>& m_source;
	dtNavMesh* m_navMesh;
};

void NavMeshTool::buildAllTiles(bool async)
{
	if (!m_geom) return;
	if (!m_navMesh) return;
	if (m_buildingTiles) return;

	// if async, invoke on a new thread
	if (async)
	{
		if (m_buildThread.joinable())
			m_buildThread.join();

		m_buildThread = std::thread([this]()
		{
			buildAllTiles(false);
		});
		return;
	}

	m_buildingTiles = true;
	m_cancelTiles = false;

	const glm::vec3& bmin = m_geom->getMeshBoundsMin();
	const glm::vec3& bmax = m_geom->getMeshBoundsMax();
	int gw = 0, gh = 0;
	rcCalcGridSize(&bmin[0], &bmax[0], m_config.cellSize, &gw, &gh);
	const int ts = (int)m_config.tileSize;
	const int tw = (gw + ts - 1) / ts;
	const int th = (gh + ts - 1) / ts;
	const float tcs = m_config.tileSize * m_config.cellSize;

	m_tilesBuilt = 0;

	//concurrency::CurrentScheduler::Create(concurrency::SchedulerPolicy(1, concurrency::MaxConcurrency, 3));

	concurrency::unbounded_buffer<TileDataPtr> agentTiles;
	NavmeshUpdaterAgent updater_agent(agentTiles, m_navMesh.get());

	updater_agent.start();

	// Start the build process.
	m_ctx->startTimer(RC_TIMER_TEMP);

	concurrency::task_group tasks;
	for (int x = 0; x < tw; x++)
	{
		for (int y = 0; y < th; y++)
		{
			tasks.run([this, x, y, &bmin, &bmax, tcs, &agentTiles]()
			{
				if (m_cancelTiles)
					return;

				++m_tilesBuilt;

				m_tileBmin[0] = bmin[0] + x*tcs;
				m_tileBmin[1] = bmin[1];
				m_tileBmin[2] = bmin[2] + y*tcs;

				m_tileBmax[0] = bmin[0] + (x + 1)*tcs;
				m_tileBmax[1] = bmax[1];
				m_tileBmax[2] = bmin[2] + (y + 1)*tcs;

				int dataSize = 0;
				uint8_t* data = buildTileMesh(x, y, glm::value_ptr(m_tileBmin),
					glm::value_ptr(m_tileBmax), dataSize);

				if (data)
				{
					TileDataPtr tileData = std::make_shared<TileData>();
					tileData->data = data;
					tileData->length = dataSize;
					tileData->x = x;
					tileData->y = y;

					asend(agentTiles, tileData);
				}
			});
		}
	}

	tasks.wait();

	// send terminator
	asend(agentTiles, TileDataPtr());

	concurrency::agent::wait(&updater_agent);


	// Start the build process.
	m_ctx->stopTimer(RC_TIMER_TEMP);

	m_totalBuildTimeMs = m_ctx->getAccumulatedTime(RC_TIMER_TEMP) / 1000.0f;

	m_buildingTiles = false;
}

deleting_unique_ptr<rcCompactHeightfield> NavMeshTool::rasterizeGeometry(rcConfig& cfg) const
{
	// Allocate voxel heightfield where we rasterize our input data to.
	deleting_unique_ptr<rcHeightfield> solid(rcAllocHeightfield(),
		[](rcHeightfield* hf) { rcFreeHeightField(hf); });

	if (!rcCreateHeightfield(m_ctx, *solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not create solid heightfield.");
		return 0;
	}

	const float* verts = m_geom->getMeshLoader()->getVerts();
	const int nverts = m_geom->getMeshLoader()->getVertCount();
	const int ntris = m_geom->getMeshLoader()->getTriCount();
	const rcChunkyTriMesh* chunkyMesh = m_geom->getChunkyMesh();

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

	//m_tileTriCount = 0;

	for (int i = 0; i < ncid; ++i)
	{
		const rcChunkyTriMeshNode& node = chunkyMesh->nodes[cid[i]];
		const int* ctris = &chunkyMesh->tris[node.i * 3];
		const int nctris = node.n;

		//m_tileTriCount += nctris;

		memset(triareas.get(), 0, nctris * sizeof(unsigned char));
		rcMarkWalkableTriangles(m_ctx, cfg.walkableSlopeAngle,
			verts, nverts, ctris, nctris, triareas.get());

		rcRasterizeTriangles(m_ctx, verts, nverts, ctris, triareas.get(), nctris, *solid, cfg.walkableClimb);
	}

	// Once all geometry is rasterized, we do initial pass of filtering to
	// remove unwanted overhangs caused by the conservative rasterization
	// as well as filter spans where the character cannot possibly stand.
	rcFilterLowHangingWalkableObstacles(m_ctx, cfg.walkableClimb, *solid);
	rcFilterLedgeSpans(m_ctx, cfg.walkableHeight, cfg.walkableClimb, *solid);
	rcFilterWalkableLowHeightSpans(m_ctx, cfg.walkableHeight, *solid);

	// Compact the heightfield so that it is faster to handle from now on.
	// This will result more cache coherent data as well as the neighbours
	// between walkable cells will be calculated.
	deleting_unique_ptr<rcCompactHeightfield> chf(rcAllocCompactHeightfield(),
		[](rcCompactHeightfield* hf) { rcFreeCompactHeightfield(hf); });

	if (!rcBuildCompactHeightfield(m_ctx, cfg.walkableHeight, cfg.walkableClimb, *solid, *chf))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build compact data.");
		return 0;
	}

	return std::move(chf);
}

unsigned char* NavMeshTool::buildTileMesh(const int tx, const int ty, const float* bmin, const float* bmax, int& dataSize) const
{
	if (!m_geom || !m_geom->getMeshLoader() || !m_geom->getChunkyMesh())
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Input mesh is not specified.");
		return 0;
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
	cfg.bmin[0] -= cfg.borderSize*cfg.cs;
	cfg.bmin[2] -= cfg.borderSize*cfg.cs;
	cfg.bmax[0] += cfg.borderSize*cfg.cs;
	cfg.bmax[2] += cfg.borderSize*cfg.cs;

	// Reset build times gathering.
	m_ctx->resetTimers();

	// Start the build process.
	m_ctx->startTimer(RC_TIMER_TOTAL);

#if 0
	m_ctx->log(RC_LOG_PROGRESS, "Building navigation:");
	m_ctx->log(RC_LOG_PROGRESS, " - %d x %d cells", m_cfg.width, m_cfg.height);
	m_ctx->log(RC_LOG_PROGRESS, " - %.1fK verts, %.1fK tris", nverts / 1000.0f, ntris / 1000.0f);
#endif

	deleting_unique_ptr<rcCompactHeightfield> chf = rasterizeGeometry(cfg);
	if (!chf)
		return 0;

	// Erode the walkable area by agent radius.
	if (!rcErodeWalkableArea(m_ctx, cfg.walkableRadius, *chf))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not erode.");
		return 0;
	}

	// (Optional) Mark areas.
	const ConvexVolume* vols = m_geom->getConvexVolumes();
	for (int i = 0; i < m_geom->getConvexVolumeCount(); ++i)
		rcMarkConvexPolyArea(m_ctx, &vols[i].verts[0][0], vols[i].nverts, vols[i].hmin, vols[i].hmax, (unsigned char)vols[i].area, *chf);


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
		if (!rcBuildDistanceField(m_ctx, *chf))
		{
			m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build distance field.");
			return false;
		}

		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildRegions(m_ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
		{
			m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build watershed regions.");
			return false;
		}
	}
	else if (m_config.partitionType == PartitionType::MONOTONE)
	{
		// Partition the walkable surface into simple regions without holes.
		// Monotone partitioning does not need distancefield.
		if (!rcBuildRegionsMonotone(m_ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
		{
			m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build monotone regions.");
			return false;
		}
	}
	else // PartitionType::LAYERS
	{
		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildLayerRegions(m_ctx, *chf, cfg.borderSize, cfg.minRegionArea))
		{
			m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build layer regions.");
			return false;
		}
	}

	// Create contours.
	deleting_unique_ptr<rcContourSet> cset(rcAllocContourSet(), [](rcContourSet* cs) { rcFreeContourSet(cs); });
	if (!rcBuildContours(m_ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not create contours.");
		return 0;
	}

	if (cset->nconts == 0)
	{
		return 0;
	}

	// Build polygon navmesh from the contours.
	deleting_unique_ptr<rcPolyMesh> pmesh(rcAllocPolyMesh(), [](rcPolyMesh* pm) { rcFreePolyMesh(pm); });
	if (!rcBuildPolyMesh(m_ctx, *cset, cfg.maxVertsPerPoly, *pmesh))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not triangulate contours.");
		return 0;
	}

	// Build detail mesh.
	deleting_unique_ptr<rcPolyMeshDetail> dmesh(rcAllocPolyMeshDetail(), [](rcPolyMeshDetail* pm) { rcFreePolyMeshDetail(pm); });
	if (!rcBuildPolyMeshDetail(m_ctx, *pmesh, *chf,
		cfg.detailSampleDist, cfg.detailSampleMaxError,
		*dmesh))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could build polymesh detail.");
		return 0;
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
			m_ctx->log(RC_LOG_ERROR, "Too many vertices per tile %d (max: %d).", pmesh->nverts, 0xffff);
			return 0;
		}

		// Update poly flags from areas.
		for (int i = 0; i < pmesh->npolys; ++i)
		{
			if (pmesh->areas[i] == RC_WALKABLE_AREA)
				pmesh->areas[i] = PolyArea::Ground;

			if (pmesh->areas[i] == PolyArea::Ground ||
				pmesh->areas[i] == PolyArea::Grass ||
				pmesh->areas[i] == PolyArea::Road)
			{
				pmesh->flags[i] = PolyFlags::Walk;
			}
			else if (pmesh->areas[i] == PolyArea::Water)
			{
				pmesh->flags[i] = PolyFlags::Swim;
			}
			else if (pmesh->areas[i] == PolyArea::Door)
			{
				pmesh->flags[i] = PolyFlags::Walk | PolyFlags::Door;
			}
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
		params.offMeshConVerts = m_geom->getOffMeshConnectionVerts();
		params.offMeshConRad = m_geom->getOffMeshConnectionRads();
		params.offMeshConDir = m_geom->getOffMeshConnectionDirs();
		params.offMeshConAreas = m_geom->getOffMeshConnectionAreas();
		params.offMeshConFlags = m_geom->getOffMeshConnectionFlags();
		params.offMeshConUserID = m_geom->getOffMeshConnectionId();
		params.offMeshConCount = m_geom->getOffMeshConnectionCount();
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
			m_ctx->log(RC_LOG_ERROR, "Could not build Detour navmesh.");
			return 0;
		}
	}
	//m_tileMemUsage = navDataSize/1024.0f;

	m_ctx->stopTimer(RC_TIMER_TOTAL);

	// Show performance stats.
	//duLogBuildTimes(*m_ctx, m_ctx->getAccumulatedTime(RC_TIMER_TOTAL));
	//m_ctx->log(RC_LOG_PROGRESS, ">> Polymesh: %d vertices  %d polygons", pmesh->nverts, pmesh->npolys);

	//m_tileBuildTime = m_ctx->getAccumulatedTime(RC_TIMER_TOTAL)/1000.0f;

	dataSize = navDataSize;
	return navData;
}


//----------------------------------------------------------------------------

void NavMeshTool::setTool(Tool* tool)
{
	m_tool.reset(tool);

	if (tool)
	{
		m_tool->init(this);
	}
}

ToolState* NavMeshTool::getToolState(ToolType type) const
{
	auto iter = m_toolStates.find(type);
	if (iter != m_toolStates.end())
		return iter->second.get();
	return nullptr;
}

void NavMeshTool::setToolState(ToolType type, ToolState* s)
{
	m_toolStates[type] = std::unique_ptr<ToolState>(s);
}

void NavMeshTool::setNavMesh(dtNavMesh* mesh)
{
	m_navMesh = deleting_unique_ptr<dtNavMesh>(mesh,
		[](dtNavMesh* m) { dtFreeNavMesh(m); });
}

void NavMeshTool::handleClick(const glm::vec3& s, const glm::vec3& p, bool shift)
{
	if (m_tool)
	{
		m_tool->handleClick(s, p, shift);
	}
}

void NavMeshTool::handleToggle()
{
	if (m_tool)
	{
		m_tool->handleToggle();
	}
}

void NavMeshTool::handleStep()
{
	if (m_tool)
	{
		m_tool->handleStep();
	}
}

void NavMeshTool::handleUpdate(float dt)
{
	if (m_tool)
	{
		m_tool->handleUpdate(dt);
	}

	for (const auto& p : m_toolStates)
	{
		if (p.second)
		{
			p.second->handleUpdate(dt);
		}
	}
}

void NavMeshTool::initToolStates()
{
	for (const auto& p : m_toolStates)
	{
		if (p.second)
		{
			p.second->init(this);
		}
	}
}

void NavMeshTool::resetToolStates()
{
	for (const auto& p : m_toolStates)
	{
		if (p.second)
		{
			p.second->reset();
		}
	}
}

void NavMeshTool::renderToolStates()
{
	for (const auto& p : m_toolStates)
	{
		if (p.second)
		{
			p.second->handleRender();
		}
	}
}

void NavMeshTool::renderOverlayToolStates(const glm::mat4& proj,
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