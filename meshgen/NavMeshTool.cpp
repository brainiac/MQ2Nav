
#include "pch.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/Application.h"
#include "meshgen/ChunkyTriMesh.h"
#include "meshgen/ConvexVolumeTool.h"
#include "meshgen/MapGeometryLoader.h"
#include "meshgen/NavMeshInfoTool.h"
#include "meshgen/NavMeshPruneTool.h"
#include "meshgen/NavMeshTesterTool.h"
#include "meshgen/NavMeshTileTool.h"
#include "meshgen/OffMeshConnectionTool.h"
#include "meshgen/RecastContext.h"
#include "meshgen/WaypointsTool.h"
#include "meshgen/ZoneContext.h"
#include "common/NavMeshData.h"
#include "common/Utilities.h"
#include "common/proto/NavMeshFile.pb.h"
#include "imgui/ImGuiUtils.h"
#include "imgui/fonts/IconsMaterialDesign.h"

#include <Recast.h>
#include <RecastDump.h>
#include <DetourCommon.h>
#include <DetourDebugDraw.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ppl.h>
#include <agents.h>
#include <complex>

//----------------------------------------------------------------------------

NavMeshTool::NavMeshTool()
{
	resetCommonSettings();

	m_outputPath = new char[MAX_PATH];
	setTool(new NavMeshTileTool);

	m_renderManager = std::make_unique<ZoneRenderManager>();
	m_renderManager->init();
	m_renderManager->SetNavMeshConfig(&m_config);
	m_renderManager->GetNavMeshRender()->SetFlags(
		ZoneNavMeshRender::DRAW_TILES | ZoneNavMeshRender::DRAW_TILE_BOUNDARIES | ZoneNavMeshRender::DRAW_CLOSED_LIST
	);

	UpdateTileSizes();
}

NavMeshTool::~NavMeshTool()
{
	delete[] m_outputPath;
}

//----------------------------------------------------------------------------

void NavMeshTool::SetZoneContext(const std::shared_ptr<ZoneContext>& zoneContext)
{
	m_zoneContext = zoneContext;

	Reset();

	m_renderManager->SetZoneContext(zoneContext);

	if (m_zoneContext)
	{
		m_navMesh = m_zoneContext->GetNavMesh();
		m_navMeshConn = m_navMesh->OnNavMeshChanged.Connect([this]()
			{ NavMeshUpdated(); });

		NavMeshUpdated();
	}
}

void NavMeshTool::Reset()
{
	m_navMesh = nullptr;
	m_navMeshConn.Disconnect();

	if (m_tool)
	{
		m_tool->reset();
	}

	resetToolStates();
}

void NavMeshTool::NavMeshUpdated()
{
	if (!m_navMesh->IsNavMeshLoadedFromDisk())
	{
		if (m_zoneContext)
		{
			// reset to default
			m_navMesh->SetNavMeshBounds(
				m_zoneContext->GetMeshBoundsMin(),
				m_zoneContext->GetMeshBoundsMax());
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

	m_renderManager->GetNavMeshRender()->SetNavMesh(m_navMesh);
	m_renderManager->GetNavMeshRender()->Build();

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
	if (m_zoneContext)
	{
		ImGui::Checkbox("Draw Input Geometry", &m_drawInputGeometry);

		uint32_t flags = m_renderManager->GetNavMeshRender()->GetFlags();
		if (ImGui::CheckboxFlags("Draw Tiles", &flags, ZoneNavMeshRender::DRAW_TILES))
			m_renderManager->GetNavMeshRender()->SetFlags(flags);
		if (ImGui::CheckboxFlags("Draw Tile Boundaries", &flags, ZoneNavMeshRender::DRAW_TILE_BOUNDARIES))
			m_renderManager->GetNavMeshRender()->SetFlags(flags);
		if (ImGui::CheckboxFlags("Draw Polygon Boundaries", &flags, ZoneNavMeshRender::DRAW_POLY_BOUNDARIES))
			m_renderManager->GetNavMeshRender()->SetFlags(flags);
		if (ImGui::CheckboxFlags("Draw Polygon Vertices", &flags, ZoneNavMeshRender::DRAW_POLY_VERTICES))
			m_renderManager->GetNavMeshRender()->SetFlags(flags);
		if (ImGui::CheckboxFlags("Draw Closed List", &flags, ZoneNavMeshRender::DRAW_CLOSED_LIST))
			m_renderManager->GetNavMeshRender()->SetFlags(flags);

		ImGui::Checkbox("Draw Grid", &m_drawGrid);

		float pointSize = m_renderManager->GetNavMeshRender()->GetPointSize();
		if (ImGui::DragFloat("PointSize", &pointSize, 0.01f, 0, 10, "%.2f"))
		{
			m_renderManager->SetPointSize(pointSize);
			m_renderManager->GetNavMeshRender()->SetPointSize(pointSize);
		}
	
		ImGui::Checkbox("Draw BV Tree", &m_drawNavMeshBVTree);
		ImGui::Checkbox("Draw Nodes", &m_drawNavMeshNodes);
		ImGui::Checkbox("Draw Portals", &m_drawNavMeshPortals);
	}
}

bool ToolButton(const char* text, const char* tooltip, bool active)
{
	if (!active)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(100, 100, 100, 255));
	}

	ImGui::PushFont(mq::imgui::LargeIconFont);

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

bool ToolButton(const char8_t* icon, const char* tooltip, bool active)
{
	return ToolButton(reinterpret_cast<const char*>(icon), tooltip, active);
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

		if (ImGui::TreeNodeEx("Tiling", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen))
		{

			ImGui::SliderFloat("Tile Size", &m_config.tileSize, 16.0f, 1024.0f, "%.0f");

			UpdateTileSizes();

			ImGui::SliderFloat("Cell Size", &m_config.cellSize, 0.1f, 1.0f);
			ImGui::SliderFloat("Cell Height", &m_config.cellHeight, 0.1f, 1.0f);
		}
		if (ImGui::TreeNodeEx("Agent", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen))
		{
			ImGui::SliderFloat("Height", &m_config.agentHeight, 0.1f, 15.0f, "%.1f");
			ImGui::SliderFloat("Radius", &m_config.agentRadius, 0.1f, 15.0f, "%.1f");
			ImGui::SliderFloat("Max Climb", &m_config.agentMaxClimb, 0.1f, 15.0f, "%.1f");
			ImGui::SliderFloat("Max Slope", &m_config.agentMaxSlope, 0.0f, 90.0f, "%.1f");
		}

		if (m_zoneContext &&
			ImGui::TreeNodeEx("Bounding Box", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen))
		{
			glm::vec3 min, max;
			m_navMesh->GetNavMeshBounds(min, max);

			glm::vec3 bmin = m_zoneContext->GetMeshBoundsMin();
			glm::vec3 bmax = m_zoneContext->GetMeshBoundsMax();

			ImGui::SliderFloat("Min X", &min.x, bmin.x, bmax.x, "%.1f");
			ImGui::SliderFloat("Min Y", &min.y, bmin.y, bmax.y, "%.1f");
			ImGui::SliderFloat("Min Z", &min.z, bmin.z, bmax.z, "%.1f");
			ImGui::SliderFloat("Max X", &max.x, bmin.x, bmax.x, "%.1f");
			ImGui::SliderFloat("Max Y", &max.y, bmin.y, bmax.y, "%.1f");
			ImGui::SliderFloat("Max Z", &max.z, bmin.z, bmax.z, "%.1f");

			m_navMesh->SetNavMeshBounds(min, max);

			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 131);
			if (ImGuiEx::ColoredButton("Reset Bounding Box", ImVec2(130, 0), 0.0))
			{
				m_navMesh->SetNavMeshBounds(bmin, bmax);
			}
		}

		if (mq::imgui::CollapsingSubHeader("Advanced Options"))
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
			mq::imgui::HelpMarker(RegionHelp, 600.0f, mq::imgui::ConsoleFont);

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
			mq::imgui::HelpMarker(PartitioningHelp, 600.0f, mq::imgui::ConsoleFont);

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
			mq::imgui::HelpMarker(PolygonizationHelp, 600.0f, mq::imgui::ConsoleFont);

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
			mq::imgui::HelpMarker(DetailMeshHelp, 600.0f, mq::imgui::ConsoleFont);

			ImGui::SliderFloat("Sample Distance", &m_config.detailSampleDist, 0.0f, 0.9f, "%.2f");
			ImGui::SliderFloat("Max Sample Error", &m_config.detailSampleMaxError, 0.0f, 100.0f, "%.1f");
		}
	}
}

void NavMeshTool::handleRender(const glm::mat4& viewModelProjMtx, const glm::ivec4& viewport)
{
	m_viewModelProjMtx = viewModelProjMtx;
	m_viewport = viewport;

	if (!m_zoneContext || !m_zoneContext->IsZoneLoaded())
		return;

	m_navMesh->SendEventIfDirty();

	ZoneRenderDebugDraw dd(m_renderManager.get());

	if (m_drawInputGeometry)
	{
		m_renderManager->GetInputGeoRender()->Render();
	}

	dd.depthMask(false);

	if (m_drawGrid)
	{
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
	}
	if (m_navMesh->IsNavMeshLoaded())
	{
		auto navMesh = m_navMesh->GetNavMesh();
		auto navQuery = m_navMesh->GetNavMeshQuery();

		if (navMesh && navQuery)
		{
			m_renderManager->GetNavMeshRender()->Render();

			if (m_drawNavMeshBVTree)
				duDebugDrawNavMeshBVTree(&dd, *navMesh);
			if (m_drawNavMeshPortals)
				duDebugDrawNavMeshPortals(&dd, *navMesh);
			if (m_drawNavMeshNodes)
				duDebugDrawNavMeshNodes(&dd, *navQuery);
		}
	}

	drawConvexVolumes(&dd);

	if (m_tool)
		m_tool->handleRender();
	renderToolStates();

	m_renderManager->Render();
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

void NavMeshTool::handleRenderOverlay()
{
	if (m_tool)
	{
		m_tool->handleRenderOverlay();
	}

	for (const auto& p : m_toolStates)
	{
		if (p.second)
		{
			p.second->handleRenderOverlay();
		}
	}
}

bool NavMeshTool::BuildMesh()
{
	if (!m_zoneContext || !m_zoneContext->GetMeshLoader())
	{
		SPDLOG_ERROR("buildTiledNavigation: No vertices and triangles.");
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
		SPDLOG_ERROR("buildTiledNavigation: Could not init navmesh.");
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
	if (!m_zoneContext) return;

	const glm::vec3& bmin = m_navMesh->GetNavMeshBoundsMin();

	const float ts = m_config.tileSize * m_config.cellSize;
	tx = (int)((pos[0] - bmin[0]) / ts);
	ty = (int)((pos[2] - bmin[2]) / ts);
}

void NavMeshTool::RemoveTile(const glm::vec3& pos)
{
	if (!m_zoneContext) return;

	auto navMesh = m_navMesh->GetNavMesh();
	if (!navMesh) return;

	glm::ivec2 tilePos = m_navMesh->GetTilePos(pos);

	m_navMesh->RemoveTileAt(tilePos.x, tilePos.y);
}

void NavMeshTool::RemoveAllTiles()
{
	m_navMesh->RemoveAllTiles();
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
}

//----------------------------------------------------------------------------

void NavMeshTool::BuildTile(const glm::vec3& pos)
{
	if (!m_zoneContext) return;

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
			SPDLOG_ERROR("buildTiledNavigation: Could not init navmesh.");
			return;
		}
	}

	int dataSize = 0;
	unsigned char* data = buildTileMesh(tx, ty, glm::value_ptr(tileBmin),
		glm::value_ptr(tileBmax), offMeshConnections, dataSize);

	m_navMesh->AddTile(data, dataSize, tx, ty);

	SPDLOG_DEBUG("Build Tile ({}, {}):", tx, ty);
}

void NavMeshTool::RebuildTile(
	const std::shared_ptr<OffMeshConnectionBuffer> connBuffer,
	dtTileRef tileRef)
{
	if (!m_zoneContext) return;
	const auto& navMesh = m_navMesh->GetNavMesh();
	if (!navMesh) return;

	const dtMeshTile* tile = navMesh->getTileByRef(tileRef);
	if (!tile || !tile->header) return;

	auto bmin = tile->header->bmin;
	auto bmax = tile->header->bmax;
	int tx = tile->header->x;
	int ty = tile->header->y;

	int dataSize = 0;
	unsigned char* data = buildTileMesh(tx, ty, bmin, bmax, connBuffer, dataSize);

	m_navMesh->AddTile(data, dataSize, tx, ty, tile->header->layer);

	SPDLOG_DEBUG("Rebuild Tile ({}, {}):", tx, ty);
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

void NavMeshTool::BuildAllTiles(const std::shared_ptr<dtNavMesh>& navMesh, bool async)
{
	if (!m_zoneContext) return;
	if (m_buildingTiles) return;
	if (!navMesh) return;

	RecastContext::ResetAllTimers();

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

	// Start the build process.
	m_ctx->startTimer(RC_TIMER_TEMP);

	concurrency::task_group tasks;
	for (int x = 0; x < tw; x++)
	{
		for (int y = 0; y < th; y++)
		{
			tasks.run([this, x, y, &bmin, &bmax, tcs, offMeshConnections]()
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

						asend(m_builtTileData, tileData);
					}
				});
		}
	}

	tasks.wait();

	// Start the build process.
	m_ctx->stopTimer(RC_TIMER_TEMP);

	int totalTime = m_ctx->getAccumulatedTime(RC_TIMER_TEMP);
	m_totalBuildTimeMs = static_cast<float>(totalTime) / 1000.0f;

	duLogBuildTimes(*m_ctx, totalTime);

	m_buildingTiles = false;
}

deleting_unique_ptr<rcCompactHeightfield> NavMeshTool::rasterizeGeometry(rcConfig& cfg) const
{
	// Allocate voxel heightfield where we rasterize our input data to.
	deleting_unique_ptr<rcHeightfield> solid(rcAllocHeightfield(),
		[](rcHeightfield* hf) { rcFreeHeightField(hf); });

	if (!rcCreateHeightfield(m_ctx, *solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
	{
		SPDLOG_ERROR("buildNavigation: Could not create solid heightfield.");
		return 0;
	}

	MapGeometryLoader* loader = m_zoneContext->GetMeshLoader();

	const float* verts = loader->getVerts();
	const int nverts = loader->getVertCount();
	const rcChunkyTriMesh* chunkyMesh = m_zoneContext->GetChunkyMesh();

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
		SPDLOG_ERROR("buildNavigation: Could not build compact data.");
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
	if (!m_zoneContext || !m_zoneContext->IsZoneLoaded() || !m_zoneContext->GetChunkyMesh())
	{
		SPDLOG_ERROR("buildTileMesh: Input mesh is not specified.");
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
	{
		return nullptr;
	}

	// Erode the walkable area by agent radius.
	if (!rcErodeWalkableArea(m_ctx, cfg.walkableRadius, *chf))
	{
		SPDLOG_ERROR("buildTileMesh: Could not erode.");
		return nullptr;
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
			SPDLOG_ERROR("buildTileMesh: Could not build distance field.");
			return nullptr;
		}

		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildRegions(m_ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
		{
			SPDLOG_ERROR("buildTileMesh: Could not build watershed regions.");
			return nullptr;
		}
	}
	else if (m_config.partitionType == PartitionType::MONOTONE)
	{
		// Partition the walkable surface into simple regions without holes.
		// Monotone partitioning does not need distancefield.
		if (!rcBuildRegionsMonotone(m_ctx, *chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
		{
			SPDLOG_ERROR("buildTileMesh: Could not build monotone regions.");
			return nullptr;
		}
	}
	else // PartitionType::LAYERS
	{
		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildLayerRegions(m_ctx, *chf, cfg.borderSize, cfg.minRegionArea))
		{
			SPDLOG_ERROR("buildTileMesh: Could not build layer regions.");
			return nullptr;
		}
	}

	// Create contours.
	deleting_unique_ptr<rcContourSet> cset(rcAllocContourSet(), [](rcContourSet* cs) { rcFreeContourSet(cs); });
	if (!rcBuildContours(m_ctx, *chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *cset))
	{
		SPDLOG_ERROR("buildTileMesh: Could not create contours.");
		return nullptr;
	}

	if (cset->nconts == 0)
	{
		return nullptr;
	}

	// Build polygon navmesh from the contours.
	deleting_unique_ptr<rcPolyMesh> pmesh(rcAllocPolyMesh(), [](rcPolyMesh* pm) { rcFreePolyMesh(pm); });
	if (!rcBuildPolyMesh(m_ctx, *cset, cfg.maxVertsPerPoly, *pmesh))
	{
		SPDLOG_ERROR("buildTileMesh: Could not triangulate contours.");
		return nullptr;
	}

	// Build detail mesh.
	deleting_unique_ptr<rcPolyMeshDetail> dmesh(rcAllocPolyMeshDetail(), [](rcPolyMeshDetail* pm) { rcFreePolyMeshDetail(pm); });
	if (!rcBuildPolyMeshDetail(m_ctx, *pmesh, *chf,
		cfg.detailSampleDist, cfg.detailSampleMaxError,
		*dmesh))
	{
		SPDLOG_ERROR("buildTileMesh: Could build polymesh detail.");
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
			SPDLOG_ERROR("buildTileMesh: Too many vertices per tile {} (max: {:#x}).",
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
			SPDLOG_ERROR("buildTileMesh: Could not build Detour navmesh.");
			return nullptr;
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

void NavMeshTool::handleClick(const glm::vec3& p, bool shift)
{
	if (m_tool)
	{
		m_tool->handleClick(p, shift);
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

	TileDataPtr data;
	while (concurrency::try_receive(m_builtTileData, data))
	{
		m_navMesh->AddTile(data->data, data->length, data->x, data->y);
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

glm::ivec2 NavMeshTool::Project(const glm::vec3& point) const
{
	// Transform the point into clip space
	glm::vec4 clipSpacePoint = m_viewModelProjMtx * glm::vec4(point, 1.0f);
	
	// Perform perspective division to get NDC coordinates
	glm::vec3 ndc = glm::vec3(clipSpacePoint) / clipSpacePoint.w;
	
	// Check if the point is outside the NDC cube of [-1, 1] in any axis;
	if (ndc.x < -1.0 || ndc.x > 1.0 ||
		ndc.y < -1.0 || ndc.y > 1.0 ||
		ndc.z < -1.0 || ndc.z > 1.0)
	{
		return glm::vec2(-1, -1);
	}

	glm::ivec2 screenPos;
	screenPos.x = static_cast<int>((ndc.x * 0.5f + 0.5f) * static_cast<float>(m_viewport.z) + static_cast<float>(m_viewport.x));
	screenPos.y = static_cast<int>((ndc.y * 0.5f + 0.5f) * static_cast<float>(m_viewport.w) + static_cast<float>(m_viewport.y));
	return screenPos;
}
