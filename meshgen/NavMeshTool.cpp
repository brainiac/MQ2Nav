
#include "meshgen/NavMeshTool.h"
#include "meshgen/Application.h"
#include "meshgen/ConvexVolumeTool.h"
#include "meshgen/InputGeom.h"
#include "meshgen/NavMeshInfoTool.h"
#include "meshgen/NavMeshPruneTool.h"
#include "meshgen/NavMeshTesterTool.h"
#include "meshgen/NavMeshTileTool.h"
#include "meshgen/OffMeshConnectionTool.h"
#include "meshgen/WaypointsTool.h"
#include "common/NavMeshData.h"
#include "common/Utilities.h"
#include "common/proto/NavMeshFile.pb.h"

#include <Recast.h>
#include <RecastDebugDraw.h>
#include <DetourCommon.h>
#include <DetourDebugDraw.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <imgui/misc/fonts/IconsMaterialDesign.h>

#include <SDL.h>
#include <imgui/imgui.h>
#include <imgui/custom/imgui_user.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <ppl.h>
#include <agents.h>
#include <mutex>
#include <fstream>

//----------------------------------------------------------------------------

NavMeshTool::NavMeshTool(const std::shared_ptr<NavMesh>& navMesh)
	: m_navMesh(navMesh)
{
	resetCommonSettings();

	m_outputPath = new char[MAX_PATH];
	setTool(new NavMeshTileTool);

	m_logger = spdlog::default_logger()->clone("NavMeshTool");

	UpdateTileSizes();

	m_navMeshConn = m_navMesh->OnNavMeshChanged.Connect([this]()
		{ NavMeshUpdated(); });
}

NavMeshTool::~NavMeshTool()
{
	delete[] m_outputPath;
}

//----------------------------------------------------------------------------

void NavMeshTool::NavMeshUpdated()
{
	if (!m_navMesh->IsNavMeshLoadedFromDisk())
	{
		if (m_geom)
		{
			// reset to default
			m_navMesh->SetNavMeshBounds(
				m_geom->getMeshBoundsMin(),
				m_geom->getMeshBoundsMax());
		}

		m_navMesh->GetNavMeshConfig() = m_config;
	}
	else
	{
		m_config = m_navMesh->GetNavMeshConfig();
	}

	if (m_tool)
	{
		m_tool->init(this);
	}
	initToolStates();
}

void NavMeshTool::getTileStatistics(int& width, int& height, int& maxTiles) const
{
	width = m_tilesWidth;
	height = m_tilesHeight;
	maxTiles = m_maxTiles;
}

void NavMeshTool::UpdateTileSizes()
{
	if (m_navMesh)
	{
		glm::vec3 bmin, bmax;
		m_navMesh->GetNavMeshBounds(bmin, bmax);
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

void NavMeshTool::handleDebug()
{
	// Check which modes are valid.
	bool valid[DrawMode::MAX];
	for (int i = 0; i < DrawMode::MAX; ++i)
		valid[i] = false;

	if (m_geom)
	{
		bool isValid = m_navMesh->IsNavMeshLoaded();

		valid[DrawMode::NAVMESH] = isValid;
		valid[DrawMode::NAVMESH_TRANS] = isValid;
		valid[DrawMode::NAVMESH_BVTREE] = isValid;
		valid[DrawMode::NAVMESH_NODES] = isValid;
		valid[DrawMode::NAVMESH_PORTALS] = isValid;
		valid[DrawMode::NAVMESH_INVIS] = isValid;
		valid[DrawMode::MESH] = true;
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
	}
}

bool ToolButton(const char* text, const char* tooltip, bool active)
{
	if (!active)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(100, 100, 100, 255));
	}

	ImGui::PushFont(ImGuiEx::LargeIconFont);

	bool result = ImGui::Button(text, ImVec2(30, 30));

	ImGui::PopFont();

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(tooltip);
		ImGui::EndTooltip();
	}

	if (!active)
	{
		ImGui::PopStyleColor();
	}

	return result;
}

void NavMeshTool::handleTools()
{
	ToolType type = !m_tool ? ToolType::NONE : m_tool->type();

	if (ToolButton(ICON_MD_LANDSCAPE, "Tile Edit Tool", type == ToolType::TILE_EDIT)) // create tiles
	{
		setTool(new NavMeshTileTool);
	}
	ImGui::SameLine();

	if (ToolButton(ICON_MD_DIRECTIONS_RUN, "NavMesh Tester Tool", type == ToolType::NAVMESH_TESTER)) // test mesh
	{
		setTool(new NavMeshTesterTool);
	}
	ImGui::SameLine();

	if (ToolButton(ICON_MD_FORMAT_SHAPES, "Mark Areas Tool", type == ToolType::CONVEX_VOLUME)) // mark areas
	{
		setTool(new ConvexVolumeTool);
	}
	ImGui::SameLine();

	if (ToolButton(ICON_MD_LINK, "Connections Tool", type == ToolType::OFFMESH_CONNECTION)) // connections tool
	{
		setTool(new OffMeshConnectionTool);
	}
	ImGui::SameLine();

	if (ToolButton(ICON_MD_CROP, "Prune NavMesh Tool", type == ToolType::NAVMESH_PRUNE)) // prune tool
	{
		setTool(new NavMeshPruneTool);
	}
	ImGui::SameLine();

	if (ToolButton(ICON_MD_PLACE, "Waypoints Tool", type == ToolType::WAYPOINTS))
	{
		setTool(new WaypointsTool);
	}
	ImGui::SameLine();

	if (ToolButton(ICON_MD_INFO, "Navmesh Info", type == ToolType::INFO))
	{
		setTool(new NavMeshInfoTool);
	}

	ImGui::Separator();

	if (m_tool)
	{
		m_tool->handleMenu();
	}

	if (type == ToolType::TILE_EDIT)
	{
		int tt = m_tilesWidth * m_tilesHeight;

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

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
		if (ImGuiEx::ColoredButton("Reset Settings", ImVec2(100, 0), 0.0))
		{
			m_config = NavMeshConfig{};
		}

		if (ImGui::TreeNodeEx("Tiling", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen)) {

			ImGui::SliderFloat("Tile Size", &m_config.tileSize, 16.0f, 1024.0f, "%.0f");

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
			glm::vec3 min, max;
			m_navMesh->GetNavMeshBounds(min, max);

			ImGui::SliderFloat("Min X", &min.x, m_geom->getMeshBoundsMin().x, m_geom->getMeshBoundsMax().x, "%.1f");
			ImGui::SliderFloat("Min Y", &min.y, m_geom->getMeshBoundsMin().y, m_geom->getMeshBoundsMax().y, "%.1f");
			ImGui::SliderFloat("Min Z", &min.z, m_geom->getMeshBoundsMin().z, m_geom->getMeshBoundsMax().z, "%.1f");
			ImGui::SliderFloat("Max X", &max.x, m_geom->getMeshBoundsMin().x, m_geom->getMeshBoundsMax().x, "%.1f");
			ImGui::SliderFloat("Max Y", &max.y, m_geom->getMeshBoundsMin().y, m_geom->getMeshBoundsMax().y, "%.1f");
			ImGui::SliderFloat("Max Z", &max.z, m_geom->getMeshBoundsMin().z, m_geom->getMeshBoundsMax().z, "%.1f");

			m_navMesh->SetNavMeshBounds(min, max);

			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 131);
			if (ImGuiEx::ColoredButton("Reset Bounding Box", ImVec2(130, 0), 0.0))
			{
				m_navMesh->SetNavMeshBounds(
					m_geom->getMeshBoundsMin(),
					m_geom->getMeshBoundsMax());
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
}

void NavMeshTool::handleRender()
{
	if (!m_geom || !m_geom->getMeshLoader())
		return;

	duDebugDraw& dd = getDebugDraw();

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
		//m_geom->drawOffMeshConnections(&dd);
	}

	dd.depthMask(false);

	// Draw bounds
	const glm::vec3& bmin = m_navMesh->GetNavMeshBoundsMin();
	const glm::vec3& bmax = m_navMesh->GetNavMeshBoundsMax();
	duDebugDrawBoxWire(&dd, bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2],
		duRGBA(255, 255, 255, 128), 1.0f);

	// Tiling grid.
	int gw = 0, gh = 0;
	rcCalcGridSize(&bmin[0], &bmax[0], m_config.cellSize, &gw, &gh);
	const int tw = (gw + (int)m_config.tileSize - 1) / (int)m_config.tileSize;
	const int th = (gh + (int)m_config.tileSize - 1) / (int)m_config.tileSize;
	const float s = m_config.tileSize * m_config.cellSize;
	duDebugDrawGridXZ(&dd, bmin[0], bmin[1], bmin[2], tw, th, s, duRGBA(0, 0, 0, 64), 1.0f);

	if (m_navMesh->IsNavMeshLoaded()
		&& (m_drawMode == DrawMode::NAVMESH
			|| m_drawMode == DrawMode::NAVMESH_TRANS
			|| m_drawMode == DrawMode::NAVMESH_BVTREE
			|| m_drawMode == DrawMode::NAVMESH_NODES
			|| m_drawMode == DrawMode::NAVMESH_PORTALS
			|| m_drawMode == DrawMode::NAVMESH_INVIS))
	{
		auto navMesh = m_navMesh->GetNavMesh();
		auto navQuery = m_navMesh->GetNavMeshQuery();

		if (navMesh && navQuery)
		{
			if (m_drawMode != DrawMode::NAVMESH_INVIS)
				duDebugDrawNavMeshWithClosedList(&dd, *navMesh, *navQuery, m_navMeshDrawFlags);
			if (m_drawMode == DrawMode::NAVMESH_BVTREE)
				duDebugDrawNavMeshBVTree(&dd, *navMesh);
			if (m_drawMode == DrawMode::NAVMESH_PORTALS)
				duDebugDrawNavMeshPortals(&dd, *navMesh);
			if (m_drawMode == DrawMode::NAVMESH_NODES)
				duDebugDrawNavMeshNodes(&dd, *navQuery);

			duDebugDrawNavMeshPolysWithFlags(&dd, *navMesh, +PolyFlags::Disabled, duRGBA(0, 0, 0, 128));
		}
	}

	drawConvexVolumes(&dd);

	if (m_tool)
		m_tool->handleRender();
	renderToolStates();
}

void NavMeshTool::drawConvexVolumes(duDebugDraw* dd)
{
	dd->depthMask(false);

	dd->begin(DU_DRAW_TRIS);

	const auto& volumes = m_navMesh->GetConvexVolumes();

	for (const auto& vol : volumes)
	{
		uint32_t col = duTransCol(m_navMesh->GetPolyArea((int)vol->areaType).color, 32);
		size_t nverts = vol->verts.size();

		for (size_t j = 0, k = nverts - 1; j < nverts; k = j++)
		{
			const glm::vec3& va = vol->verts[k];
			const glm::vec3& vb = vol->verts[j];

			dd->vertex(vol->verts[0][0], vol->hmax, vol->verts[0][2], col);
			dd->vertex(vb[0], vol->hmax, vb[2], col);
			dd->vertex(va[0], vol->hmax, va[2], col);

			dd->vertex(va[0], vol->hmin, va[2], duDarkenCol(col));
			dd->vertex(va[0], vol->hmax, va[2], col);
			dd->vertex(vb[0], vol->hmax, vb[2], col);

			dd->vertex(va[0], vol->hmin, va[2], duDarkenCol(col));
			dd->vertex(vb[0], vol->hmax, vb[2], col);
			dd->vertex(vb[0], vol->hmin, vb[2], duDarkenCol(col));
		}
	}
	dd->end();

	dd->begin(DU_DRAW_LINES, 2.0f);
	for (const auto& vol : volumes)
	{
		uint32_t col = duTransCol(m_navMesh->GetPolyArea((int)vol->areaType).color, 220);
		size_t nverts = vol->verts.size();

		for (size_t j = 0, k = nverts - 1; j < nverts; k = j++)
		{
			const glm::vec3& va = vol->verts[k];
			const glm::vec3& vb = vol->verts[j];

			dd->vertex(va[0], vol->hmin, va[2], duDarkenCol(col));
			dd->vertex(vb[0], vol->hmin, vb[2], duDarkenCol(col));
			dd->vertex(va[0], vol->hmax, va[2], col);
			dd->vertex(vb[0], vol->hmax, vb[2], col);
			dd->vertex(va[0], vol->hmin, va[2], duDarkenCol(col));
			dd->vertex(va[0], vol->hmax, va[2], col);
		}
	}
	dd->end();

	dd->begin(DU_DRAW_POINTS, 3.0f);
	for (const auto& vol : volumes)
	{
		uint32_t col = duDarkenCol(duTransCol(m_navMesh->GetPolyArea((int)vol->areaType).color, 255));
		size_t nverts = vol->verts.size();

		for (size_t j = 0; j < nverts; ++j)
		{
			dd->vertex(vol->verts[j].x, vol->verts[j].y + 0.1f, vol->verts[j].z, col);
			dd->vertex(vol->verts[j].x, vol->hmin, vol->verts[j].z, col);
			dd->vertex(vol->verts[j].x, vol->hmax, vol->verts[j].z, col);
		}
	}
	dd->end();

	dd->depthMask(true);
}

void NavMeshTool::handleRenderOverlay(const glm::mat4& proj,
	const glm::mat4& model, const glm::ivec4& view)
{
	if (m_tool)
	{
		m_tool->handleRenderOverlay(proj, model, view);
	}

	renderOverlayToolStates(proj, model, view);
}

void NavMeshTool::handleGeometryChanged(class InputGeom* geom)
{
	m_geom = geom;

	if (m_tool)
	{
		m_tool->reset();
	}
	resetToolStates();

	NavMeshUpdated();
}

bool NavMeshTool::handleBuild()
{
	if (!m_geom || !m_geom->getMeshLoader())
	{
		SPDLOG_LOGGER_ERROR(m_logger, "buildTiledNavigation: No vertices and triangles.");
		return false;
	}

	std::shared_ptr<dtNavMesh> navMesh(dtAllocNavMesh(),
		[](dtNavMesh* ptr) { dtFreeNavMesh(ptr); });

	m_navMesh->SetNavMesh(navMesh, false);

	dtNavMeshParams params;
	glm::vec3 boundsMin = m_navMesh->GetNavMeshBoundsMin();
	rcVcopy(params.orig, glm::value_ptr(boundsMin));
	params.tileWidth = m_config.tileSize * m_config.cellSize;
	params.tileHeight = m_config.tileSize * m_config.cellSize;
	params.maxTiles = m_tilesWidth * m_tilesHeight;
	params.maxPolys = m_maxPolysPerTile * params.maxTiles;

	dtStatus status;

	status = navMesh->init(&params);
	if (dtStatusFailed(status))
	{
		SPDLOG_LOGGER_ERROR(m_logger, "buildTiledNavigation: Could not init navmesh.");
		return false;
	}

	BuildAllTiles(navMesh);

	if (m_tool)
	{
		m_tool->init(this);
	}

	initToolStates();

	return true;
}

void NavMeshTool::GetTilePos(const glm::vec3& pos, int& tx, int& ty)
{
	if (!m_geom) return;

	const glm::vec3& bmin = m_navMesh->GetNavMeshBoundsMin();

	const float ts = m_config.tileSize * m_config.cellSize;
	tx = (int)((pos[0] - bmin[0]) / ts);
	ty = (int)((pos[2] - bmin[2]) / ts);
}

void NavMeshTool::RemoveTile(const glm::vec3& pos)
{
	if (!m_geom) return;

	auto navMesh = m_navMesh->GetNavMesh();
	if (!navMesh) return;

	const glm::vec3& bmin = m_navMesh->GetNavMeshBoundsMin();
	const glm::vec3& bmax = m_navMesh->GetNavMeshBoundsMax();

	const float ts = m_config.tileSize * m_config.cellSize;
	const int tx = (int)((pos[0] - bmin[0]) / ts);
	const int ty = (int)((pos[2] - bmin[2]) / ts);

	dtTileRef tileRef = navMesh->getTileRefAt(tx, ty, 0);
	navMesh->removeTile(tileRef, 0, 0);
}

void NavMeshTool::RemoveAllTiles()
{
	std::shared_ptr<dtNavMesh> navMesh = m_navMesh->GetNavMesh();
	if (!navMesh) return;

	for (int i = 0; i < navMesh->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = nullptr;

		if ((tile = const_cast<const dtNavMesh*>(navMesh.get())->getTile(i))
			&& tile->header != nullptr)
		{
			navMesh->removeTile(navMesh->getTileRef(tile), 0, 0);
		}
	}
}

void NavMeshTool::CancelBuildAllTiles(bool wait)
{
	if (m_buildingTiles)
		m_cancelTiles = true;

	if (wait && m_buildThread.joinable())
		m_buildThread.join();
}

void NavMeshTool::setOutputPath(const char* output_path)
{
	strcpy(m_outputPath, output_path);
	CreateDirectory(output_path, nullptr);
}

void NavMeshTool::resetCommonSettings()
{
	m_config = NavMeshConfig{};
	m_navMeshDrawFlags = DU_DRAWNAVMESH_OFFMESHCONS | DU_DRAWNAVMESH_CLOSEDLIST;
}

//----------------------------------------------------------------------------

void NavMeshTool::BuildTile(const glm::vec3& pos)
{
	if (!m_geom) return;

	const glm::vec3& bmin = m_navMesh->GetNavMeshBoundsMin();
	const glm::vec3& bmax = m_navMesh->GetNavMeshBoundsMax();

	const float ts = m_config.tileSize * m_config.cellSize;
	const int tx = (int)((pos[0] - bmin[0]) / ts);
	const int ty = (int)((pos[2] - bmin[2]) / ts);

	glm::vec3 tileBmin, tileBmax;
	tileBmin[0] = bmin[0] + tx * ts;
	tileBmin[1] = bmin[1];
	tileBmin[2] = bmin[2] + ty * ts;

	tileBmax[0] = bmin[0] + (tx + 1)*ts;
	tileBmax[1] = bmax[1];
	tileBmax[2] = bmin[2] + (ty + 1)*ts;

	m_ctx->resetLog();
	auto offMeshConnections = m_navMesh->CreateOffMeshConnectionBuffer();

	std::shared_ptr<dtNavMesh> navMesh = m_navMesh->GetNavMesh();
	if (!navMesh)
	{
		navMesh = std::shared_ptr<dtNavMesh>(dtAllocNavMesh(),
			[](dtNavMesh* ptr) { dtFreeNavMesh(ptr); });

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
			SPDLOG_LOGGER_ERROR(m_logger, "buildTiledNavigation: Could not init navmesh.");
			return;
		}
	}

	int dataSize = 0;
	unsigned char* data = buildTileMesh(tx, ty, glm::value_ptr(tileBmin),
		glm::value_ptr(tileBmax), offMeshConnections, dataSize);

	// Remove any previous data (navmesh owns and deletes the data).
	dtTileRef tileRef = navMesh->getTileRefAt(tx, ty, 0);
	navMesh->removeTile(tileRef, 0, 0);

	// Add tile, or leave the location empty.
	if (data)
	{
		// Let the navmesh own the data.
		dtStatus status = navMesh->addTile(data, dataSize, DT_TILE_FREE_DATA, 0, 0);
		if (dtStatusFailed(status))
			dtFree(data);
	}

	SPDLOG_LOGGER_DEBUG(m_logger, "Build Tile ({}, {}):", tx, ty);
}

void NavMeshTool::RebuildTile(
	const std::shared_ptr<OffMeshConnectionBuffer> connBuffer,
	dtTileRef tileRef)
{
	if (!m_geom) return;
	const auto& navMesh = m_navMesh->GetNavMesh();
	if (!navMesh) return;

	const dtMeshTile* tile = navMesh->getTileByRef(tileRef);
	if (!tile || !tile->header) return;

	auto bmin = tile->header->bmin;
	auto bmax = tile->header->bmax;

	int dataSize = 0;
	unsigned char* data = buildTileMesh(tile->header->x, tile->header->y, bmin, bmax, connBuffer, dataSize);

	navMesh->removeTile(tileRef, 0, 0);

	if (data)
	{
		dtStatus status = navMesh->addTile(data, dataSize, DT_TILE_FREE_DATA, 0, 0);
		if (dtStatusFailed(status))
			dtFree(data);
	}

}

void NavMeshTool::RebuildTiles(const std::vector<dtTileRef>& tiles)
{
	auto connBuffer = m_navMesh->CreateOffMeshConnectionBuffer();

	for (dtTileRef tileRef : tiles)
		RebuildTile(connBuffer, tileRef);
}

void NavMeshTool::SaveNavMesh()
{
	// update nav mesh config with current settings
	m_navMesh->GetNavMeshConfig() = m_config;

	// save everything out
	m_navMesh->SaveNavMeshFile();
}

struct TileData
{
	unsigned char* data = 0;
	int length = 0;
	int x = 0;
	int y = 0;
};

using TileDataPtr = std::shared_ptr<TileData>;

class NavmeshUpdaterAgent : public concurrency::agent
{
public:
	explicit NavmeshUpdaterAgent(concurrency::ISource<TileDataPtr>& dataSource,
		const std::shared_ptr<dtNavMesh>& navMesh)
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
	std::shared_ptr<dtNavMesh> m_navMesh;
};

void NavMeshTool::BuildAllTiles(const std::shared_ptr<dtNavMesh>& navMesh, bool async)
{
	if (!m_geom) return;
	if (m_buildingTiles) return;
	if (!navMesh) return;

	// if async, invoke on a new thread
	if (async)
	{
		if (m_buildThread.joinable())
			m_buildThread.join();

		m_buildThread = std::thread([this, navMesh]()
			{
				BuildAllTiles(navMesh, false);
			});
		return;
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

	auto offMeshConnections = m_navMesh->CreateOffMeshConnectionBuffer();

	//concurrency::CurrentScheduler::Create(concurrency::SchedulerPolicy(1, concurrency::MaxConcurrency, 3));

	concurrency::unbounded_buffer<TileDataPtr> agentTiles;
	NavmeshUpdaterAgent updater_agent(agentTiles, navMesh);

	updater_agent.start();

	// Start the build process.
	m_ctx->startTimer(RC_TIMER_TEMP);

	concurrency::task_group tasks;
	for (int x = 0; x < tw; x++)
	{
		for (int y = 0; y < th; y++)
		{
			tasks.run([this, x, y, &bmin, &bmax, tcs, &agentTiles, offMeshConnections]()
				{
					if (m_cancelTiles)
						return;

					++m_tilesBuilt;

					glm::vec3 tileBmin, tileBmax;
					tileBmin[0] = bmin[0] + x * tcs;
					tileBmin[1] = bmin[1];
					tileBmin[2] = bmin[2] + y * tcs;

					tileBmax[0] = bmin[0] + (x + 1)*tcs;
					tileBmax[1] = bmax[1];
					tileBmax[2] = bmin[2] + (y + 1)*tcs;

					int dataSize = 0;
					uint8_t* data = buildTileMesh(x, y, glm::value_ptr(tileBmin),
						glm::value_ptr(tileBmax), offMeshConnections, dataSize);

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
		SPDLOG_LOGGER_ERROR(m_logger, "buildNavigation: Could not create solid heightfield.");
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

	for (int i = 0; i < ncid; ++i)
	{
		const rcChunkyTriMeshNode& node = chunkyMesh->nodes[cid[i]];
		const int* ctris = &chunkyMesh->tris[node.i * 3];
		const int nctris = node.n;

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
		SPDLOG_LOGGER_ERROR(m_logger, "buildNavigation: Could not build compact data.");
		return 0;
	}

	return std::move(chf);
}

unsigned char* NavMeshTool::buildTileMesh(
	const int tx,
	const int ty,
	const float* bmin,
	const float* bmax,
	const std::shared_ptr<OffMeshConnectionBuffer>& offMeshConnections,
	int& dataSize) const
{
	if (!m_geom || !m_geom->getMeshLoader() || !m_geom->getChunkyMesh())
	{
		SPDLOG_LOGGER_ERROR(m_logger, "buildNavigation: Input mesh is not specified.");
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

	deleting_unique_ptr<rcCompactHeightfield> chf = rasterizeGeometry(cfg);
	if (!chf)
		return 0;

	// Erode the walkable area by agent radius.
	if (!rcErodeWalkableArea(m_ctx, cfg.walkableRadius, *chf))
	{
		SPDLOG_LOGGER_ERROR(m_logger, "buildNavigation: Could not erode.");
		return 0;
	}

	// Mark areas.
	const auto& volumes = m_navMesh->GetConvexVolumes();
	for (const auto& vol : volumes)
	{
		rcMarkConvexPolyArea(m_ctx, glm::value_ptr(vol->verts[0]), static_cast<int>(vol->verts.size()),
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
		if (!rcBuildDistanceField(m_ctx, *chf))
		{
			SPDLOG_LOGGER_ERROR(m_logger, "buildNavigation: Could not build distance field.");
			return false;
		}

		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildRegions(m_ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
		{
			SPDLOG_LOGGER_ERROR(m_logger, "buildNavigation: Could not build watershed regions.");
			return false;
		}
	}
	else if (m_config.partitionType == PartitionType::MONOTONE)
	{
		// Partition the walkable surface into simple regions without holes.
		// Monotone partitioning does not need distancefield.
		if (!rcBuildRegionsMonotone(m_ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
		{
			SPDLOG_LOGGER_ERROR(m_logger, "buildNavigation: Could not build monotone regions.");
			return false;
		}
	}
	else // PartitionType::LAYERS
	{
		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildLayerRegions(m_ctx, *chf, cfg.borderSize, cfg.minRegionArea))
		{
			SPDLOG_LOGGER_ERROR(m_logger, "buildNavigation: Could not build layer regions.");
			return false;
		}
	}

	// Create contours.
	deleting_unique_ptr<rcContourSet> cset(rcAllocContourSet(), [](rcContourSet* cs) { rcFreeContourSet(cs); });
	if (!rcBuildContours(m_ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset))
	{
		SPDLOG_LOGGER_ERROR(m_logger, "buildNavigation: Could not create contours.");
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
		SPDLOG_LOGGER_ERROR(m_logger, "buildNavigation: Could not triangulate contours.");
		return 0;
	}

	// Build detail mesh.
	deleting_unique_ptr<rcPolyMeshDetail> dmesh(rcAllocPolyMeshDetail(), [](rcPolyMeshDetail* pm) { rcFreePolyMeshDetail(pm); });
	if (!rcBuildPolyMeshDetail(m_ctx, *pmesh, *chf,
		cfg.detailSampleDist, cfg.detailSampleMaxError,
		*dmesh))
	{
		SPDLOG_LOGGER_ERROR(m_logger, "buildNavigation: Could build polymesh detail.");
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
			SPDLOG_LOGGER_ERROR(m_logger, "Too many vertices per tile {} (max: {:#x}).",
				pmesh->nverts, (uint16_t)0xffff);
			return 0;
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

		offMeshConnections->UpdateNavMeshCreateParams(params);
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
			SPDLOG_LOGGER_ERROR(m_logger, "Could not build Detour navmesh.");
			return 0;
		}
	}

	m_ctx->stopTimer(RC_TIMER_TOTAL);

	dataSize = navDataSize;
	return navData;
}

unsigned int NavMeshTool::GetColorForPoly(const dtPoly* poly)
{
	if (poly)
	{
		return m_navMesh->GetPolyArea(poly->getArea()).color;
	}

	return 0;
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

void NavMeshTool::handleClick(const glm::vec3& s, const glm::vec3& p, bool shift)
{
	if (m_tool)
	{
		m_tool->handleClick(s, p, shift);
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

unsigned int NavMeshDebugDraw::polyToCol(const dtPoly* poly)
{
	return m_tool->GetColorForPoly(poly);
}
