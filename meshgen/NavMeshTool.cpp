
#include "pch.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/Application.h"
#include "meshgen/BackgroundTaskManager.h"
#include "meshgen/ConvexVolumeTool.h"
#include "meshgen/NavMeshInfoTool.h"
#include "meshgen/NavMeshPruneTool.h"
#include "meshgen/NavMeshTesterTool.h"
#include "meshgen/NavMeshTileTool.h"
#include "meshgen/OffMeshConnectionTool.h"
#include "meshgen/WaypointsTool.h"
#include "meshgen/ZoneProject.h"
#include "common/NavMeshData.h"
#include "common/Utilities.h"
#include "common/proto/NavMeshFile.pb.h"
#include "imgui/ImGuiUtils.h"
#include "imgui/fonts/IconsMaterialDesign.h"
#include "meshgen/ZoneRenderManager.h"

#include <Recast.h>
#include <DetourCommon.h>
#include <DetourDebugDraw.h>
#include <DetourNavMesh.h>

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <complex>

//----------------------------------------------------------------------------

NavMeshTool::NavMeshTool()
{
	setTool(new NavMeshTileTool);

	UpdateTileSizes();
}

NavMeshTool::~NavMeshTool()
{
}

//----------------------------------------------------------------------------

void NavMeshTool::OnProjectChanged(const std::shared_ptr<ZoneProject>& project)
{
	Reset();

	m_zoneProj = project;
}

void NavMeshTool::OnNavMeshProjectChanged(const std::shared_ptr<NavMeshProject>& navMeshProj)
{
	if (navMeshProj)
	{
		m_navMeshProj = navMeshProj;
		m_navMesh = m_navMeshProj->GetNavMesh();

		// TODO: Refactor how changes are sent through the system (threading...)
		m_navMeshConn = m_navMesh->OnNavMeshChanged.Connect([this]()
			{
				Application::Get().GetBackgroundTaskManager().PostToMainThread([this]()
					{
						NavMeshUpdated();
					});
			});


		NavMeshUpdated();
	}
	else if (m_navMeshProj)
	{
		m_navMeshProj = nullptr;
		m_navMeshConn.Disconnect();
		m_navMesh.reset();

		if (m_tool)
		{
			m_tool->reset();
		}

		resetToolStates();
	}
}

void NavMeshTool::Reset()
{
	m_navMesh.reset();
	m_navMeshProj.reset();
	m_navMeshConn.Disconnect();
	m_zoneProj.reset();

	if (m_tool)
	{
		m_tool->reset();
	}

	resetToolStates();
}

void NavMeshTool::NavMeshUpdated()
{
	if (m_tool)
	{
		m_tool->init(this);
	}

	initToolStates();
}

void NavMeshTool::UpdateTileSizes()
{
	if (m_navMesh)
	{
		auto& config = m_navMeshProj->GetNavMeshConfig();
		glm::vec3 bmin, bmax;
		m_navMesh->GetNavMeshBounds(bmin, bmax);
		int gw = 0, gh = 0;
		rcCalcGridSize(&bmin[0], &bmax[0], config.cellSize, &gw, &gh);
		const int ts = (int)config.tileSize;
		m_tilesWidth = (gw + ts - 1) / ts;
		m_tilesHeight = (gh + ts - 1) / ts;
		m_tilesCount = m_tilesWidth * m_tilesHeight;
	}
	else
	{
		m_tilesWidth = 0;
		m_tilesHeight = 0;
		m_tilesCount = 0;
	}
}

void NavMeshTool::HandleViewOptions()
{
	if (m_zoneProj)
	{
		
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
		ImGui::TextColored(col, "%d Tiles (%d x %d)", tt, m_tilesWidth, m_tilesHeight);

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
		if (ImGuiEx::ColoredButton("Reset Settings", ImVec2(100, 0), 0.0))
		{
			m_navMeshProj->ResetNavMeshConfig();
		}

		auto& config = m_navMeshProj->GetNavMeshConfig();

		if (ImGui::TreeNodeEx("Tiling", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen))
		{

			ImGui::SliderFloat("Tile Size", &config.tileSize, 16.0f, 1024.0f, "%.0f");

			UpdateTileSizes();

			ImGui::SliderFloat("Cell Size", &config.cellSize, 0.1f, 1.0f);
			ImGui::SliderFloat("Cell Height", &config.cellHeight, 0.1f, 1.0f);
		}

		if (ImGui::TreeNodeEx("Agent", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen))
		{
			ImGui::SliderFloat("Height", &config.agentHeight, 0.1f, 15.0f, "%.1f");
			ImGui::SliderFloat("Radius", &config.agentRadius, 0.1f, 15.0f, "%.1f");
			ImGui::SliderFloat("Max Climb", &config.agentMaxClimb, 0.1f, 15.0f, "%.1f");
			ImGui::SliderFloat("Max Slope", &config.agentMaxSlope, 0.0f, 90.0f, "%.1f");
		}

		if (m_zoneProj &&
			ImGui::TreeNodeEx("Bounding Box", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_NoTreePushOnOpen))
		{
			glm::vec3 min, max;
			m_navMesh->GetNavMeshBounds(min, max);

			glm::vec3 bmin = m_zoneProj->GetMeshBoundsMin();
			glm::vec3 bmax = m_zoneProj->GetMeshBoundsMax();

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

			ImGui::SliderFloat("Min Region Size", &config.regionMinSize, 0.0f, 150.0f, "%.0f");
			ImGui::SliderFloat("Merged Region Size", &config.regionMergeSize, 0.0f, 150.0f, "%.0f");

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
			ImGui::Combo("Partition Type", (int*)&config.partitionType, partition_types, 3);

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

			ImGui::SliderFloat("Max Edge Length", &config.edgeMaxLen, 0.0f, 50.0f, "%.0f");
			ImGui::SliderFloat("Max Edge Error", &config.edgeMaxError, 0.1f, 3.0f, "%.1f");
			ImGui::SliderFloat("Verts Per Poly", &config.vertsPerPoly, 3.0f, 12.0f, "%.0f");

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

			ImGui::SliderFloat("Sample Distance", &config.detailSampleDist, 0.0f, 0.9f, "%.2f");
			ImGui::SliderFloat("Max Sample Error", &config.detailSampleMaxError, 0.0f, 100.0f, "%.1f");
		}
	}
}

void NavMeshTool::handleRender(const glm::mat4& viewModelProjMtx, const glm::ivec4& viewport)
{
	m_viewModelProjMtx = viewModelProjMtx;
	m_viewport = viewport;

	if (!m_zoneProj || !m_zoneProj->IsZoneLoaded())
		return;

	if (m_tool)
		m_tool->handleRender();

	renderToolStates();
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

void NavMeshTool::GetTilePos(const glm::vec3& pos, int& tx, int& ty)
{
	if (!m_zoneProj) return;

	const glm::vec3& bmin = m_navMesh->GetNavMeshBoundsMin();
	auto& config = m_navMeshProj->GetNavMeshConfig();

	const float ts = config.tileSize * config.cellSize;
	tx = (int)((pos[0] - bmin[0]) / ts);
	ty = (int)((pos[2] - bmin[2]) / ts);
}

void NavMeshTool::RemoveTile(const glm::vec3& pos)
{
	if (!m_navMeshProj) return;

	m_navMeshProj->RemoveTile(pos);
}

void NavMeshTool::RemoveAllTiles()
{
	if (!m_navMeshProj) return;

	m_navMeshProj->RemoveAllTiles();
}

//----------------------------------------------------------------------------

float NavMeshTool::GetLastBuildTime() const
{
	if (!m_navMeshProj)
		return 0.0f;

	auto navMeshBuilder = m_navMeshProj->GetBuilder();
	if (!navMeshBuilder)
		return 0.0f;

	return navMeshBuilder->GetLastBuildTimeMS();
}

bool NavMeshTool::BuildMesh()
{
	auto navMeshBuilder = m_navMeshProj->GetBuilder();
	if (!navMeshBuilder)
	{
		SPDLOG_ERROR("NavMeshTool::BuildMesh(): No navmesh builder");
		return false;
	}

	// Note: this is called on the builder thread
	auto callback = [](bool success, float elapsedTime)
		{
			if (success)
			{
				SPDLOG_INFO("NavMeshTool::BuildMesh(): NavMesh built in {} ms", elapsedTime);
			}
			else
			{
				SPDLOG_ERROR("NavMeshTool::BuildMesh(): Failed to build NavMesh");
			}
		};

	if (!navMeshBuilder->BuildNavMesh(callback))
		return false;

	if (m_tool)
	{
		m_tool->init(this);
	}

	initToolStates();

	return true;
}

void NavMeshTool::BuildTile(const glm::vec3& pos)
{
	auto navMeshBuilder = m_navMeshProj->GetBuilder();
	if (!navMeshBuilder)
	{
		SPDLOG_ERROR("NavMeshTool::BuildMesh(): No navmesh builder");
		return;
	}

	navMeshBuilder->BuildTile(pos);
}

void NavMeshTool::RebuildTiles(const std::vector<dtTileRef>& tiles)
{
	auto navMeshBuilder = m_navMeshProj->GetBuilder();
	if (!navMeshBuilder)
	{
		SPDLOG_ERROR("NavMeshTool::RebuildTiles(): No navmesh builder");
		return;
	}

	navMeshBuilder->RebuildTiles(tiles);
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
