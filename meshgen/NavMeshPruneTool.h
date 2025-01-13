//
// NavMeshPruneTool.h
//

#pragma once

#include "meshgen/NavMeshTool.h"

#include <glm/glm.hpp>
#include <memory>

class NavmeshFlags;

// Prune navmesh to accessible locations from a point.
class NavMeshPruneTool : public Tool
{
public:
	NavMeshPruneTool();
	virtual ~NavMeshPruneTool();

	virtual ToolType type() const override { return ToolType::NAVMESH_PRUNE; }
	virtual void init(NavMeshTool* meshTool) override;
	virtual void reset() override;
	virtual void handleMenu() override;
	virtual void handleClick(const glm::vec3& p, bool shift) override;
	virtual void handleRender() override;
	virtual void handleRenderOverlay() override;

private:
	NavMeshTool* m_meshTool = nullptr;
	std::unique_ptr<NavmeshFlags> m_flags;
	glm::vec3 m_hitPos;
	bool m_hitPosSet = false;
};
