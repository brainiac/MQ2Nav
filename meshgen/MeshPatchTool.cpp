//
// MeshPatchTool.cpp
//

#include "MeshPatchTool.h"

#include "common/MathUtil.h"
#include "common/NavMeshData.h"
#include "meshgen/Application.h"
#include "meshgen/Editor.h"
#include "meshgen/NavMeshTool.h"
#include "meshgen/ZoneProject.h"
#include "meshgen/ZoneRenderManager.h"

#include <DetourDebugDraw.h>

#include <fmt/format.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <glm/gtc/type_ptr.hpp>

//
// A block is entered as the center of its top face, since that is the surface being walked
// on and the point the user is actually aiming at - standing at the spot and snapshotting
// the camera then puts the block underfoot. Storage is two opposite corners, so the two
// conversions below are the only places that know the difference.

static void BlockCornersFromTopCenter(const glm::vec3& posEq, const glm::vec3& sizeEq,
	glm::vec3& outLo, glm::vec3& outHi)
{
	const glm::vec3 center = from_eq_coord(posEq);
	const glm::vec3 size = glm::abs(from_eq_coord(sizeEq));

	outLo = { center.x - size.x * 0.5f, center.y - size.y, center.z - size.z * 0.5f };
	outHi = { center.x + size.x * 0.5f, center.y,          center.z + size.z * 0.5f };
}

static void BlockTopCenterFromCorners(const glm::vec3& c0, const glm::vec3& c1,
	glm::vec3& outPosEq, glm::vec3& outSizeEq)
{
	const glm::vec3 lo = glm::min(c0, c1);
	const glm::vec3 hi = glm::max(c0, c1);

	outPosEq = to_eq_coord(glm::vec3{ (lo.x + hi.x) * 0.5f, hi.y, (lo.z + hi.z) * 0.5f });
	outSizeEq = to_eq_coord(hi - lo);
}

// Recast strips walkable surface back from every drop-off, twice over: rcFilterLedgeSpans
// nulls about a cell around the rim of anything raised, and rcErodeWalkableArea then takes
// walkableRadius more. At the default cell size and agent radius that is roughly three units
// in from every edge. A steep patch that stops exactly at the ledge it is climbing to
// therefore ends inside that dead band, and comes out as an island with nothing joining it.
//
// Overshooting both ends puts the patch surface well past the band, so the two rasterize
// into one continuous surface. Each end keeps its own height and only moves horizontally, so
// the quad stays planar and the ring stays four vertices.
//
// Flat patches meet coplanar ground with no drop-off and no dead band, so they are left
// alone rather than made to overshoot into geometry that did not need it.
static constexpr float kSteepPatchSlopeDegrees = 15.0f;

static bool IsSteep(const glm::vec3& from, const glm::vec3& to, float& outRun, glm::vec3& outDir)
{
	const glm::vec3 flat{ to.x - from.x, 0.0f, to.z - from.z };

	outRun = glm::length(flat);

	// Purely vertical, so there is no direction of travel to lay a lip along.
	if (outRun < 1e-4f)
		return false;

	outDir = flat / outRun;

	return glm::degrees(std::atan2(std::abs(to.y - from.y), outRun)) >= kSteepPatchSlopeDegrees;
}

// The four vertex ring older patches were built from, before strips: the two ends were laid
// down in order, so each is the midpoint of its own pair and the width is the span between
// them. Anything not built that way is left alone rather than guessed at.
static bool SurfaceEndsFromRing(const std::vector<glm::vec3>& verts,
	glm::vec3& outStart, glm::vec3& outEnd, float& outWidth)
{
	if (verts.size() != 4)
		return false;

	outStart = (verts[0] + verts[1]) * 0.5f;
	outEnd = (verts[2] + verts[3]) * 0.5f;
	outWidth = glm::length(verts[1] - verts[0]);

	return true;
}

// ImGui only offers the +/- step buttons on the single component form, so a stepped vec3 has
// to be three rows. Worth the space where a value is nudged into place rather than typed.
// The labels are the EQ axis names, in the order these fields are already stored, so they
// read the same way as a position does in game.
static bool SteppedVec3(const char* id, const char* const (&labels)[3], glm::vec3& v, float step)
{
	bool changed = false;

	ImGui::PushID(id);

	for (int i = 0; i < 3; ++i)
		changed |= ImGui::InputFloat(labels[i], &v[i], step, step * 10.0f, "%.1f");

	ImGui::PopID();

	return changed;
}

// Sets a vec3 field from the camera. Used by the creator and by both editors, since flying
// to the spot beats reading coordinates off somewhere else and typing them in. The id is
// needed because several of these can be visible at once.
static bool CameraButton(const char* id, glm::vec3& outPosEq)
{
	ImGui::PushID(id);
	const bool clicked = ImGui::SmallButton("From Camera");
	ImGui::PopID();

	if (!clicked)
		return false;

	Editor* editor = Application::Get().GetEditor();
	if (!editor)
		return false;

	outPosEq = to_eq_coord(editor->GetCamera().GetPosition());
	return true;
}

MeshPatchTool::MeshPatchTool()
{
}

MeshPatchTool::~MeshPatchTool()
{
}

void MeshPatchTool::init(NavMeshTool* meshTool)
{
	m_meshTool = meshTool;

	m_state = (MeshPatchToolState*)m_meshTool->getToolState(type());
	if (!m_state)
	{
		m_state = new MeshPatchToolState();
		m_meshTool->setToolState(type(), m_state);
	}

	m_state->init(m_meshTool);
}

void MeshPatchTool::reset()
{
	m_state->reset();
}

void MeshPatchTool::handleMenu()
{
	auto navMesh = m_meshTool->GetNavMesh();
	if (!navMesh) return;

	const float agentRadius = navMesh->GetNavMeshConfig().agentRadius;

	m_state->initDefaultsFromConfig();

	ImGui::TextWrapped("Click a point on each side of the gap to lay a patch of geometry"
		" between them. The navmesh is rebuilt from it, so the two sides join properly"
		" instead of being bridged after the fact.");

	ImGui::Separator();

	ImGui::SliderFloat("Width", &m_state->m_width, 1.0f, 100.0f, "%.1f");

	// A raised strip has a drop on either side, so it loses a cell to rcFilterLedgeSpans and
	// an agent radius to rcErodeWalkableArea on each. Only what is left over is walkable, and
	// a patch that comes out with nothing looks exactly like a patch that did not work.
	const float deadBand = (agentRadius + navMesh->GetNavMeshConfig().cellSize) * 2.0f;
	const float usableWidth = m_state->m_width - deadBand;

	if (usableWidth <= 0.0f)
	{
		ImGui::TextColored(ImColor(255, 128, 0),
			"Too narrow: the ledge filter and erosion take %.1f off, so nothing walkable will"
			" remain. Use more than %.1f.", deadBand, deadBand);
	}
	else
	{
		ImGui::TextDisabled("About %.1f units walkable after the ledge filter and erosion.",
			usableWidth);
	}

	ImGui::SliderFloat("End Extension", &m_state->m_endExtension, 0.0f, 20.0f, "%.1f");

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("How far a steep patch overshoots each clicked point.\n\n"
			"Recast strips walkable surface back from every drop-off, so a patch\n"
			"that stops exactly at the ledge it climbs to ends inside that dead\n"
			"band and comes out disconnected. Overshooting makes the surfaces\n"
			"overlap instead. Patches shallower than %.0f degrees are left alone.",
			kSteepPatchSlopeDegrees);
	}

	ImGui::Checkbox("Snap To Top", &m_state->m_snapToTop);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Lift each click to the highest surface over it.\n\n"
			"A click lands on whichever face the ray met first, which on a block\n"
			"is often the underside or a vertical side rather than the top. Turn\n"
			"it off to lay a patch under something on purpose.");
	}

	if (m_state->m_snapToTop)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat("Search Height", &m_state->m_snapSearchHeight, 0.5f, 1.0f, 200.0f, "%.0f");

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("How far above a click to look. Tall enough to clear whatever\n"
				"was clicked through, short enough not to catch the ceiling.");
		}
	}

	if (m_state->m_hitPosSet)
	{
		ImGui::TextColored(ImColor(0, 255, 0), "Click the far side of the gap.");

		if (ImGui::Button("Cancel"))
			m_state->m_hitPosSet = false;
	}

	ImGui::Separator();

	// Block creator. Two clicks cannot describe a solid, and some repairs need one: a landing
	// standing out of nothing, or a plug for a crack the floor falls through.
	if (ImGui::CollapsingHeader("Block"))
	{
		ImGui::TextWrapped("A solid box, entered rather than clicked. Position is the center"
			" of the top face - the surface that gets walked on - in the same coordinates the"
			" tester tool uses. The block hangs below it.");

		static const char* const posLabels[3] = { "Y", "X", "Z" };
		static const char* const sizeLabels[3] = { "L", "W", "H" };

		ImGui::TextUnformatted("Position (top face center)");
		SteppedVec3("newpos", posLabels, m_state->m_blockPos, 1.0f);
		CameraButton("newposcam", m_state->m_blockPos);

		ImGui::TextUnformatted("Size");
		SteppedVec3("newsize", sizeLabels, m_state->m_blockSize, 1.0f);

		if (ImGui::Button("Create Block"))
		{
			std::vector<dtTileRef> modifiedTiles = m_state->addBlock();

			if (!modifiedTiles.empty())
				m_meshTool->RebuildTiles(modifiedTiles);
		}
	}

	ImGui::Separator();

	// Existing patches
	size_t count = navMesh->GetMeshPatchCount();
	ImGui::Text("%d patch%s", static_cast<int>(count), count == 1 ? "" : "es");

	uint32_t deleteId = 0;

	// Saving is deferred to after the loop: it regenerates the collision mesh and rebuilds
	// tiles, which is not something to be doing while walking the patch list.
	bool save = false;

	for (size_t i = 0; i < count; ++i)
	{
		MeshPatch* patch = navMesh->GetMeshPatch(i);

		ImGui::PushID(static_cast<int>(patch->id));

		std::string label = patch->name.empty()
			? fmt::format("{} {}", patch->type == MeshPatchType::Box ? "Block" : "Patch", patch->id)
			: patch->name;

		// Driven from the edit buffer rather than from ImGui's own storage, so that only one
		// patch is ever open. The buffer holds a single patch, and two open editors would
		// otherwise overwrite each other's values every frame.
		ImGui::SetNextItemOpen(m_state->m_editId == patch->id);

		const bool open = ImGui::TreeNode(label.c_str());

		ImGui::SameLine();

		if (ImGui::SmallButton("Delete"))
			deleteId = patch->id;

		if (open && m_state->m_editId != patch->id)
			m_state->beginEdit(patch);
		else if (!open && m_state->m_editId == patch->id)
			m_state->m_editId = 0;

		if (open)
		{
			m_state->m_editModified |= ImGui::InputText("Name", &m_state->m_editName);

			if (!m_state->m_editSupported)
			{
				ImGui::TextDisabled("%d vertices - not a two point patch, so not editable here.",
					static_cast<int>(patch->verts.size()));
			}
			else if (patch->type == MeshPatchType::Box)
			{
				static const char* const posLabels[3] = { "Y", "X", "Z" };
				static const char* const sizeLabels[3] = { "L", "W", "H" };

				ImGui::TextUnformatted("Position (top face center)");
				m_state->m_editModified |= SteppedVec3("pos", posLabels, m_state->m_editPos, 1.0f);
				m_state->m_editModified |= CameraButton("poscam", m_state->m_editPos);

				ImGui::TextUnformatted("Size");
				m_state->m_editModified |= SteppedVec3("size", sizeLabels, m_state->m_editSize, 1.0f);
			}
			else
			{
				m_state->m_editModified |=
					ImGui::InputFloat3("Start", glm::value_ptr(m_state->m_editPos));
				m_state->m_editModified |= CameraButton("startcam", m_state->m_editPos);

				m_state->m_editModified |=
					ImGui::InputFloat3("End", glm::value_ptr(m_state->m_editSize));
				m_state->m_editModified |= CameraButton("endcam", m_state->m_editSize);

				m_state->m_editModified |=
					ImGui::SliderFloat("Width", &m_state->m_editWidth, 1.0f, 100.0f, "%.1f");
			}

			ImGui::BeginDisabled(!m_state->m_editModified);

			if (ImGui::Button("Save Changes"))
				save = true;

			ImGui::EndDisabled();

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	if (save)
	{
		auto modifiedTiles = m_state->saveEdit();

		if (!modifiedTiles.empty())
			m_meshTool->RebuildTiles(modifiedTiles);
	}

	if (deleteId != 0)
	{
		// Collect the tiles before deleting: afterwards there is no patch to ask about.
		auto modifiedTiles = navMesh->GetTilesIntersectingMeshPatch(deleteId);

		navMesh->DeleteMeshPatchById(deleteId);

		// Deleting renumbers what is left, so a held id would now name a different patch.
		m_state->m_editId = 0;
		m_state->m_editModified = false;

		// Geometry can be appended to the collision mesh but not taken back out of it, so
		// removing a patch means regenerating the whole thing before the tiles rebuild.
		if (auto zoneProj = m_meshTool->GetZoneProj())
			zoneProj->RebuildCollisionMesh();

		if (!modifiedTiles.empty())
			m_meshTool->RebuildTiles(modifiedTiles);
	}
}

void MeshPatchTool::handleClick(const glm::vec3& p, bool shift)
{
	if (!m_meshTool) return;

	std::vector<dtTileRef> modifiedTiles = m_state->handlePatchClick(p, shift);

	if (!modifiedTiles.empty())
		m_meshTool->RebuildTiles(modifiedTiles);
}

//----------------------------------------------------------------------------

void MeshPatchToolState::init(NavMeshTool* meshTool)
{
	m_meshTool = meshTool;
}

void MeshPatchToolState::reset()
{
	m_hitPosSet = false;
	m_currentPatchId = 0;
	m_defaultsInitialized = false;
	m_editId = 0;
	m_editModified = false;
}

void MeshPatchToolState::initDefaultsFromConfig()
{
	if (m_defaultsInitialized)
		return;

	auto navMesh = m_meshTool->GetNavMesh();
	if (!navMesh)
		return;

	const NavMeshConfig& config = navMesh->GetNavMeshConfig();

	// A raised strip has a drop on both sides, so it loses a cell to the ledge filter and an
	// agent radius to erosion on each - that whole band is gone before anything is walkable.
	// This leaves an agent diameter of walkable surface once the band is taken off both
	// sides, which is the narrowest thing worth calling a path.
	m_width = config.agentRadius * 4.0f + config.cellSize * 2.0f;

	// One cell for the ledge filter, one agent radius for erosion, and another as margin so
	// the surfaces genuinely overlap rather than just touching at the far edge of the band.
	m_endExtension = config.cellSize + config.agentRadius * 2.0f;

	m_defaultsInitialized = true;
}

// Puts a newly added patch's geometry into the collision mesh and reports the tiles that need
// rebuilding. Recording the patch is not enough on its own: tiles are rebuilt from the
// collision mesh, so the geometry has to be in there first or nothing changes.
std::vector<dtTileRef> MeshPatchToolState::commitPatch(MeshPatch* patch)
{
	m_currentPatchId = patch->id;

	if (auto zoneProj = m_meshTool->GetZoneProj())
		zoneProj->AddMeshPatchGeometry(*patch);

	return m_meshTool->GetNavMesh()->GetTilesIntersectingMeshPatch(patch->id);
}

std::vector<dtTileRef> MeshPatchToolState::addBlock()
{
	auto navMesh = m_meshTool->GetNavMesh();
	if (!navMesh)
		return {};

	// A zero extent in any axis gives a box with no volume, which rasterizes to nothing.
	if (m_blockSize.x == 0.0f || m_blockSize.y == 0.0f || m_blockSize.z == 0.0f)
		return {};

	glm::vec3 lo, hi;
	BlockCornersFromTopCenter(m_blockPos, m_blockSize, lo, hi);

	MeshPatch* patch = navMesh->AddMeshPatch({ lo, hi }, std::string(), MeshPatchType::Box);

	return commitPatch(patch);
}

std::vector<glm::vec3> MeshPatchToolState::BuildPatchStrip(
	const glm::vec3& from, const glm::vec3& to, float width, float extension)
{
	// Widen across the horizontal direction of travel. A patch bridging a gap wants to be
	// perpendicular to the crossing, and staying horizontal keeps the width honest however
	// steep the span is - widening in the plane of a steep slope would pinch it once
	// projected back down.
	glm::vec3 flat{ to.x - from.x, 0.0f, to.z - from.z };

	if (glm::dot(flat, flat) < 1e-6f)
	{
		// Purely vertical: no meaningful direction of travel, so pick one arbitrarily
		// rather than produce a degenerate quad.
		flat = glm::vec3{ 1.0f, 0.0f, 0.0f };
	}

	const glm::vec3 side = glm::normalize(glm::vec3{ -flat.z, 0.0f, flat.x }) * (width * 0.5f);

	// The run itself is always the middle two pairs, so the clicked points are recoverable
	// whether or not lips were laid - which is what keeps a re-save from growing the patch.
	std::vector<glm::vec3> verts;
	verts.reserve(8);

	float run;
	glm::vec3 dir;
	const bool lips = extension > 0.0f && IsSteep(from, to, run, dir);

	// A lip continues from the clicked point horizontally, at that point's own height, so the
	// surface arrives at what it is joining rather than short of it. Extending the ramp
	// itself instead would either tilt it - putting the end below where it was clicked, which
	// is worse than doing nothing - or float it above, if extended along its own slope.
	if (lips)
	{
		const glm::vec3 lip = from - dir * extension;
		verts.push_back(lip - side);
		verts.push_back(lip + side);
	}

	verts.push_back(from - side);
	verts.push_back(from + side);
	verts.push_back(to - side);
	verts.push_back(to + side);

	if (lips)
	{
		const glm::vec3 lip = to + dir * extension;
		verts.push_back(lip - side);
		verts.push_back(lip + side);
	}

	return verts;
}

// Recovers the clicked points from a strip. The run is the middle pair of pairs when lips
// were laid and the whole thing when they were not, so what comes back is where the user
// clicked either way and re-saving lays the lips afresh rather than on top of the old ones.
bool MeshPatchToolState::StripEnds(const std::vector<glm::vec3>& verts,
	glm::vec3& outStart, glm::vec3& outEnd, float& outWidth)
{
	const size_t pairs = verts.size() / 2;

	if (verts.size() % 2 != 0 || (pairs != 2 && pairs != 4))
		return false;

	const size_t first = (pairs == 4) ? 2 : 0;

	outStart = (verts[first] + verts[first + 1]) * 0.5f;
	outEnd = (verts[first + 2] + verts[first + 3]) * 0.5f;
	outWidth = glm::length(verts[first + 1] - verts[first]);

	return true;
}

glm::vec3 MeshPatchToolState::SnapToTopSurface(const glm::vec3& p) const
{
	auto zoneProj = m_meshTool->GetZoneProj();
	if (!zoneProj)
		return p;

	// Straight down from the top of the search window: the nearest hit is the highest surface
	// over the spot. Starting above the click rather than at it is the point - a click that
	// landed on an underside or a vertical side is below the surface it was aiming at. The
	// window is bounded so this finds the block that was clicked and not the ceiling above it.
	const glm::vec3 src{ p.x, p.y + m_snapSearchHeight, p.z };
	const glm::vec3 dest{ p.x, p.y - 1.0f, p.z };

	float t;
	if (!zoneProj->RaycastMesh(src, dest, t))
		return p;

	return src + (dest - src) * t;
}

std::vector<dtTileRef> MeshPatchToolState::handlePatchClick(const glm::vec3& p, bool shift)
{
	auto navMesh = m_meshTool->GetNavMesh();
	if (!navMesh)
		return {};

	const glm::vec3 point = m_snapToTop ? SnapToTopSurface(p) : p;

	if (!m_hitPosSet)
	{
		m_hitPos = point;
		m_hitPosSet = true;

		return {};
	}

	m_hitPosSet = false;

	MeshPatch* patch = navMesh->AddMeshPatch(
		BuildPatchStrip(m_hitPos, point, m_width, m_endExtension), std::string(),
		MeshPatchType::Strip);

	return commitPatch(patch);
}

std::vector<dtTileRef> MeshPatchToolState::updatePatch(uint32_t id, const std::vector<glm::vec3>& verts)
{
	auto navMesh = m_meshTool->GetNavMesh();
	if (!navMesh)
		return {};

	MeshPatch* patch = navMesh->GetMeshPatchById(id);
	if (!patch)
		return {};

	// Where it was, before it moves. Those tiles have the old geometry baked into them and
	// have to be rebuilt too, or the patch leaves a copy of itself behind.
	std::vector<dtTileRef> tiles = navMesh->GetTilesIntersectingMeshPatch(id);

	patch->verts = verts;

	for (dtTileRef tile : navMesh->GetTilesIntersectingMeshPatch(id))
		tiles.push_back(tile);

	std::sort(tiles.begin(), tiles.end());
	tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());

	// Unlike adding, this cannot be done incrementally: the old triangles are already in the
	// collision mesh and there is no way to take them back out, so it is regenerated whole.
	if (auto zoneProj = m_meshTool->GetZoneProj())
		zoneProj->RebuildCollisionMesh();

	return tiles;
}

void MeshPatchToolState::beginEdit(const MeshPatch* patch)
{
	m_editId = patch->id;
	m_editModified = false;
	m_editName = patch->name;

	if (patch->type == MeshPatchType::Box)
	{
		m_editSupported = patch->verts.size() >= 2;

		if (m_editSupported)
			BlockTopCenterFromCorners(patch->verts[0], patch->verts[1], m_editPos, m_editSize);
	}
	else
	{
		glm::vec3 start, end;

		m_editSupported = (patch->type == MeshPatchType::Strip)
			? StripEnds(patch->verts, start, end, m_editWidth)
			: SurfaceEndsFromRing(patch->verts, start, end, m_editWidth);

		if (m_editSupported)
		{
			m_editPos = to_eq_coord(start);
			m_editSize = to_eq_coord(end);
		}
	}
}

std::vector<dtTileRef> MeshPatchToolState::saveEdit()
{
	auto navMesh = m_meshTool->GetNavMesh();
	if (!navMesh || m_editId == 0)
		return {};

	MeshPatch* patch = navMesh->GetMeshPatchById(m_editId);
	if (!patch)
		return {};

	// Costs nothing to apply: the name is not build input, so no tile has to change for it.
	// It is also the one thing editable on a patch whose geometry this editor cannot describe.
	patch->name = m_editName;
	m_editModified = false;

	if (!m_editSupported)
		return {};

	std::vector<glm::vec3> verts;

	if (patch->type == MeshPatchType::Box)
	{
		if (m_editSize.x == 0.0f || m_editSize.y == 0.0f || m_editSize.z == 0.0f)
			return {};

		glm::vec3 lo, hi;
		BlockCornersFromTopCenter(m_editPos, m_editSize, lo, hi);

		verts = { lo, hi };
	}
	else
	{
		// Rebuilt as a strip whatever it was, so an older four vertex patch picks up the end
		// lips the first time it is saved. StripEnds gives back the clicked points rather
		// than the lip ends, so this does not stack a second set of lips on the first.
		verts = BuildPatchStrip(from_eq_coord(m_editPos), from_eq_coord(m_editSize),
			m_editWidth, m_endExtension);

		patch->type = MeshPatchType::Strip;
	}

	// A rename on its own leaves the geometry alone, and regenerating the collision mesh for
	// it would be a few seconds spent to change a label.
	if (verts == patch->verts)
		return {};

	return updatePatch(m_editId, verts);
}

void MeshPatchToolState::handleRender()
{
	auto navMesh = m_meshTool->GetNavMesh();
	if (!navMesh)
		return;

	ZoneRenderDebugDraw dd(g_zoneRenderManager);
	const float s = navMesh->GetNavMeshConfig().agentRadius;

	// The pending first click, so it is clear the tool is waiting for a second one.
	if (m_hitPosSet)
	{
		duDebugDrawCross(&dd, m_hitPos[0], m_hitPos[1] + 0.1f, m_hitPos[2], s,
			duRGBA(255, 190, 0, 200), 2.0f);
	}

	// Outline every patch. They become ordinary geometry once the tiles rebuild, so without
	// this there is nothing to show which mesh came from a patch and which from the zone.
	const uint32_t color = duRGBA(255, 190, 0, 220);

	dd.begin(DU_DRAW_LINES, 2.0f);

	for (size_t i = 0; i < navMesh->GetMeshPatchCount(); ++i)
	{
		const MeshPatch* patch = navMesh->GetMeshPatch(i);
		const size_t n = patch->verts.size();

		if (patch->type == MeshPatchType::Box)
		{
			if (n < 2)
				continue;

			// Two corners, so the ring walk below does not apply - draw the twelve edges.
			const glm::vec3 lo = glm::min(patch->verts[0], patch->verts[1]);
			const glm::vec3 hi = glm::max(patch->verts[0], patch->verts[1]);

			duAppendBoxWire(&dd, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z, color);
			continue;
		}

		// A strip is pairs along its length rather than a ring, so walk one side out and the
		// other back to get a perimeter. Legacy four vertex patches are already a ring.
		std::vector<glm::vec3> ring;

		if (patch->type == MeshPatchType::Strip)
		{
			if (n < 4)
				continue;

			for (size_t j = 0; j < n; j += 2)
				ring.push_back(patch->verts[j]);

			for (size_t j = n; j > 0; j -= 2)
				ring.push_back(patch->verts[j - 1]);
		}
		else
		{
			ring.assign(patch->verts.begin(), patch->verts.end());
		}

		for (size_t j = 0; j < ring.size(); ++j)
		{
			const glm::vec3& a = ring[j];
			const glm::vec3& b = ring[(j + 1) % ring.size()];

			dd.vertex(a.x, a.y + 0.1f, a.z, color);
			dd.vertex(b.x, b.y + 0.1f, b.z, color);
		}
	}

	dd.end();
}
