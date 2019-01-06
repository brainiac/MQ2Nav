//
// NavMeshTesterTool.cpp
//

#include "meshgen/NavMeshTesterTool.h"
#include "common/NavMeshData.h"

#include <Recast.h>
#include <RecastDebugDraw.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourDebugDraw.h>
#include <DetourCommon.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/custom/imgui_user.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp>

// Returns a random number [0..1)
static float frand()
{
	//	return ((float)(rand() & 0xffff)/(float)0xffff);
	return (float)rand() / (float)RAND_MAX;
}

inline bool inRange(const glm::vec3& v1, const glm::vec3& v2, float r, float h)
{
	const float dx = v2[0] - v1[0];
	const float dy = v2[1] - v1[1];
	const float dz = v2[2] - v1[2];
	return (dx*dx + dz * dz) < r*r && glm::abs(dy) < h;
}

static int fixupCorridor(dtPolyRef* path, const int npath, const int maxPath,
	const dtPolyRef* visited, const int nvisited)
{
	int furthestPath = -1;
	int furthestVisited = -1;

	// Find furthest common polygon.
	for (int i = npath - 1; i >= 0; --i)
	{
		bool found = false;
		for (int j = nvisited - 1; j >= 0; --j)
		{
			if (path[i] == visited[j])
			{
				furthestPath = i;
				furthestVisited = j;
				found = true;
			}
		}
		if (found)
			break;
	}

	// If no intersection found just return current path.
	if (furthestPath == -1 || furthestVisited == -1)
		return npath;

	// Concatenate paths.

	// Adjust beginning of the buffer to include the visited.
	const int req = nvisited - furthestVisited;
	const int orig = rcMin(furthestPath + 1, npath);
	int size = rcMax(0, npath - orig);
	if (req + size > maxPath)
		size = maxPath - req;
	if (size)
		memmove(path + req, path + orig, size * sizeof(dtPolyRef));

	// Store visited
	for (int i = 0; i < req; ++i)
		path[i] = visited[(nvisited - 1) - i];

	return req + size;
}

// This function checks if the path has a small U-turn, that is,
// a polygon further in the path is adjacent to the first polygon
// in the path. If that happens, a shortcut is taken.
// This can happen if the target (T) location is at tile boundary,
// and we're (S) approaching it parallel to the tile edge.
// The choice at the vertex can be arbitrary,
//  +---+---+
//  |:::|:::|
//  +-S-+-T-+
//  |:::|   | <-- the step can end up in here, resulting U-turn path.
//  +---+---+
static int fixupShortcuts(dtPolyRef* path, int npath, dtNavMeshQuery* navQuery)
{
	if (npath < 3)
		return npath;

	// Get connected polygons
	static const int maxNeis = 16;
	dtPolyRef neis[maxNeis];
	int nneis = 0;

	const dtMeshTile* tile = 0;
	const dtPoly* poly = 0;
	if (dtStatusFailed(navQuery->getAttachedNavMesh()->getTileAndPolyByRef(path[0], &tile, &poly)))
		return npath;

	for (unsigned int k = poly->firstLink; k != DT_NULL_LINK; k = tile->links[k].next)
	{
		const dtLink* link = &tile->links[k];
		if (link->ref != 0)
		{
			if (nneis < maxNeis)
				neis[nneis++] = link->ref;
		}
	}

	// If any of the neighbour polygons is within the next few polygons
	// in the path, short cut to that polygon directly.
	static const int maxLookAhead = 6;
	int cut = 0;
	for (int i = dtMin(maxLookAhead, npath) - 1; i > 1 && cut == 0; i--) {
		for (int j = 0; j < nneis; j++)
		{
			if (path[i] == neis[j]) {
				cut = i;
				break;
			}
		}
	}
	if (cut > 1)
	{
		int offset = cut - 1;
		npath -= offset;
		for (int i = 1; i < npath; i++)
			path[i] = path[i + offset];
	}

	return npath;
}

static bool getSteerTarget(dtNavMeshQuery* navQuery, const glm::vec3& startPos,
	const glm::vec3& endPos, float minTargetDist, const dtPolyRef* path, int pathSize,
	glm::vec3& steerPos, uint8_t& steerPosFlag, dtPolyRef& steerPosRef,
	std::vector<glm::vec3>* outPoints = nullptr)
{
	// Find steer target.
	static const int MAX_STEER_POINTS = 64;
	glm::vec3 steerPath[MAX_STEER_POINTS];
	uint8_t steerPathFlags[MAX_STEER_POINTS];
	dtPolyRef steerPathPolys[MAX_STEER_POINTS];
	int nsteerPath = 0;

	navQuery->findStraightPath(glm::value_ptr(startPos), glm::value_ptr(endPos),
		path, pathSize, glm::value_ptr(steerPath[0]), steerPathFlags, steerPathPolys,
		&nsteerPath, MAX_STEER_POINTS, DT_STRAIGHTPATH_AREA_CROSSINGS);

	if (!nsteerPath)
		return false;

	if (outPoints)
	{
		outPoints->clear();

		for (int i = 0; i < nsteerPath; ++i)
			outPoints->push_back(steerPath[i]);
	}

	// Find vertex far enough to steer to.
	int ns = 0;
	while (ns < nsteerPath)
	{
		// Stop at Off-Mesh link or when point is further than slop away.
		if ((steerPathFlags[ns] & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
			|| !inRange(steerPath[ns], startPos, minTargetDist, 1000.0f))
		{
			break;
		}
		ns++;
	}

	// Failed to find good point to steer to.
	if (ns >= nsteerPath)
		return false;

	steerPos = steerPath[ns];
	steerPos.y = startPos.y;
	steerPosFlag = steerPathFlags[ns];
	steerPosRef = steerPathPolys[ns];

	return true;
}


NavMeshTesterTool::NavMeshTesterTool()
{
	m_filter.setIncludeFlags(+PolyFlags::All);
	m_filter.setExcludeFlags(+PolyFlags::Disabled);
}

NavMeshTesterTool::~NavMeshTesterTool()
{
}

void NavMeshTesterTool::init(NavMeshTool* meshTool)
{
	m_meshTool = meshTool;

	auto navMesh = meshTool->GetNavMesh();
	m_navMesh = navMesh->GetNavMesh().get();
	m_navQuery = navMesh->GetNavMeshQuery().get();
	recalc();

	if (m_navQuery)
	{
		// update costs
		navMesh->FillFilterAreaCosts(m_filter);
	}

	m_neighbourhoodRadius = navMesh->GetNavMeshConfig().agentRadius * 20.0f;
	m_randomRadius = navMesh->GetNavMeshConfig().agentRadius * 30.0f;
}

void NavMeshTesterTool::handleMenu()
{
	glm::vec3 start = m_spos.zxy;
	if (ImGui::InputFloat3("Start loc", glm::value_ptr(start)))
		m_spos = start.yzx;
	glm::vec3 end = m_epos.zxy;
	if (ImGui::InputFloat3("End loc", glm::value_ptr(end)))
		m_epos = end.yzx;


	if (ImGui::RadioButton("Pathfind Follow", m_toolMode == ToolMode::PATHFIND_FOLLOW))
	{
		m_toolMode = ToolMode::PATHFIND_FOLLOW;
		recalc();
	}

	if (ImGui::RadioButton("Pathfind Straight", m_toolMode == ToolMode::PATHFIND_STRAIGHT))
	{
		m_toolMode = ToolMode::PATHFIND_STRAIGHT;
		recalc();
	}
	if (m_toolMode == ToolMode::PATHFIND_STRAIGHT)
	{
		ImGui::Indent();

		ImGui::Text("Vertices at crossings");
		if (ImGui::RadioButton("None", m_straightPathOptions == 0))
		{
			m_straightPathOptions = 0;
			recalc();
		}
		if (ImGui::RadioButton("Area", m_straightPathOptions == DT_STRAIGHTPATH_AREA_CROSSINGS))
		{
			m_straightPathOptions = DT_STRAIGHTPATH_AREA_CROSSINGS;
			recalc();
		}
		if (ImGui::RadioButton("All", m_straightPathOptions == DT_STRAIGHTPATH_ALL_CROSSINGS))
		{
			m_straightPathOptions = DT_STRAIGHTPATH_ALL_CROSSINGS;
			recalc();
		}

		ImGui::Unindent();
	}
	if (ImGui::RadioButton("Pathfind Sliced", m_toolMode == ToolMode::PATHFIND_SLICED))
	{
		m_toolMode = ToolMode::PATHFIND_SLICED;
		recalc();
	}

	ImGui::Separator();

	if (ImGui::RadioButton("Distance to Wall", m_toolMode == ToolMode::DISTANCE_TO_WALL))
	{
		m_toolMode = ToolMode::DISTANCE_TO_WALL;
		recalc();
	}

	ImGui::Separator();

	if (ImGui::RadioButton("Raycast", m_toolMode == ToolMode::RAYCAST))
	{
		m_toolMode = ToolMode::RAYCAST;
		recalc();
	}

	ImGui::Separator();

	if (ImGui::RadioButton("Find Polys in Circle", m_toolMode == ToolMode::FIND_POLYS_IN_CIRCLE))
	{
		m_toolMode = ToolMode::FIND_POLYS_IN_CIRCLE;
		recalc();
	}
	if (ImGui::RadioButton("Find Polys in Shape", m_toolMode == ToolMode::FIND_POLYS_IN_SHAPE))
	{
		m_toolMode = ToolMode::FIND_POLYS_IN_SHAPE;
		recalc();
	}

	ImGui::Separator();

	if (ImGui::RadioButton("Find Local Neighbourhood", m_toolMode == ToolMode::FIND_LOCAL_NEIGHBOURHOOD))
	{
		m_toolMode = ToolMode::FIND_LOCAL_NEIGHBOURHOOD;
		recalc();
	}

	ImGui::Separator();

	if (ImGui::Button("Set Random Start"))
	{
		dtStatus status = m_navQuery->findRandomPoint(&m_filter, frand, &m_startRef, glm::value_ptr(m_spos));
		if (dtStatusSucceed(status))
		{
			m_sposSet = true;
			recalc();
		}
	}

	if (ImGui::ButtonEx("Set Random End", ImVec2(0, 0), !m_sposSet ? ImGuiButtonFlags_Disabled : 0))
	{
		if (m_sposSet)
		{
			dtStatus status = m_navQuery->findRandomPointAroundCircle(m_startRef,
				glm::value_ptr(m_spos), m_randomRadius, &m_filter, frand, &m_endRef, glm::value_ptr(m_epos));
			if (dtStatusSucceed(status))
			{
				m_eposSet = true;
				recalc();
			}
		}
	}

	ImGui::Separator();

	if (ImGui::Button("Make Random Points"))
	{
		m_randPointsInCircle = false;
		m_nrandPoints = 0;

		for (int i = 0; i < MAX_RAND_POINTS; i++)
		{
			glm::vec3 pt;
			dtPolyRef ref;
			dtStatus status = m_navQuery->findRandomPoint(&m_filter, frand, &ref, glm::value_ptr(pt));
			if (dtStatusSucceed(status))
			{
				m_randPoints[m_nrandPoints++] = pt;
			}
		}
	}
	if (ImGui::ButtonEx("Make Random Points Around", ImVec2(0, 0), !m_sposSet ? ImGuiButtonFlags_Disabled : 0))
	{
		if (m_sposSet)
		{
			m_nrandPoints = 0;
			m_randPointsInCircle = true;

			for (int i = 0; i < MAX_RAND_POINTS; i++)
			{
				glm::vec3 pt;
				dtPolyRef ref;
				dtStatus status = m_navQuery->findRandomPointAroundCircle(m_startRef,
					glm::value_ptr(m_spos), m_randomRadius, &m_filter, frand, &ref, glm::value_ptr(pt));
				if (dtStatusSucceed(status))
				{
					m_randPoints[m_nrandPoints++] = pt;
				}
			}
		}
	}

	uint32_t includeFlags = m_filter.getIncludeFlags();
	uint32_t excludeFlags = m_filter.getExcludeFlags();
	bool changed = false;

	ImGui::Separator();
	ImGui::Text("Include Flags");

	ImGui::Indent();
	changed |= ImGui::CheckboxFlags("Walk", &includeFlags, +PolyFlags::Walk);
	changed |= ImGui::CheckboxFlags("Swim", &includeFlags, +PolyFlags::Swim);
	//changed |= ImGui::CheckboxFlags("Door", &includeFlags, +PolyFlags::Door);
	changed |= ImGui::CheckboxFlags("Jump", &includeFlags, +PolyFlags::Jump);
	ImGui::Unindent();

	ImGui::Separator();
	ImGui::Text("Exclude Flags");

	ImGui::Indent();
	ImGui::PushID("Excludes");
	changed |= ImGui::CheckboxFlags("Walk", &excludeFlags, +PolyFlags::Walk);
	changed |= ImGui::CheckboxFlags("Swim", &excludeFlags, +PolyFlags::Swim);
	//changed |= ImGui::CheckboxFlags("Door", &excludeFlags, +PolyFlags::Door);
	changed |= ImGui::CheckboxFlags("Jump", &excludeFlags, +PolyFlags::Jump);
	ImGui::PopID();
	ImGui::Unindent();

	ImGui::Separator();

	if (changed)
	{
		m_filter.setIncludeFlags(includeFlags);
		m_filter.setExcludeFlags(excludeFlags);
		recalc();
	}
}

void NavMeshTesterTool::handleClick(const glm::vec3& s, const glm::vec3& p, bool shift)
{
	if (shift)
	{
		m_sposSet = true;
		m_spos = p;
	}
	else
	{
		m_eposSet = true;
		m_epos = p;
	}

	recalc();
}

void NavMeshTesterTool::handleUpdate(float /*dt*/)
{
	if (m_toolMode == ToolMode::PATHFIND_SLICED)
	{
		if (dtStatusInProgress(m_pathFindStatus))
		{
			m_pathFindStatus = m_navQuery->updateSlicedFindPath(1, 0);
		}
		if (dtStatusSucceed(m_pathFindStatus))
		{
			m_navQuery->finalizeSlicedFindPath(m_polys, &m_npolys, MAX_POLYS);
			m_nstraightPath = 0;

			if (m_npolys)
			{
				// In case of partial path, make sure the end point is clamped to the last polygon.
				glm::vec3 epos = m_epos;

				if (m_polys[m_npolys - 1] != m_endRef)
				{
					m_navQuery->closestPointOnPoly(m_polys[m_npolys - 1],
						glm::value_ptr(m_epos), glm::value_ptr(epos), 0);
				}

				m_navQuery->findStraightPath(glm::value_ptr(m_spos), glm::value_ptr(epos),
					m_polys, m_npolys, glm::value_ptr(m_straightPath[0]),
					m_straightPathFlags, m_straightPathPolys,
					&m_nstraightPath, MAX_POLYS, DT_STRAIGHTPATH_ALL_CROSSINGS);
			}

			m_pathFindStatus = DT_FAILURE;
		}
	}
}

void NavMeshTesterTool::reset()
{
	m_startRef = 0;
	m_endRef = 0;
	m_npolys = 0;
	m_nstraightPath = 0;
	m_nsmoothPath = 0;
	m_hitPos = glm::vec3();
	m_hitNormal = glm::vec3();
	m_distanceToWall = 0;
}

void NavMeshTesterTool::recalc()
{
	if (!m_navMesh)
		return;

	if (m_sposSet)
	{
		m_navQuery->findNearestPoly(glm::value_ptr(m_spos), glm::value_ptr(m_polyPickExt),
			&m_filter, &m_startRef, 0);
	}
	else
		m_startRef = 0;

	if (m_eposSet)
	{
		m_navQuery->findNearestPoly(glm::value_ptr(m_epos), glm::value_ptr(m_polyPickExt),
			&m_filter, &m_endRef, 0);
	}
	else
		m_endRef = 0;

	m_pathFindStatus = DT_FAILURE;

	if (m_toolMode == ToolMode::PATHFIND_FOLLOW)
	{
		m_pathIterNum = 0;
		if (m_sposSet && m_eposSet && m_startRef && m_endRef)
		{
			m_navQuery->findPath(m_startRef, m_endRef, glm::value_ptr(m_spos), glm::value_ptr(m_epos),
				&m_filter, m_polys, &m_npolys, MAX_POLYS);

			m_nsmoothPath = 0;

			if (m_npolys)
			{
				// Iterate over the path to find smooth path on the detail mesh surface.
				dtPolyRef polys[MAX_POLYS];
				memcpy(polys, m_polys, sizeof(dtPolyRef) * m_npolys);
				int npolys = m_npolys;

				glm::vec3 iterPos, targetPos;
				m_navQuery->closestPointOnPoly(m_startRef, glm::value_ptr(m_spos), glm::value_ptr(iterPos), 0);
				m_navQuery->closestPointOnPoly(polys[npolys - 1], glm::value_ptr(m_epos), glm::value_ptr(targetPos), 0);

				static const float STEP_SIZE = 0.5f;
				static const float SLOP = 0.01f;

				m_nsmoothPath = 0;
				m_smoothPath[m_nsmoothPath++] = iterPos;

				// Move towards target a small advancement at a time until target reached or
				// when ran out of memory to store the path.
				while (npolys && m_nsmoothPath < MAX_SMOOTH)
				{
					// Find location to steer towards.
					glm::vec3 steerPos;
					uint8_t steerPosFlag = 0;
					dtPolyRef steerPosRef;

					if (!getSteerTarget(m_navQuery, iterPos, targetPos, SLOP,
						polys, npolys, steerPos, steerPosFlag, steerPosRef,
						&m_steerPoints))
					{
						break;
					}

					bool endOfPath = (steerPosFlag & DT_STRAIGHTPATH_END) != 0;
					bool offMeshConnection = (steerPosFlag & DT_STRAIGHTPATH_OFFMESH_CONNECTION) != 0;

					// Find movement delta.
					glm::vec3 delta = steerPos - iterPos;
					float len = glm::sqrt(glm::dot(delta, delta));

					// If the steer target is end of path or off-mesh link, do not move past the location.
					if ((endOfPath || offMeshConnection) && len < STEP_SIZE)
						len = 1;
					else
						len = STEP_SIZE / len;

					glm::vec3 moveTgt = iterPos + (delta * len);

					// Move
					glm::vec3 result;
					dtPolyRef visited[16];
					int nvisited = 0;
					m_navQuery->moveAlongSurface(polys[0], glm::value_ptr(iterPos), glm::value_ptr(moveTgt),
						&m_filter, glm::value_ptr(result), visited, &nvisited, 16);

					npolys = fixupCorridor(polys, npolys, MAX_POLYS, visited, nvisited);
					npolys = fixupShortcuts(polys, npolys, m_navQuery);

					float h = 0;
					m_navQuery->getPolyHeight(polys[0], glm::value_ptr(result), &h);
					result.y = h;

					iterPos = result;

					// Handle end of path and off-mesh links when close enough.
					if (endOfPath && inRange(iterPos, steerPos, SLOP, 1.0f))
					{
						// Reached end of path.
						iterPos = targetPos;
						if (m_nsmoothPath < MAX_SMOOTH)
						{
							m_smoothPath[m_nsmoothPath++] = iterPos;
						}
						break;
					}
					else if (offMeshConnection && inRange(iterPos, steerPos, SLOP, 1.0f))
					{
						// Reached off-mesh connection.
						glm::vec3 startPos, endPos;

						// Advance the path up to and over the off-mesh connection.
						dtPolyRef prevRef = 0, polyRef = polys[0];
						int npos = 0;
						while (npos < npolys && polyRef != steerPosRef)
						{
							prevRef = polyRef;
							polyRef = polys[npos];
							npos++;
						}
						for (int i = npos; i < npolys; ++i)
							polys[i - npos] = polys[i];
						npolys -= npos;

						// Handle the connection.
						dtStatus status = m_navMesh->getOffMeshConnectionPolyEndPoints(prevRef, polyRef,
							glm::value_ptr(startPos), glm::value_ptr(endPos));
						if (dtStatusSucceed(status))
						{
							if (m_nsmoothPath < MAX_SMOOTH)
							{
								m_smoothPath[m_nsmoothPath++] = startPos;

								// Hack to make the dotted path not visible during off-mesh connection.
								if (m_nsmoothPath & 1)
								{
									m_smoothPath[m_nsmoothPath++] = startPos;
								}
							}

							// Move position at the other side of the off-mesh link.
							iterPos = endPos;

							float eh = 0.0f;
							m_navQuery->getPolyHeight(polys[0], glm::value_ptr(iterPos), &eh);
							iterPos.y = eh;
						}
					}

					// Store results.
					if (m_nsmoothPath < MAX_SMOOTH)
					{
						m_smoothPath[m_nsmoothPath++] = iterPos;
					}
				}
			}
		}
		else
		{
			m_npolys = 0;
			m_nsmoothPath = 0;
		}
	}
	else if (m_toolMode == ToolMode::PATHFIND_STRAIGHT)
	{
		if (m_sposSet && m_eposSet && m_startRef && m_endRef)
		{
			m_navQuery->findPath(m_startRef, m_endRef, glm::value_ptr(m_spos),
				glm::value_ptr(m_epos), &m_filter, m_polys, &m_npolys, MAX_POLYS);
			m_nstraightPath = 0;
			if (m_npolys)
			{
				// In case of partial path, make sure the end point is clamped to the last polygon.
				glm::vec3 epos = m_epos;

				if (m_polys[m_npolys - 1] != m_endRef)
					m_navQuery->closestPointOnPoly(m_polys[m_npolys - 1], glm::value_ptr(m_epos), glm::value_ptr(epos), 0);

				m_navQuery->findStraightPath(glm::value_ptr(m_spos), glm::value_ptr(epos), m_polys, m_npolys,
					glm::value_ptr(m_straightPath[0]), m_straightPathFlags, m_straightPathPolys, &m_nstraightPath,
					MAX_POLYS, m_straightPathOptions);
			}
		}
		else
		{
			m_npolys = 0;
			m_nstraightPath = 0;
		}
	}
	else if (m_toolMode == ToolMode::PATHFIND_SLICED)
	{
		if (m_sposSet && m_eposSet && m_startRef && m_endRef)
		{
			m_npolys = 0;
			m_nstraightPath = 0;

			m_pathFindStatus = m_navQuery->initSlicedFindPath(m_startRef, m_endRef, glm::value_ptr(m_spos),
				glm::value_ptr(m_epos), &m_filter, DT_FINDPATH_ANY_ANGLE);
		}
		else
		{
			m_npolys = 0;
			m_nstraightPath = 0;
		}
	}
	else if (m_toolMode == ToolMode::RAYCAST)
	{
		m_nstraightPath = 0;
		if (m_sposSet && m_eposSet && m_startRef)
		{
			float t = 0;
			m_npolys = 0;
			m_nstraightPath = 2;
			m_straightPath[0] = m_spos;

			m_navQuery->raycast(m_startRef, glm::value_ptr(m_spos), glm::value_ptr(m_epos),
				&m_filter, &t, glm::value_ptr(m_hitNormal), m_polys, &m_npolys, MAX_POLYS);

			if (t > 1)
			{
				// No hit
				m_hitPos = m_epos;
				m_hitResult = false;
			}
			else
			{
				// Hit
				m_hitPos = glm::lerp(m_spos, m_epos, t);
				m_hitResult = true;
			}
			// Adjust height.
			if (m_npolys > 0)
			{
				float h = 0;
				m_navQuery->getPolyHeight(m_polys[m_npolys - 1], glm::value_ptr(m_hitPos), &h);
				m_hitPos[1] = h;
			}

			m_straightPath[1] = m_hitPos;
		}
	}
	else if (m_toolMode == ToolMode::DISTANCE_TO_WALL)
	{
		m_distanceToWall = 0;
		if (m_sposSet && m_startRef)
		{
			m_distanceToWall = 0.0f;
			m_navQuery->findDistanceToWall(m_startRef, glm::value_ptr(m_spos), 100.0f, &m_filter,
				&m_distanceToWall, glm::value_ptr(m_hitPos), glm::value_ptr(m_hitNormal));
		}
	}
	else if (m_toolMode == ToolMode::FIND_POLYS_IN_CIRCLE)
	{
		if (m_sposSet && m_startRef && m_eposSet)
		{
			const float dx = m_epos.x - m_spos.x;
			const float dz = m_epos.z - m_spos.z;
			float dist = glm::sqrt(dx * dx + dz * dz);
			m_navQuery->findPolysAroundCircle(m_startRef, glm::value_ptr(m_spos), dist, &m_filter,
				m_polys, m_parent, 0, &m_npolys, MAX_POLYS);

		}
	}
	else if (m_toolMode == ToolMode::FIND_POLYS_IN_SHAPE)
	{
		if (m_sposSet && m_startRef && m_eposSet)
		{
			const float nx = (m_epos.z - m_spos.z) * 0.25f;
			const float nz = -(m_epos.x - m_spos.x) * 0.25f;
			const float agentHeight = m_meshTool ? m_meshTool->GetNavMesh()->GetNavMeshConfig().agentHeight : 0;

			m_queryPoly[0].x = m_spos.x + nx * 1.2f;
			m_queryPoly[0].y = m_spos.y + agentHeight / 2;
			m_queryPoly[0].z = m_spos.z + nz * 1.2f;

			m_queryPoly[1].x = m_spos.x - nx * 1.3f;
			m_queryPoly[1].y = m_spos.y + agentHeight / 2;
			m_queryPoly[1].z = m_spos.z - nz * 1.3f;

			m_queryPoly[2].x = m_epos.x - nx * 0.8f;
			m_queryPoly[2].y = m_epos.y + agentHeight / 2;
			m_queryPoly[2].z = m_epos.z - nz * 0.8f;

			m_queryPoly[3].x = m_epos.x + nx;
			m_queryPoly[3].y = m_epos.y + agentHeight / 2;
			m_queryPoly[3].z = m_epos.z + nz;

			m_navQuery->findPolysAroundShape(m_startRef, glm::value_ptr(m_queryPoly[0]), 4, &m_filter,
				m_polys, m_parent, 0, &m_npolys, MAX_POLYS);
		}
	}
	else if (m_toolMode == ToolMode::FIND_LOCAL_NEIGHBOURHOOD)
	{
		if (m_sposSet && m_startRef)
		{
			m_navQuery->findLocalNeighbourhood(m_startRef, glm::value_ptr(m_spos),
				m_neighbourhoodRadius, &m_filter, m_polys, m_parent, &m_npolys, MAX_POLYS);
		}
	}
}

static void getPolyCenter(dtNavMesh* navMesh, dtPolyRef ref, glm::vec3& center)
{
	center = glm::vec3();

	const dtMeshTile* tile = 0;
	const dtPoly* poly = 0;
	dtStatus status = navMesh->getTileAndPolyByRef(ref, &tile, &poly);
	if (dtStatusFailed(status))
		return;

	for (int i = 0; i < (int)poly->vertCount; ++i)
	{
		const float* v = &tile->verts[poly->verts[i] * 3];
		center += glm::vec3{ v[0], v[1], v[2] };
	}

	const float s = 1.0f / poly->vertCount;
	center *= s;
}

void NavMeshTesterTool::handleRender()
{
	DebugDrawGL dd;

	static const unsigned int startCol = duRGBA(128, 25, 0, 192);
	static const unsigned int endCol = duRGBA(51, 102, 0, 129);
	static const unsigned int pathCol = duRGBA(0, 0, 0, 64);

	const auto& config = m_meshTool->GetNavMesh()->GetNavMeshConfig();
	const float agentRadius = config.agentRadius;
	const float agentHeight = config.agentHeight;
	const float agentClimb = config.agentMaxClimb;

	dd.depthMask(false);
	if (m_sposSet)
		drawAgent(m_spos, agentRadius, agentHeight, agentClimb, startCol);
	if (m_eposSet)
		drawAgent(m_epos, agentRadius, agentHeight, agentClimb, endCol);
	dd.depthMask(true);

	if (!m_navMesh)
	{
		return;
	}

	if (m_toolMode == ToolMode::PATHFIND_FOLLOW)
	{
		duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_startRef, startCol);
		duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_endRef, endCol);

		if (m_npolys)
		{
			for (int i = 0; i < m_npolys; ++i)
			{
				if (m_polys[i] == m_startRef || m_polys[i] == m_endRef)
					continue;
				duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_polys[i], pathCol);
			}
		}

		if (m_nsmoothPath)
		{
			dd.depthMask(false);
			const unsigned int spathCol = duRGBA(0, 0, 0, 220);
			dd.begin(DU_DRAW_LINES, 3.0f);
			for (int i = 0; i < m_nsmoothPath; ++i)
				dd.vertex(m_smoothPath[i].x, m_smoothPath[i].y + 0.1f, m_smoothPath[i].z, spathCol);
			dd.end();
			dd.depthMask(true);
		}

		if (m_pathIterNum)
		{
			duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_pathIterPolys[0], duRGBA(255, 255, 255, 128));

			dd.depthMask(false);
			dd.begin(DU_DRAW_LINES, 1.0f);

			const unsigned int prevCol = duRGBA(255, 192, 0, 220);
			const unsigned int curCol = duRGBA(255, 255, 255, 220);
			const unsigned int steerCol = duRGBA(0, 192, 255, 220);

			dd.vertex(m_prevIterPos[0], m_prevIterPos[1] - 0.3f, m_prevIterPos[2], prevCol);
			dd.vertex(m_prevIterPos[0], m_prevIterPos[1] + 0.3f, m_prevIterPos[2], prevCol);

			dd.vertex(m_iterPos[0], m_iterPos[1] - 0.3f, m_iterPos[2], curCol);
			dd.vertex(m_iterPos[0], m_iterPos[1] + 0.3f, m_iterPos[2], curCol);

			dd.vertex(m_prevIterPos[0], m_prevIterPos[1] + 0.3f, m_prevIterPos[2], prevCol);
			dd.vertex(m_iterPos[0], m_iterPos[1] + 0.3f, m_iterPos[2], prevCol);

			dd.vertex(m_prevIterPos[0], m_prevIterPos[1] + 0.3f, m_prevIterPos[2], steerCol);
			dd.vertex(m_steerPos[0], m_steerPos[1] + 0.3f, m_steerPos[2], steerCol);

			for (size_t i = 0; i < m_steerPoints.size() - 1; ++i)
			{
				dd.vertex(m_steerPoints[i].x, m_steerPoints[i].y + 0.2f, m_steerPoints[i].z, duDarkenCol(steerCol));
				dd.vertex(m_steerPoints[i + 1].x, m_steerPoints[i + 1].y + 0.2f, m_steerPoints[i + 1].z, duDarkenCol(steerCol));
			}

			dd.end();
			dd.depthMask(true);
		}
	}
	else if (m_toolMode == ToolMode::PATHFIND_STRAIGHT || m_toolMode == ToolMode::PATHFIND_SLICED)
	{
		duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_startRef, startCol);
		duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_endRef, endCol);

		if (m_npolys)
		{
			for (int i = 0; i < m_npolys; ++i)
			{
				if (m_polys[i] == m_startRef || m_polys[i] == m_endRef)
					continue;
				duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_polys[i], pathCol);
			}
		}

		if (m_nstraightPath)
		{
			dd.depthMask(false);
			const unsigned int spathCol = duRGBA(64, 16, 0, 220);
			const unsigned int offMeshCol = duRGBA(128, 96, 0, 220);
			dd.begin(DU_DRAW_LINES, 2.0f);
			for (int i = 0; i < m_nstraightPath - 1; ++i)
			{
				unsigned int col;
				if (m_straightPathFlags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
					col = offMeshCol;
				else
					col = spathCol;

				dd.vertex(m_straightPath[i].x, m_straightPath[i].y + 0.4f, m_straightPath[i].z, col);
				dd.vertex(m_straightPath[i + 1].x, m_straightPath[i + 1].y + 0.4f, m_straightPath[i + 1].z, col);
			}
			dd.end();
			dd.begin(DU_DRAW_POINTS, 6.0f);
			for (int i = 0; i < m_nstraightPath; ++i)
			{
				unsigned int col;
				if (m_straightPathFlags[i] & DT_STRAIGHTPATH_START)
					col = startCol;
				else if (m_straightPathFlags[i] & DT_STRAIGHTPATH_END)
					col = endCol;
				else if (m_straightPathFlags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
					col = offMeshCol;
				else
					col = spathCol;
				dd.vertex(m_straightPath[i].x, m_straightPath[i].y + 0.4f, m_straightPath[i].z, col);
			}
			dd.end();
			dd.depthMask(true);
		}
	}
	else if (m_toolMode == ToolMode::RAYCAST)
	{
		duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_startRef, startCol);

		if (m_nstraightPath)
		{
			for (int i = 1; i < m_npolys; ++i)
				duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_polys[i], pathCol);

			dd.depthMask(false);
			const unsigned int spathCol = m_hitResult ? duRGBA(64, 16, 0, 220) : duRGBA(240, 240, 240, 220);
			dd.begin(DU_DRAW_LINES, 2.0f);
			for (int i = 0; i < m_nstraightPath - 1; ++i)
			{
				dd.vertex(m_straightPath[i].x, m_straightPath[i].y + 0.4f, m_straightPath[i].z, spathCol);
				dd.vertex(m_straightPath[i + 1].x, m_straightPath[i + 1].y + 0.4f, m_straightPath[i + 1].z, spathCol);
			}
			dd.end();
			dd.begin(DU_DRAW_POINTS, 4.0f);
			for (int i = 0; i < m_nstraightPath; ++i)
				dd.vertex(m_straightPath[i].x, m_straightPath[i].y + 0.4f, m_straightPath[i].z, spathCol);
			dd.end();

			if (m_hitResult)
			{
				const unsigned int hitCol = duRGBA(0, 0, 0, 128);
				dd.begin(DU_DRAW_LINES, 2.0f);
				dd.vertex(m_hitPos.x, m_hitPos.y + 0.4f, m_hitPos.z, hitCol);
				dd.vertex(m_hitPos.x + m_hitNormal.x * agentRadius,
					m_hitPos.y + 0.4f + m_hitNormal.y * agentRadius,
					m_hitPos.z + m_hitNormal.z * agentRadius, hitCol);
				dd.end();
			}
			dd.depthMask(true);
		}
	}
	else if (m_toolMode == ToolMode::DISTANCE_TO_WALL)
	{
		duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_startRef, startCol);
		dd.depthMask(false);
		duDebugDrawCircle(&dd, m_spos.x, m_spos.y + agentHeight / 2, m_spos.z, m_distanceToWall, duRGBA(64, 16, 0, 220), 2.0f);
		dd.begin(DU_DRAW_LINES, 3.0f);
		dd.vertex(m_hitPos.x, m_hitPos.y + 0.02f, m_hitPos.z, duRGBA(0, 0, 0, 192));
		dd.vertex(m_hitPos.x, m_hitPos.y + agentHeight, m_hitPos.z, duRGBA(0, 0, 0, 192));
		dd.end();
		dd.depthMask(true);
	}
	else if (m_toolMode == ToolMode::FIND_POLYS_IN_CIRCLE)
	{
		for (int i = 0; i < m_npolys; ++i)
		{
			duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_polys[i], pathCol);
			dd.depthMask(false);
			if (m_parent[i])
			{
				glm::vec3 p0, p1;
				dd.depthMask(false);
				getPolyCenter(m_navMesh, m_parent[i], p0);
				getPolyCenter(m_navMesh, m_polys[i], p1);
				duDebugDrawArc(&dd, p0.x, p0.y, p0.z, p1.x, p1.y, p1.z, 0.25f, 0.0f, 0.4f, duRGBA(0, 0, 0, 128), 2.0f);
				dd.depthMask(true);
			}
			dd.depthMask(true);
		}

		if (m_sposSet && m_eposSet)
		{
			dd.depthMask(false);
			float dx = m_epos.x - m_spos.x;
			float dz = m_epos.z - m_spos.z;
			float dist = glm::sqrt(dx*dx + dz * dz);
			duDebugDrawCircle(&dd, m_spos.x, m_spos.y + agentHeight / 2, m_spos.z, dist, duRGBA(64, 16, 0, 220), 2.0f);
			dd.depthMask(true);
		}
	}
	else if (m_toolMode == ToolMode::FIND_POLYS_IN_SHAPE)
	{
		for (int i = 0; i < m_npolys; ++i)
		{
			duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_polys[i], pathCol);
			dd.depthMask(false);
			if (m_parent[i])
			{
				glm::vec3 p0, p1;
				dd.depthMask(false);
				getPolyCenter(m_navMesh, m_parent[i], p0);
				getPolyCenter(m_navMesh, m_polys[i], p1);
				duDebugDrawArc(&dd, p0.x, p0.y, p0.z, p1.x, p1.y, p1.z, 0.25f, 0.0f, 0.4f, duRGBA(0, 0, 0, 128), 2.0f);
				dd.depthMask(true);
			}
			dd.depthMask(true);
		}

		if (m_sposSet && m_eposSet)
		{
			dd.depthMask(false);
			const uint32_t col = duRGBA(64, 16, 0, 220);
			dd.begin(DU_DRAW_LINES, 2.0f);
			for (int i = 0, j = 3; i < 4; j = i++)
			{
				dd.vertex(glm::value_ptr(m_queryPoly[j]), col);
				dd.vertex(glm::value_ptr(m_queryPoly[i]), col);
			}
			dd.end();
			dd.depthMask(true);
		}
	}
	else if (m_toolMode == ToolMode::FIND_LOCAL_NEIGHBOURHOOD)
	{
		for (int i = 0; i < m_npolys; ++i)
		{
			duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_polys[i], pathCol);
			dd.depthMask(false);
			if (m_parent[i])
			{
				glm::vec3 p0, p1;
				dd.depthMask(false);
				getPolyCenter(m_navMesh, m_parent[i], p0);
				getPolyCenter(m_navMesh, m_polys[i], p1);
				duDebugDrawArc(&dd, p0.x, p0.y, p0.z, p1.x, p1.y, p1.z, 0.25f, 0.0f, 0.4f, duRGBA(0, 0, 0, 128), 2.0f);
				dd.depthMask(true);
			}

			static const int MAX_SEGS = DT_VERTS_PER_POLYGON * 4;
			glm::vec3 segs[MAX_SEGS][2];
			dtPolyRef refs[MAX_SEGS];
			memset(refs, 0, sizeof(dtPolyRef) * MAX_SEGS);
			int nsegs = 0;
			m_navQuery->getPolyWallSegments(m_polys[i], &m_filter, glm::value_ptr(segs[0][0]), refs, &nsegs, MAX_SEGS);
			dd.begin(DU_DRAW_LINES, 2.0f);
			for (int j = 0; j < nsegs; ++j)
			{
				glm::vec3(&s)[2] = segs[j];

				// Skip too distant segments.
				float tseg;
				float distSqr = dtDistancePtSegSqr2D(glm::value_ptr(m_spos),
					glm::value_ptr(s[0]), glm::value_ptr(s[1]), tseg);
				if (distSqr > dtSqr(m_neighbourhoodRadius))
					continue;

				glm::vec3 delta = s[1] - s[0];
				glm::vec3 norm = glm::normalize(glm::vec3{ delta.z, 0, -delta.x });

				glm::vec3 p0 = s[0] + delta * 0.5f;
				glm::vec3 p1 = p0 + norm * (agentRadius * 0.5f);

				// Skip backfacing segments.
				if (refs[j])
				{
					unsigned int col = duRGBA(255, 255, 255, 32);
					dd.vertex(s[0].x, s[0].y + agentClimb, s[0].z, col);
					dd.vertex(s[1].x, s[1].y + agentClimb, s[1].z, col);
				}
				else
				{
					unsigned int col = duRGBA(192, 32, 16, 192);
					if (dtTriArea2D(glm::value_ptr(m_spos), glm::value_ptr(s[0]), glm::value_ptr(s[1])) < 0.0f)
						col = duRGBA(96, 32, 16, 192);

					dd.vertex(p0.x, p0.y + agentClimb, p0.z, col);
					dd.vertex(p1.x, p1.y + agentClimb, p1.z, col);

					dd.vertex(s[0].x, s[0].y + agentClimb, s[0].z, col);
					dd.vertex(s[1].x, s[1].y + agentClimb, s[1].z, col);
				}
			}
			dd.end();

			dd.depthMask(true);
		}

		if (m_sposSet)
		{
			dd.depthMask(false);
			duDebugDrawCircle(&dd, m_spos.x, m_spos.y + agentHeight / 2, m_spos.z,
				m_neighbourhoodRadius, duRGBA(64, 16, 0, 220), 2.0f);
			dd.depthMask(true);
		}
	}

	if (m_nrandPoints > 0)
	{
		dd.begin(DU_DRAW_POINTS, 6.0f);
		for (int i = 0; i < m_nrandPoints; i++)
		{
			const glm::vec3& p = m_randPoints[i];
			dd.vertex(p.x, p.x + 0.1f, p.z, duRGBA(220, 32, 16, 192));
		}
		dd.end();

		if (m_randPointsInCircle && m_sposSet)
		{
			duDebugDrawCircle(&dd, m_spos.x, m_spos.y + agentHeight / 2, m_spos.z, m_randomRadius,
				duRGBA(64, 16, 0, 220), 2.0f);
		}
	}
}

void NavMeshTesterTool::handleRenderOverlay(const glm::mat4& proj,
	const glm::mat4& model, const glm::ivec4& view)
{
	// Draw start and end point labels
	if (m_sposSet)
	{
		glm::vec3 pos = glm::project(m_spos, model, proj, view);

		ImGui::RenderTextCentered((int)pos.x + 5, -((int)pos.y - 5), ImVec4(0, 0, 0, 220), "Start");
	}
	if (m_eposSet)
	{
		glm::vec3 pos = glm::project(m_epos, model, proj, view);

		ImGui::RenderTextCentered((int)pos.x + 5, -((int)pos.y - 5), ImVec4(0, 0, 0, 220), "End");
	}

	// Tool help
	ImGui::RenderTextRight(-330, -(view[3] - 40), ImVec4(255, 255, 255, 192),
		"LMB+SHIFT: Set start location  LMB: Set end location");
}

void NavMeshTesterTool::drawAgent(const glm::vec3& pos, float r, float h, float c, uint32_t col)
{
	DebugDrawGL dd;

	dd.depthMask(false);

	// Agent dimensions.
	duDebugDrawCylinderWire(&dd, pos.x - r, pos.y + 0.02f, pos.z - r, pos.x + r, pos.y + h, pos.z + r, col, 2.0f);

	duDebugDrawCircle(&dd, pos.x, pos.y + c, pos.z, r, duRGBA(0, 0, 0, 64), 1.0f);

	uint32_t colb = duRGBA(0, 0, 0, 196);
	dd.begin(DU_DRAW_LINES);
	dd.vertex(pos.x, pos.y - c, pos.z, colb);
	dd.vertex(pos.x, pos.y + c, pos.z, colb);
	dd.vertex(pos.x - r / 2, pos.y + 0.02f, pos.z, colb);
	dd.vertex(pos.x + r / 2, pos.y + 0.02f, pos.z, colb);
	dd.vertex(pos.x, pos.y + 0.02f, pos.z - r / 2, colb);
	dd.vertex(pos.x, pos.y + 0.02f, pos.z + r / 2, colb);
	dd.end();

	dd.depthMask(true);
}
