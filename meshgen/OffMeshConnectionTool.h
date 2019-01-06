//
// OffMeshConnectionTool.h
//

#pragma once

#include "meshgen/NavMeshTool.h"

#include <cstdint>

// Tool to create off-mesh connection for InputGeom

class OffMeshConnectionTool : public Tool
{
public:
	OffMeshConnectionTool();
	~OffMeshConnectionTool();

	virtual ToolType type() const override { return ToolType::OFFMESH_CONNECTION; }
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
