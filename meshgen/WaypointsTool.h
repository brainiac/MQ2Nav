//
// WaypointsTool.h
//

#pragma once

#include "meshgen/NavMeshTool.h"

#include <cstdint>

// Tool to create and edit waypoints

class WaypointsTool : public Tool
{
public:
	WaypointsTool();
	~WaypointsTool();

	virtual ToolType type() const override { return ToolType::WAYPOINTS; }
	virtual void init(NavMeshTool* meshTool) override;
	virtual void reset() override;
	virtual void handleMenu() override;
	virtual void handleClick(const glm::vec3& s, const glm::vec3& p, bool shift) override;
	virtual void handleUpdate(float dt) override;
	virtual void handleRender() override;
	virtual void handleRenderOverlay(const glm::mat4& proj,
		const glm::mat4& model, const glm::ivec4& view) override;

private:
	NavMeshTool* m_meshTool = nullptr;
	glm::vec3 m_hitPos;
	bool m_hitPosSet = false;
	bool m_bidir = true;
	int8_t m_oldFlags = 0;
};
