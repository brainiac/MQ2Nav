//
// ConvexVolumeTool.cpp
//

#include "pch.h"
#include "common/Utilities.h"
#include "im3d/im3d.h"
#include "imgui/ImGuiUtils.h"
#include "meshgen/ConvexVolumeTool.h"
#include "meshgen/ImGuiWidgets.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/ZoneRenderManager.h"

#include <Recast.h>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#include <functional>

// Quick and dirty convex hull.

// Calculates convex hull on xz-plane of points on 'pts',
// stores the indices of the resulting hull in 'out' and
// returns number of points on hull.
static void convexhull(const std::vector<glm::vec3>& pts, std::vector<uint32_t>& out)
{
	// Find lower-leftmost point.
	uint32_t hull = 0;
	for (uint32_t i = 1; i < static_cast<uint32_t>(pts.size()); ++i)
		if (cmppt(pts[i], pts[hull]))
			hull = i;
	// Gift wrap hull.
	uint32_t endpt = 0;
	out.clear();
	do
	{
		out.push_back(hull);
		endpt = 0;
		for (uint32_t j = 1; j < static_cast<uint32_t>(pts.size()); ++j)
			if (hull == endpt || left(pts[hull], pts[endpt], pts[j]))
				endpt = j;
		hull = endpt;
	} while (endpt != out[0]);
}

static int pointInPoly(const std::vector<glm::vec3>& verts, const glm::vec3& p)
{
	bool c = false;

	for (size_t i = 0, j = verts.size() - 1; i < verts.size(); j = i++)
	{
		const glm::vec3& vi = verts[i];
		const glm::vec3& vj = verts[j];

		if (((vi[2] > p[2]) != (vj[2] > p[2])) &&
			(p[0] < (vj[0] - vi[0]) * (p[2] - vi[2]) / (vj[2] - vi[2]) + vi[0]))
		{
			c = !c;
		}
	}
	return c;
}

//----------------------------------------------------------------------------

ConvexVolumeTool::ConvexVolumeTool()
{
}

ConvexVolumeTool::~ConvexVolumeTool()
{
}

void ConvexVolumeTool::init(NavMeshTool* meshTool)
{
	m_meshTool = meshTool;

	m_state = (ConvexVolumeToolState*)m_meshTool->getToolState(type());
	if (!m_state)
	{
		m_state = new ConvexVolumeToolState();
		m_meshTool->setToolState(type(), m_state);
	}

	m_state->init(m_meshTool);
}

void ConvexVolumeTool::reset()
{
	m_editing = false;
}

void ConvexVolumeTool::handleMenu()
{
	auto navMesh = m_meshTool->GetNavMesh();
	if (!navMesh) return;

	// show list of existing convex volumes
	if (mq::imgui::CollapsingSubHeader("Help"))
	{
		ImGui::TextWrapped("Volumes can be used to mark parts of the map with different area types, including"
			" unwalkable areas. You can also create custom areas with modified travel costs, making certain"
			" areas cheaper or more expensive to travel. When planning paths, cheaper areas are preferred.\n");

		ImGui::TextWrapped("To create a new volume, click 'Create New'. To edit an existing volume, click on it"
			" in the list of volumes.\n");

		ImGui::TextWrapped("Click on the mesh to place points to create a volume. Alt-LMB or "
			"press 'Create Volume' to generate the volume from the points. Clear shape to cancel.");

		ImGui::Separator();
	}

	ImGui::Text("%d Volumes", navMesh->GetConvexVolumeCount());
	ImGui::BeginChild("VolumeList", ImVec2(0, 200), true);

	ImGuiStyle& style = ImGui::GetStyle();
	float w = ImGui::GetContentRegionAvail().x;
	float spacing = style.ItemInnerSpacing.x;
	float button_sz = ImGui::GetFrameHeight();

	ImGui::Columns(2, nullptr, false);

	for (size_t i = 0; i < navMesh->GetConvexVolumeCount(); ++i)
	{
		ConvexVolume* volume = navMesh->GetConvexVolume(i);
		const PolyAreaType& area = navMesh->GetPolyArea(volume->areaType);

		ImGui::PushID((int)i);

		bool isFirst = (i == 0);
		bool isLast = (i == navMesh->GetConvexVolumeCount() - 1);

		char label[256];
		const char* volumeName = volume->name.empty() ? "unnamed" : volume->name.c_str();

		ImGui::SetColumnWidth(-1, w - spacing * 2.0f - button_sz * 2.0f);

		if (!area.valid)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(255, 0, 0));

			sprintf_s(label, "%04d: %s (Invalid Area %d)", volume->id, volumeName, volume->areaType);
		}
		else
		{
			if (area.name.empty())
				sprintf_s(label, "%04d: %s (Area %d)", volume->id, volumeName, volume->areaType);
			else
				sprintf_s(label, "%04d: %s (%s)", volume->id, volumeName, area.name.c_str());
		}

		bool selected = (m_state->m_currentVolumeId == volume->id);


		if (ImGui::Selectable(label, &selected))
		{
			if (selected)
			{
				m_state->reset();
				m_state->m_editVolume = *volume;
				m_state->m_currentVolumeId = volume->id;
				m_editing = false;
			}
		}

		if (!area.valid)
		{
			ImGui::PopStyleColor(1);
		}

		ImGui::NextColumn();

		bool moved = false;

		if (isFirst)
		{
			// Maybe could be made into a helper function
			ImGui::PushStyleColor(ImGuiCol_Text, GImGui->Style.Colors[ImGuiCol_TextDisabled]);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		if (ImGui::ArrowButton("##up", ImGuiDir_Up) && !isFirst)
		{
			navMesh->MoveConvexVolumeToIndex(volume->id, i - 1);
			moved = true;
		}

		if (isFirst)
		{
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
			ImGui::PopStyleColor();
		}

		ImGui::SameLine();

		if (isLast)
		{
			// Maybe could be made into a helper function
			ImGui::PushStyleColor(ImGuiCol_Text, GImGui->Style.Colors[ImGuiCol_TextDisabled]);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		if (ImGui::ArrowButton("##down", ImGuiDir_Down) && !isLast)
		{
			navMesh->MoveConvexVolumeToIndex(volume->id, i + 1);
			moved = true;
		}

		if (isLast)
		{
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
			ImGui::PopStyleColor();
		}

		ImGui::NextColumn();

		if (moved)
		{
			auto modifiedTiles = navMesh->GetTilesIntersectingConvexVolume(volume->id);
			if (!modifiedTiles.empty())
			{
				m_meshTool->RebuildTiles(modifiedTiles);
			}
		}

		ImGui::PopID();
	}
	ImGui::EndChild();

	ImGui::Columns(1);

	{
		ImGui::BeginChild("##buttons", ImVec2(0, 30), false);
		ImGui::Columns(3, 0, false);

		if (ImGui::Button("Create New", ImVec2(-1, 0)))
		{
			m_state->reset();
			m_editing = true;
		}

		ImGui::NextColumn();
		ImGui::NextColumn();

		if (!m_editing
			&& m_state->m_currentVolumeId != 0
			&& ImGuiEx::ColoredButton("Delete", ImVec2(-1, 0), 0.0))
		{
			auto modifiedTiles = navMesh->GetTilesIntersectingConvexVolume(m_state->m_currentVolumeId);
			navMesh->DeleteConvexVolumeById(m_state->m_currentVolumeId);

			if (!modifiedTiles.empty())
			{
				m_meshTool->RebuildTiles(modifiedTiles);
			}
			m_state->m_currentVolumeId = 0;
		}

		ImGui::Columns(1);
		ImGui::EndChild();
	}

	if (m_editing)
	{
		ImGui::Text("Create New Volume");
		ImGui::Separator();

		ImGui::InputText("Name", m_state->m_name, 256);
		AreaTypeCombo(navMesh.get(), &m_state->m_areaType);
		ImGui::SliderFloat("Shape Height", &m_state->m_boxHeight, 0.1f, 100.0f);
		ImGui::SliderFloat("Shape Descent", &m_state->m_boxDescent, -100.f, 100.f);
		ImGui::SliderFloat("Poly Offset", &m_state->m_polyOffset, 0.0f, 10.0f);

		ImGui::Columns(3, 0, false);
		if (m_state->m_hull.size() > 2 && ImGuiEx::ColoredButton("Create Volume", ImVec2(0, 0), 0.28f))
		{
			auto modifiedTiles = m_state->CreateShape();
			if (!modifiedTiles.empty())
			{
				m_meshTool->RebuildTiles(modifiedTiles);
			}
			m_editing = false;
		}
		ImGui::NextColumn();
		ImGui::NextColumn();
		if (ImGuiEx::ColoredButton("Cancel", ImVec2(-1, 0), 0.0))
		{
			m_state->reset();
			m_editing = false;
		}
		ImGui::Columns(1);
	}
	else if (m_state->m_currentVolumeId != 0)
	{
		ImGui::Text("Edit Volume");
		ImGui::Separator();

		char name[256];
		sprintf_s(name, m_state->m_editVolume.name.c_str());
		if (ImGui::InputText("Name", name, 256))
		{
			m_state->m_modified = true;
			m_state->m_editVolume.name = name;
		}

		m_state->m_modified |= AreaTypeCombo(navMesh.get(), &m_state->m_editVolume.areaType);

		m_state->m_modified |= ImGui::InputFloat("Height Min", &m_state->m_editVolume.hmin, 1.0, 10.0, "%.1f");
		m_state->m_modified |= ImGui::InputFloat("Height Max", &m_state->m_editVolume.hmax, 1.0, 10.0, "%.1f");

		if (m_state->m_modified && ImGui::Button("Save Changes"))
		{
			if (ConvexVolume* vol = navMesh->GetConvexVolumeById(m_state->m_currentVolumeId))
			{
				vol->areaType = m_state->m_editVolume.areaType;
				vol->hmin = m_state->m_editVolume.hmin;
				vol->hmax = m_state->m_editVolume.hmax;
				vol->name = m_state->m_editVolume.name;
				vol->verts = m_state->m_editVolume.verts;

				auto modifiedTiles = navMesh->GetTilesIntersectingConvexVolume(vol->id);
				if (!modifiedTiles.empty())
				{
					m_meshTool->RebuildTiles(modifiedTiles);
				}
			}

			m_state->m_modified = false;
		}
	}
	else
	{
		m_state->m_currentVolumeId = 0;
	}

	if (m_state->m_currentVolumeId != 0)
	{
		//static Im3d::Mat4 transform(1.0f);
		//// Context-global gizmo modes are set via actions in the AppData::m_keyDown but could also be modified via a GUI as follows:
		//int gizmoMode = (int)Im3d::GetContext().m_gizmoMode;
		//ImGui::Checkbox("Local (Ctrl+L)", &Im3d::GetContext().m_gizmoLocal);
		//ImGui::SameLine();
		//ImGui::RadioButton("Translate (Ctrl+T)", &gizmoMode, Im3d::GizmoMode_Translation);
		//ImGui::SameLine();
		//ImGui::RadioButton("Rotate (Ctrl+R)", &gizmoMode, Im3d::GizmoMode_Rotation);
		//ImGui::SameLine();
		//ImGui::RadioButton("Scale (Ctrl+S)", &gizmoMode, Im3d::GizmoMode_Scale);
		//Im3d::GetContext().m_gizmoMode = (Im3d::GizmoMode)gizmoMode;

		//// The ID passed to Gizmo() should be unique during a frame - to create gizmos in a loop use PushId()/PopId().
		//if (Im3d::Gizmo("GizmoUnified", transform))
		//{
		//	// if Gizmo() returns true, the transform was modified
		//	switch (Im3d::GetContext().m_gizmoMode)
		//	{
		//	case Im3d::GizmoMode_Translation:
		//	{
		//		Im3d::Vec3 pos = transform.getTranslation();
		//		ImGui::Text("Position: %.3f, %.3f, %.3f", pos.x, pos.y, pos.z);
		//		break;
		//	}
		//	case Im3d::GizmoMode_Rotation:
		//	{
		//		Im3d::Vec3 euler = Im3d::ToEulerXYZ(transform.getRotation());
		//		ImGui::Text("Rotation: %.3f, %.3f, %.3f", Im3d::Degrees(euler.x), Im3d::Degrees(euler.y), Im3d::Degrees(euler.z));
		//		break;
		//	}
		//	case Im3d::GizmoMode_Scale:
		//	{
		//		Im3d::Vec3 scale = transform.getScale();
		//		ImGui::Text("Scale: %.3f, %.3f, %.3f", scale.x, scale.y, scale.z);
		//		break;
		//	}
		//	default: break;
		//	};

		//}
	}
}

void ConvexVolumeTool::handleClick(const glm::vec3& p, bool shift)
{
	// if we're not editing a volume right now, switch to edit mode
	if (m_state->m_currentVolumeId == 0)
		m_editing = true;

	if (!m_editing) return;

	if (!m_meshTool) return;
	auto navMesh = m_meshTool->GetNavMesh();

	std::vector<dtTileRef> modifiedTiles = m_state->handleVolumeClick(p, shift);

	if (!modifiedTiles.empty())
	{
		m_meshTool->RebuildTiles(modifiedTiles);
	}
}

void ConvexVolumeTool::handleRenderOverlay()
{
	if (m_editing)
	{
		// Tool help
		if (m_state->m_pts.empty())
		{
			ImGui::TextColored(ImVec4(255, 255, 255, 192),
				"LMB: Create new shape. SHIFT+LMB: Delete existing shape (click inside a shape).");
		}
		else
		{
			ImGui::TextColored(ImVec4(255, 255, 255, 192),
				"Click LMB to add new points. Alt+Click to finish the shape.");
			ImGui::TextColored(ImVec4(255, 255, 255, 192),
				"The shape will be convex hull of all added points.");
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(255, 255, 255, 192),
			"LMB: Click to start new shape.");
	}
}

//----------------------------------------------------------------------------

void ConvexVolumeToolState::init(NavMeshTool* meshTool)
{
	m_meshTool = meshTool;
}

void ConvexVolumeToolState::reset()
{
	m_pts.clear();
	m_hull.clear();
	m_currentVolumeId = 0;
	m_modified = false;
	m_editVolume = ConvexVolume{};
	m_name[0] = 0;
}

void ConvexVolumeToolState::handleRender()
{
	ZoneRenderDebugDraw dd(g_zoneRenderManager);

	// Find height extents of the shape.
	float minh = FLT_MAX, maxh = 0;

	if (m_currentVolumeId != 0)
	{
		minh = m_editVolume.hmin;
		maxh = m_editVolume.hmax;

		dd.begin(DU_DRAW_POINTS, 4.0f);
		for (size_t i = 0; i < m_editVolume.verts.size(); ++i)
		{
			unsigned int col = duRGBA(255, 255, 255, 255);
			if (i == m_editVolume.verts.size() - 1)
				col = duRGBA(240, 32, 16, 255);
			dd.vertex(m_editVolume.verts[i].x,
				m_editVolume.verts[i].y + 0.1f,
				m_editVolume.verts[i].z, col);
		}
		dd.end();

		dd.begin(DU_DRAW_LINES, 2.0f);
		for (size_t i = 0, j = m_editVolume.verts.size() - 1; i < m_editVolume.verts.size(); j = i++)
		{
			const glm::vec3& vi = m_editVolume.verts[j];
			const glm::vec3& vj = m_editVolume.verts[i];
			dd.vertex(vj.x, minh, vj.z, duRGBA(255, 255, 255, 64));
			dd.vertex(vi.x, minh, vi.z, duRGBA(255, 255, 255, 64));
			dd.vertex(vj.x, maxh, vj.z, duRGBA(255, 255, 255, 64));
			dd.vertex(vi.x, maxh, vi.z, duRGBA(255, 255, 255, 64));
			dd.vertex(vj.x, minh, vj.z, duRGBA(255, 255, 255, 64));
			dd.vertex(vj.x, maxh, vj.z, duRGBA(255, 255, 255, 64));
		}
		dd.end();
	}
	else
	{
		for (size_t i = 0; i < m_pts.size(); ++i)
			minh = glm::min(minh, m_pts[i].y);
		minh -= m_boxDescent;
		maxh = minh + m_boxHeight;

		dd.begin(DU_DRAW_POINTS, 4.0f);
		for (size_t i = 0; i < m_pts.size(); ++i)
		{
			unsigned int col = duRGBA(255, 255, 255, 255);
			if (i == m_pts.size() - 1)
				col = duRGBA(240, 32, 16, 255);
			dd.vertex(m_pts[i].x, m_pts[i].y + 0.1f, m_pts[i].z, col);
		}
		dd.end();

		dd.begin(DU_DRAW_LINES, 2.0f);
		for (size_t i = 0, j = m_hull.size() - 1; i < m_hull.size(); j = i++)
		{
			const glm::vec3& vi = m_pts[m_hull[j]];
			const glm::vec3& vj = m_pts[m_hull[i]];
			dd.vertex(vj.x, minh, vj.z, duRGBA(255, 255, 255, 64));
			dd.vertex(vi.x, minh, vi.z, duRGBA(255, 255, 255, 64));
			dd.vertex(vj.x, maxh, vj.z, duRGBA(255, 255, 255, 64));
			dd.vertex(vi.x, maxh, vi.z, duRGBA(255, 255, 255, 64));
			dd.vertex(vj.x, minh, vj.z, duRGBA(255, 255, 255, 64));
			dd.vertex(vj.x, maxh, vj.z, duRGBA(255, 255, 255, 64));
		}
		dd.end();
	}
}

void ConvexVolumeToolState::handleRenderOverlay()
{
}

std::vector<dtTileRef> ConvexVolumeToolState::handleVolumeClick(const glm::vec3& p, bool shift)
{
	auto navMesh = m_meshTool->GetNavMesh();
	std::vector<dtTileRef> modifiedTiles;

	if (shift)
	{
		// Delete
		size_t nearestIndex = -1;
		const auto& volumes = navMesh->GetConvexVolumes();
		size_t i = 0;
		for (const auto& vol : volumes)
		{
			if (pointInPoly(vol->verts, p) && p.y >= vol->hmin && p.y <= vol->hmax)
			{
				nearestIndex = i;
				break;
			}

			i++;
		}

		// If end point close enough, delete it.
		if (nearestIndex != -1)
		{
			const ConvexVolume* volume = navMesh->GetConvexVolume(nearestIndex);
			modifiedTiles = navMesh->GetTilesIntersectingConvexVolume(volume->id);
			m_meshTool->GetNavMesh()->DeleteConvexVolumeById(volume->id);
		}
	}
	else
	{
		// Create
		bool alt = false; // (SDL_GetModState() & KMOD_ALT) != 0;

		// If clicked on that last pt, create the shape.
		if (!m_pts.empty() && (alt || distSqr(p, m_pts[m_pts.size() - 1]) < rcSqr(0.2f)))
		{
			modifiedTiles = CreateShape();
		}
		else
		{
			// Add new point
			m_pts.push_back(p);

			// Update hull.
			if (m_pts.size() >= 2)
				convexhull(m_pts, m_hull);
			else
				m_hull.clear();
		}
	}

	return modifiedTiles;
}

std::vector<dtTileRef> ConvexVolumeToolState::CreateShape()
{
	std::vector<dtTileRef> modifiedTiles;
	auto navMesh = m_meshTool->GetNavMesh();

	if (m_hull.size() > 2)
	{
		std::vector<glm::vec3> verts(m_hull.size());

		// Create shape.
		for (size_t i = 0; i < m_hull.size(); ++i)
			verts[i] = m_pts[m_hull[i]];

		float minh = FLT_MAX, maxh = 0;
		for (size_t i = 0; i < m_hull.size(); ++i)
			minh = glm::min(minh, verts[i].y);

		minh -= m_boxDescent;
		maxh = minh + m_boxHeight;

		if (m_polyOffset > 0.01f)
		{
			std::vector<glm::vec3> offset(m_hull.size() * 2 + 1);

			int noffset = rcOffsetPoly(glm::value_ptr(verts[0]), static_cast<int>(verts.size()),
				m_polyOffset, glm::value_ptr(offset[0]), static_cast<int>(verts.size() * 2 + 1));
			if (noffset > 0)
			{
				offset.resize(noffset);

				ConvexVolume* volume = navMesh->AddConvexVolume(offset, m_name, minh, maxh, (uint8_t)m_areaType);
				modifiedTiles = navMesh->GetTilesIntersectingConvexVolume(volume->id);
			}
		}
		else
		{
			ConvexVolume* volume = navMesh->AddConvexVolume(verts, m_name, minh, maxh, (uint8_t)m_areaType);
			modifiedTiles = navMesh->GetTilesIntersectingConvexVolume(volume->id);
		}
	}

	reset();

	return modifiedTiles;
}
