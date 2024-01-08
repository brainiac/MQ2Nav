//
// NavMeshTileTool.h
//

#pragma once

#include "meshgen/NavMeshTool.h"

class NavMeshTileTool : public Tool
{
public:
	NavMeshTileTool() {}
	virtual ~NavMeshTileTool() {}

	virtual ToolType type() const override { return ToolType::TILE_EDIT; }
	virtual void init(NavMeshTool* meshTool) override;
	virtual void reset() override;
	virtual void handleMenu() override;
	virtual void handleClick(const glm::vec3& p, bool shift) override;
	virtual void handleUpdate(float /*dt*/) override {}
	virtual void handleRender() override;
	virtual void handleRenderOverlay() override;

private:
	NavMeshTool* m_meshTool = nullptr;
	glm::vec3 m_hitPos;
	bool m_hitPosSet = false;
};
