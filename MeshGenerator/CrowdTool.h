//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#ifndef CROWDTOOL_H
#define CROWDTOOL_H

#include "Sample.h"
#include "DetourNavMesh.h"
#include "DetourObstacleAvoidance.h"
#include "ValueHistory.h"
#include "DetourCrowd.h"

#include <glm/glm.hpp>

// Tool to create crowds.

struct CrowdToolParams
{
	bool m_showCorners;
	bool m_showCollisionSegments;
	bool m_showPath;
	bool m_showVO;
	bool m_showOpt;
	bool m_showNeis;
	
	bool m_showLabels;
	bool m_showGrid;
	bool m_showNodes;
	bool m_showPerfGraph;
	bool m_showDetailAll;
	
	bool m_anticipateTurns;
	bool m_optimizeVis;
	bool m_optimizeTopo;
	bool m_obstacleAvoidance;
	float m_obstacleAvoidanceType;
	bool m_separation;
	float m_separationWeight;
};

class CrowdToolState : public SampleToolState
{
	Sample* m_sample;
	dtNavMesh* m_nav;
	dtCrowd* m_crowd;
	
	glm::vec3 m_targetPos;
	dtPolyRef m_targetRef;

	dtCrowdAgentDebugInfo m_agentDebug;
	dtObstacleAvoidanceDebugData* m_vod;
	
	static const int AGENT_MAX_TRAIL = 64;
	static const int MAX_AGENTS = 128;
	struct AgentTrail
	{
		float trail[AGENT_MAX_TRAIL*3];
		int htrail;
	};
	AgentTrail m_trails[MAX_AGENTS];
	
	ValueHistory m_crowdTotalTime;
	ValueHistory m_crowdSampleCount;

	CrowdToolParams m_toolParams;

	bool m_run;

public:
	CrowdToolState();
	virtual ~CrowdToolState();
	
	virtual void init(class Sample* sample) override;
	virtual void reset() override;
	virtual void handleRender() override;
	virtual void handleRenderOverlay(const glm::mat4& proj,
		const glm::mat4& model, const glm::ivec4& view) override;
	virtual void handleUpdate(const float dt) override;

	inline bool isRunning() const { return m_run; }
	inline void setRunning(const bool s) { m_run = s; }
	
	void addAgent(const float* pos);
	void removeAgent(const int idx);
	void hilightAgent(const int idx);
	void updateAgentParams();
	int hitTestAgents(const float* s, const float* p);
	void setMoveTarget(const float* p, bool adjust);
	void updateTick(const float dt);

	inline CrowdToolParams* getToolParams() { return &m_toolParams; }
};


class CrowdTool : public SampleTool
{
	Sample* m_sample;
	CrowdToolState* m_state;
	
	enum ToolMode
	{
		TOOLMODE_CREATE,
		TOOLMODE_MOVE_TARGET,
		TOOLMODE_SELECT,
		TOOLMODE_TOGGLE_POLYS,
	};
	ToolMode m_mode;
	
	void updateAgentParams();
	void updateTick(const float dt);
	
public:
	CrowdTool();
	virtual ~CrowdTool();
	
	virtual int type() override { return TOOL_CROWD; }
	virtual void init(Sample* sample) override;
	virtual void reset() override;
	virtual void handleMenu() override;
	virtual void handleClick(const float* s, const float* p, bool shift) override;
	virtual void handleToggle() override;
	virtual void handleStep() override;
	virtual void handleUpdate(const float dt) override;
	virtual void handleRender() override;
	virtual void handleRenderOverlay(const glm::mat4& proj,
		const glm::mat4& model, const glm::ivec4& view) override;
};

#endif // CROWDTOOL_H
