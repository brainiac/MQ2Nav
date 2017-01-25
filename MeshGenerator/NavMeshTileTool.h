
#pragma once

#include "Sample_TileMesh.h"

class NavMeshTileTool : public Tool
{
public:
	NavMeshTileTool() {}
	virtual ~NavMeshTileTool() {}

	virtual ToolType type() const override { return ToolType::TILE_EDIT; }
	virtual void init(Sample* sample) override;
	virtual void reset() override {}
	virtual void handleMenu() override;
	virtual void handleClick(const glm::vec3& s, const glm::vec3& p, bool shift) override;
	virtual void handleToggle() override {}
	virtual void handleStep() override {}
	virtual void handleUpdate(float /*dt*/) override {}
	virtual void handleRender() override;
	virtual void handleRenderOverlay(const glm::mat4& proj,
		const glm::mat4& model, const glm::ivec4& view) override;

private:
	Sample_TileMesh* m_sample = nullptr;
	glm::vec3 m_hitPos;
	bool m_hitPosSet = false;
};
