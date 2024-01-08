//
// OffMeshConnectionTool.h
//

#pragma once

#include "common/NavMeshData.h"
#include "meshgen/NavMeshTool.h"

#include <cstdint>

// Tool to create and edit connections

class OffMeshConnectionToolState;
struct OffMeshConnection;

class OffMeshConnectionTool : public Tool
{
public:
	OffMeshConnectionTool();
	~OffMeshConnectionTool() override;

	virtual ToolType type() const override { return ToolType::OFFMESH_CONNECTION; }
	virtual void init(NavMeshTool* meshTool) override;
	virtual void reset() override;
	virtual void handleMenu() override;
	virtual void handleClick(const glm::vec3& p, bool shift) override;
	virtual void handleUpdate(float dt) override;
	virtual void handleRender() override;
	virtual void handleRenderOverlay() override;

private:
	NavMeshTool* m_meshTool = nullptr;
	OffMeshConnectionToolState* m_state = nullptr;

	bool m_editing = false;
};

class OffMeshConnectionToolState : public ToolState
{
	friend class OffMeshConnectionTool;

public:
	virtual void init(NavMeshTool* meshTool) override;
	virtual void reset() override;
	virtual void handleRender() override;
	virtual void handleRenderOverlay() override;
	virtual void handleUpdate(const float dt) override {}

	std::vector<dtTileRef> handleConnectionClick(const glm::vec3& p, bool shift);
	std::vector<dtTileRef> UpdateConnection(OffMeshConnection* conn);

private:
	NavMeshTool* m_meshTool = nullptr;

	// properties for constructing new connection
	bool m_hitPosSet = false;
	glm::vec3 m_hitPos;

	// editing existing connection (or staging for new). We copy so we can make edits without committing.
	OffMeshConnection m_editConnection;
	uint32_t m_currentConnectionId = 0;
	bool m_modified = false;
};
