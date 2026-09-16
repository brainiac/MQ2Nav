//
// MeshPatchTool.h
//

#pragma once

#include "meshgen/NavMeshTool.h"
#include "common/NavMeshData.h"

#include <cstdint>

// Tool to lay patches of geometry into the collision mesh, for the places the generator
// cannot produce connected mesh on its own. Two clicks: one on each side of the gap.

class MeshPatchToolState;
struct MeshPatch;

class MeshPatchTool : public Tool
{
public:
	MeshPatchTool();
	~MeshPatchTool() override;

	ToolType type() const override { return ToolType::MESH_PATCH; }
	void init(NavMeshTool* meshTool) override;
	void reset() override;
	void handleMenu() override;
	void handleClick(const glm::vec3& p, bool shift) override;

private:
	NavMeshTool* m_meshTool = nullptr;
	MeshPatchToolState* m_state = nullptr;
};

class MeshPatchToolState : public ToolState
{
	friend class MeshPatchTool;

public:
	void init(NavMeshTool* meshTool) override;
	void reset() override;
	void handleRender() override;
	void handleUpdate(const float dt) override {}

	std::vector<dtTileRef> handlePatchClick(const glm::vec3& p, bool shift);

	// Creates a solid block from the currently entered position and size. Returns the tiles
	// that need rebuilding, empty if the block would be degenerate.
	std::vector<dtTileRef> addBlock();

	// Replaces an existing patch's geometry. Returns every tile that needs rebuilding, which
	// is the ones the patch used to cover as well as the ones it covers now.
	std::vector<dtTileRef> updatePatch(uint32_t id, const std::vector<glm::vec3>& verts);

	// Opens a patch for editing, reading its values into the buffer below.
	void beginEdit(const MeshPatch* patch);

	// Writes the buffer back to the patch being edited.
	std::vector<dtTileRef> saveEdit();

	// The default width and end extension only make sense once the agent radius and cell size
	// are known, and there is no navmesh yet when the tool is constructed.
	void initDefaultsFromConfig();

	// Builds the strip for a patch spanning from one point to another, as left/right pairs
	// along its length. A steep run gets a flat lip at each end so that it arrives at what it
	// is joining instead of stopping inside the band Recast strips back from every drop-off.
	static std::vector<glm::vec3> BuildPatchStrip(const glm::vec3& from, const glm::vec3& to,
		float width, float extension);

	// Recovers the clicked points and width from a strip built by the above.
	static bool StripEnds(const std::vector<glm::vec3>& verts, glm::vec3& outStart,
		glm::vec3& outEnd, float& outWidth);

	// Moves a clicked point up to the topmost surface over it, within the search height.
	// Returns the point unchanged if there is nothing above it.
	glm::vec3 SnapToTopSurface(const glm::vec3& p) const;

private:
	// Puts a new patch's geometry into the collision mesh and returns the tiles to rebuild.
	std::vector<dtTileRef> commitPatch(MeshPatch* patch);

	NavMeshTool* m_meshTool = nullptr;

	bool m_hitPosSet = false;
	glm::vec3 m_hitPos;

	// Width of the strip laid between the two clicked points. Erosion takes agentRadius off
	// each side, so anything at or below 2 * agentRadius leaves no walkable surface at all;
	// the default is the narrowest useful value plus a little, since the tool is mostly used
	// to define a narrow path rather than to floor an area. Set from the config on first use.
	float m_width = 12.0f;

	// How far a steep patch overshoots each of its clicked points. Recast strips walkable
	// surface back from every drop-off, so a patch that stops exactly at the ledge it climbs
	// to ends inside that dead band and comes out disconnected. Flat patches meet coplanar
	// ground with no drop-off and are left alone. Set from the config on first use.
	float m_endExtension = 6.0f;

	// Clicking a block tends to land on whichever face the ray met first, which is often the
	// underside or a vertical side rather than the surface being aimed at. With this on, a
	// click is lifted to the topmost surface over it. Bounded by a search height rather than
	// unconditional, and switchable off, since a patch laid under something is a legitimate
	// thing to want.
	bool m_snapToTop = true;
	float m_snapSearchHeight = 15.0f;

	bool m_defaultsInitialized = false;

	// Block creator, in EQ coordinates to match what the tester tool shows and what the user
	// reads off in game. The position is the corner, not the center.
	glm::vec3 m_blockPos{ 0.0f, 0.0f, 0.0f };
	glm::vec3 m_blockSize{ 20.0f, 20.0f, 4.0f };

	// The patch open for editing and its values in flight. Editing writes here rather than to
	// the patch, because applying a change regenerates the whole collision mesh - far too
	// expensive to do per keystroke, and reading the patch back every frame would snap a
	// slider to where the drag started. Only one patch is open at a time so there is never a
	// second copy of these fighting over the same buffer.
	uint32_t m_editId = 0;
	bool m_editSupported = false;
	bool m_editModified = false;
	glm::vec3 m_editPos{};   // block: center of the top face. surface: start point
	glm::vec3 m_editSize{};  // block: L W H.                  surface: end point
	float m_editWidth = 0.0f;
	std::string m_editName;

	uint32_t m_currentPatchId = 0;
};
