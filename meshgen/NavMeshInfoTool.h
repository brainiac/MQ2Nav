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

	virtual void handleMenu() override;

private:
	NavMeshTool* m_meshTool = nullptr;
};
