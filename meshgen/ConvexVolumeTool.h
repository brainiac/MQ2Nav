//
// ConvexVolumeTool.h
//

#pragma once

#include "meshgen/NavMeshTool.h"
#include "common/NavMeshData.h"

#include <glm/glm.hpp>
#include <vector>

class ConvexVolumeToolState;
struct ConvexVolume;

class ConvexVolumeTool : public Tool
{
public:
	ConvexVolumeTool();
	~ConvexVolumeTool() override;

	ToolType type() const override { return ToolType::CONVEX_VOLUME; }
	void init(NavMeshTool* meshTool) override;
	void reset() override;
	void handleMenu() override;
	void handleClick(const glm::vec3& p, bool shift) override;
	void handleUpdate(float dt) override {}
	void handleRender() override;
	void handleRenderOverlay() override;

private:
	NavMeshTool* m_meshTool = nullptr;
	ConvexVolumeToolState* m_state = nullptr;

	bool m_editing = false;
};

class ConvexVolumeToolState : public ToolState
{
	friend class ConvexVolumeTool;

public:
	void init(NavMeshTool* meshTool) override;
	void reset() override;
	void handleRender() override;
	void handleRenderOverlay() override;
	void handleUpdate(const float dt) override {}

	std::vector<dtTileRef> handleVolumeClick(const glm::vec3& p, bool shift);
	std::vector<dtTileRef> CreateShape();

private:
	NavMeshTool* m_meshTool = nullptr;

	// properties for constructing new area
	char m_name[256];
	uint8_t m_areaType = (uint8_t)PolyArea::Unwalkable;
	float m_polyOffset = 0.0f;
	float m_boxHeight = 6.0f;
	float m_boxDescent = 1.0f;
	std::vector<glm::vec3> m_pts;
	std::vector<uint32_t> m_hull;

	// editing existing volume. We copy so we can make edits without committing
	ConvexVolume m_editVolume;
	uint32_t m_currentVolumeId = 0; // valid ids start with 1
	bool m_modified = false;
};
