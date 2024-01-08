//
// NavMeshInfoTool.h
//

#pragma once

#include "meshgen/NavMeshTool.h"

// Tool to create and edit waypoints

class NavMeshInfoTool : public Tool
{
public:
	NavMeshInfoTool();
	~NavMeshInfoTool() override;

	virtual ToolType type() const override { return ToolType::INFO; }
	virtual void init(NavMeshTool* meshTool) override;
	virtual void reset() override;
	virtual void handleMenu() override;
	virtual void handleClick(const glm::vec3& p, bool shift) override;
	virtual void handleUpdate(float dt) override;
	virtual void handleRender() override;
	virtual void handleRenderOverlay() override;

private:
	NavMeshTool* m_meshTool = nullptr;
};
