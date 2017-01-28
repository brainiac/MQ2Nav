//
// NavMeshTesterTool.h
//

#pragma once

#include "NavMeshTool.h"

#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include <glm/glm.hpp>
#include <vector>

class NavMeshTesterTool : public Tool
{
public:
	NavMeshTesterTool();
	virtual ~NavMeshTesterTool();

	virtual ToolType type() const override { return ToolType::NAVMESH_TESTER; }
	virtual void init(NavMeshTool* meshtool) override;
	virtual void reset() override;
	virtual void handleMenu() override;
	virtual void handleClick(const glm::vec3& s, const glm::vec3& p, bool shift) override;
	virtual void handleToggle() override;
	virtual void handleStep() override;
	virtual void handleUpdate(float dt) override;
	virtual void handleRender() override;
	virtual void handleRenderOverlay(const glm::mat4& proj,
		const glm::mat4& model, const glm::ivec4& view) override;

private:
	void recalc();
	void drawAgent(const glm::vec3& pos, float r, float h, float c, uint32_t col);

private:
	NavMeshTool* m_meshTool = nullptr;
	dtNavMesh* m_navMesh = nullptr;
	dtNavMeshQuery* m_navQuery = nullptr;

	dtQueryFilter m_filter;
	dtStatus m_pathFindStatus = DT_FAILURE;

	enum struct ToolMode
	{
		PATHFIND_FOLLOW,
		PATHFIND_STRAIGHT,
		PATHFIND_SLICED,
		RAYCAST,
		DISTANCE_TO_WALL,
		FIND_POLYS_IN_CIRCLE,
		FIND_POLYS_IN_SHAPE,
		FIND_LOCAL_NEIGHBOURHOOD,
	};
	ToolMode m_toolMode = ToolMode::PATHFIND_FOLLOW;

	int m_straightPathOptions = 0;

	static const int MAX_POLYS = 4196 * 10;
	static const int MAX_SMOOTH = 2048 * 10;

	dtPolyRef m_startRef;
	dtPolyRef m_endRef;
	dtPolyRef m_polys[MAX_POLYS];
	dtPolyRef m_parent[MAX_POLYS];
	int m_npolys = 0;
	glm::vec3 m_straightPath[MAX_POLYS];
	uint8_t m_straightPathFlags[MAX_POLYS];
	dtPolyRef m_straightPathPolys[MAX_POLYS];
	int m_nstraightPath = 0;
	glm::vec3 m_polyPickExt = { 2, 4, 2 };
	glm::vec3 m_smoothPath[MAX_SMOOTH];
	int m_nsmoothPath = 0;
	glm::vec3 m_queryPoly[4];

	static const int MAX_RAND_POINTS = 64;
	glm::vec3 m_randPoints[MAX_RAND_POINTS];
	int m_nrandPoints = 0;
	bool m_randPointsInCircle = false;

	bool m_sposSet = false;
	glm::vec3 m_spos;
	bool m_eposSet = false;
	glm::vec3 m_epos;

	glm::vec3 m_hitPos;
	glm::vec3 m_hitNormal;
	bool m_hitResult = false;
	float m_distanceToWall = 0.0f;
	float m_neighbourhoodRadius = 2.5f;
	float m_randomRadius = 5.0f;

	int m_pathIterNum = 0;
	dtPolyRef m_pathIterPolys[MAX_POLYS];
	int m_pathIterPolyCount = 0;
	glm::vec3 m_prevIterPos;
	glm::vec3 m_iterPos;
	glm::vec3 m_steerPos;
	glm::vec3 m_targetPos;

	std::vector<glm::vec3> m_steerPoints;
};
