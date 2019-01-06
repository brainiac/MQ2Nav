//
// Navigator.cpp
//

#include "Navigator.h"

#include <glm/gtc/type_ptr.hpp>

#include <DetourCommon.h>
#include <DetourNavMesh.h>
#include <DetourPathCorridor.h>

bool withinRadiusOfOffMeshConnection(const glm::vec3& pos, const glm::vec3& connPos,
	dtPolyRef connPoly, dtNavMesh* navMesh)
{
	const dtOffMeshConnection* conn = navMesh->getOffMeshConnectionByRef(connPoly);
	if (!conn)
	{
		return false;
	}

	// not sure how to do 2d distance squared with glm..
	// check if we are within the radius of the connection.
	return dtVdist2DSqr(glm::value_ptr(pos), glm::value_ptr(connPos)) < conn->rad * conn->rad;
}

bool overOffMeshConnectionStart(const glm::vec3& pos, const NavigationBuffer& buffer, dtNavMesh* navMesh,
	dtPolyRef* polyRef)
{
	// find the closest off mesh connection in the path
	for (int i = 0; i < buffer.count; ++i)
	{
		if (buffer.flags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
		{
			*polyRef = buffer.polys[i];
			return withinRadiusOfOffMeshConnection(pos, buffer.points[i], buffer.polys[i], navMesh);
		}
	}

	return false;
}

Navigator::Navigator(NavMesh* navMesh, int maxNodes)
	: m_navMesh(navMesh)
	, m_maxNodes(maxNodes)
{
	m_query = m_navMesh->GetNavMeshQuery();
	m_filter = std::make_unique<dtQueryFilter>();

	// default extents are based on the agent radius/height. Can be overridden by user.
	auto& config = navMesh->GetNavMeshConfig();
	m_extents = { config.agentRadius, config.agentHeight, config.agentRadius };
}

Navigator::~Navigator()
{
}

bool Navigator::FindRoute(
	const glm::vec3& startPos,
	const glm::vec3& targetPos,
	bool allowPartial)
{
	dtStatus result;

	m_requestedStartPos = startPos;
	m_requestedEndPos = targetPos;
	m_allowPartialResults = allowPartial;

	// find the source position
	dtPolyRef startPoly;
	result = FindNearestPolyRef(startPos, &startPoly, &m_pos);
	if (startPoly == 0)
	{
		m_error = NavigatorError::SourceOffMesh;
		m_lastStatus = result;
		return false;
	}

	// find the target position
	dtPolyRef targetPoly;
	result = FindNearestPolyRef(targetPos, &targetPoly, &m_target);
	if (targetPoly == 0)
	{
		m_error = NavigatorError::TargetOffMesh;
		m_lastStatus = result;
		return false;
	}

	int iterations = 0;

	// determine a path of polygons that connects the start to the end.
	m_polyPathReserved = m_maxNodes;
	m_polyPathLen = 0;
	bool firstIteration = true;

	do
	{
		if (!firstIteration) m_polyPathReserved = m_polyPathReserved + m_polyPathReserved / 2;
		m_polyPath = std::make_unique<dtPolyRef[]>(m_polyPathReserved);

		// find path
		result = m_query->findPath(startPoly, targetPoly,
			glm::value_ptr(m_pos), glm::value_ptr(m_target),
			m_filter.get(), m_polyPath.get(), &m_polyPathLen, m_polyPathReserved);

		firstIteration = false;
		++iterations;

	} while (dtStatusSucceed(result) && ((result & DT_BUFFER_TOO_SMALL) != 0) && iterations < 5);

	m_lastStatus = result;

	if (dtStatusFailed(m_lastStatus))
	{
		m_error = NavigatorError::Unexpected;

		Reset();
		return false;
	}

	if (!m_allowPartialResults)
	{
		if (dtStatusDetail(result, DT_BUFFER_TOO_SMALL)
			|| dtStatusDetail(result, DT_OUT_OF_NODES)
			|| dtStatusDetail(result, DT_PARTIAL_RESULT))
		{
			m_error = NavigatorError::PathIncomplete;

			Reset();
			return false;
		}
	}

	m_needsUpdate = true;
	return true;
}

bool Navigator::MovePosition(const glm::vec3& position)
{
	assert(m_polyPathLen > 0);
	assert(m_polyPath != nullptr);

	// avoid unnecessary changes
	if (position == m_requestedStartPos)
	{
		return true;
	}

	// Move along the nav mesh and update new position
	glm::vec3 result;
	static const int MAX_VISITED = 16;
	dtPolyRef visited[MAX_VISITED];
	int nvisited = 0;

	m_lastStatus = m_query->moveAlongSurface(m_polyPath[0],
		glm::value_ptr(m_pos), glm::value_ptr(position), m_filter.get(),
		glm::value_ptr(result), visited, &nvisited, MAX_VISITED);

	if (dtStatusSucceed(m_lastStatus))
	{
		m_polyPathLen = dtMergeCorridorStartMoved(m_polyPath.get(),
			m_polyPathLen, m_polyPathReserved, visited, nvisited);

		// adjust the position to stay on top of the navmesh.
		float h = m_pos[1];
		m_query->getPolyHeight(m_polyPath[0], glm::value_ptr(result), &h);
		result[1] = h;

		m_requestedStartPos = position;
		m_pos = result;
		m_needsUpdate = true;
		return true;
	}

	return false;
}

bool Navigator::MoveTargetPosition(const glm::vec3& position)
{
	assert(m_polyPathLen > 0);
	assert(m_polyPath != nullptr);

	// avoid unnecessary changes
	if (position == m_requestedEndPos)
	{
		return true;
	}

	// Move along the nav mesh and update new position
	glm::vec3 result;
	static const int MAX_VISITED = 16;
	dtPolyRef visited[MAX_VISITED];
	int nvisited = 0;

	m_lastStatus = m_query->moveAlongSurface(m_polyPath[m_polyPathLen - 1],
		glm::value_ptr(m_target), glm::value_ptr(position), m_filter.get(),
		glm::value_ptr(result), visited, &nvisited, MAX_VISITED);

	if (dtStatusSucceed(m_lastStatus))
	{
		m_polyPathLen = dtMergeCorridorEndMoved(m_polyPath.get(),
			m_polyPathLen, m_polyPathReserved, visited, nvisited);

		// adjust the position to stay on top of the navmesh.
		float h = m_target[1];
		m_query->getPolyHeight(m_polyPath[m_polyPathLen - 1], glm::value_ptr(result), &h);
		result[1] = h;

		m_target = result;
		m_requestedEndPos = position;
		m_needsUpdate = true;
		return true;
	}

	return false;
}

void Navigator::OptimizePathVisibility(const glm::vec3& next, const float pathOptimizationRange)
{
	assert(m_polyPath != nullptr);

	// Clamp the ray to max distance
	glm::vec3 goal = next;
	float dist = dtVdist2D(glm::value_ptr(m_pos), glm::value_ptr(goal));

	// If too close to the goal, do not try to optimize.
	if (dist < 0.01f)
		return;

	// Overshoot a little. This helps to optimize open fields in tiled meshes.
	dist = dtMin(dist + 0.01f, pathOptimizationRange);

	// adjust ray length
	glm::vec3 delta = goal - m_pos;
	goal = m_pos + (delta * (pathOptimizationRange / dist));

	static const int MAX_RES = 32;
	dtPolyRef res[MAX_RES];
	float t;
	glm::vec3 norm;
	int nres = 0;

	m_query->raycast(m_polyPath[0], glm::value_ptr(m_pos), glm::value_ptr(goal),
		m_filter.get(), &t, glm::value_ptr(norm), res, &nres, MAX_RES);
	if (nres > 1 && t > 0.99f)
	{
		m_polyPathLen = dtMergeCorridorStartShortcut(m_polyPath.get(), m_polyPathLen, m_polyPathReserved,
			res, nres);
		m_needsUpdate = true;
	}
}

bool Navigator::OptimizePathTopology()
{
	assert(m_polyPath != nullptr);

	if (m_polyPathLen < 3)
		return false;

	static const int MAX_ITER = 32;
	static const int MAX_RES = 32;

	dtPolyRef res[MAX_RES];
	int nres = 0;

	m_query->initSlicedFindPath(m_polyPath[0], m_polyPath[m_polyPathLen - 1],
		glm::value_ptr(m_pos), glm::value_ptr(m_target), m_filter.get());
	m_query->updateSlicedFindPath(MAX_ITER, nullptr);

	dtStatus status = m_query->finalizeSlicedFindPathPartial(m_polyPath.get(), m_polyPathLen,
		res, &nres, MAX_RES);

	if (dtStatusSucceed(status) && nres > 0)
	{
		m_polyPathLen = dtMergeCorridorStartShortcut(m_polyPath.get(), m_polyPathLen,
			m_polyPathReserved, res, nres);
		m_needsUpdate = true;
		return true;
	}

	return false;
}

bool Navigator::FindCorners(NavigationBuffer& buffer, int maxCorners)
{
	assert(m_polyPath != nullptr);
	assert(m_polyPathLen > 0);

	static const float MIN_TARGET_DIST = 0.01f;

	if (maxCorners == -1)
		maxCorners = m_polyPathReserved;

	int ncorners = 0;
	buffer.Resize(maxCorners);

	m_lastStatus = m_query->findStraightPath(glm::value_ptr(m_pos), glm::value_ptr(m_target),
		m_polyPath.get(), m_polyPathLen, glm::value_ptr(buffer.points[0]),
		buffer.flags.get(), buffer.polys.get(), &ncorners, maxCorners);
	buffer.count = ncorners;

	if (!ncorners)
	{
		m_needsUpdate = false;
		return true;
	}

	// Prune points in the beginning of the path which are too close
	int ncount = 0;
	for (ncount = 0; ncount < ncorners; ++ncount)
	{
		// stop if its an offmesh connection
		if (buffer.flags[0] & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
			break;

		// stop if its too far away
		if (dtVdist2DSqr(glm::value_ptr(buffer.points[ncount]), glm::value_ptr(m_pos)) > dtSqr(MIN_TARGET_DIST))
			break;
	}
	ncorners -= ncount;
	if (ncount > 0 && ncorners > 0)
	{
		// move everything up
		memmove(buffer.flags.get(), buffer.flags.get() + ncount, sizeof(uint8_t) * ncorners);
		memmove(buffer.polys.get(), buffer.polys.get() + ncount, sizeof(dtPolyRef) * ncorners);
		memmove(buffer.points.get(), buffer.points.get() + ncount, sizeof(glm::vec3) * ncorners);
	}

	// Prune points after an off-mesh connection
	// TODO: Don't want to do this?

	buffer.count = ncorners;
	m_needsUpdate = false;

	return true;
}

bool Navigator::MoveOverOffMeshConnection(dtPolyRef offMeshConRef,
	glm::vec3& startPos, glm::vec3& endPos)
{
	assert(m_polyPath != nullptr);
	assert(m_polyPathLen > 0);

	// Advance the path up to and over the off-mesh connection.
	dtPolyRef prevRef = 0, polyRef = m_polyPath[0];
	int npos = 0;

	while (npos < m_polyPathLen && polyRef != offMeshConRef)
	{
		prevRef = std::exchange(polyRef, m_polyPath[npos++]);
	}

	if (npos == m_polyPathLen)
	{
		// Could not find offMeshConRef
		return false;
	}

	// Prune path
	for (int i = npos; i < m_polyPathLen; ++i)
		m_polyPath[i - npos] = m_polyPath[i];
	m_polyPathLen -= npos;

	dtStatus status = m_navMesh->GetNavMesh()->getOffMeshConnectionPolyEndPoints(prevRef, polyRef,
		glm::value_ptr(startPos), glm::value_ptr(endPos));
	if (dtStatusSucceed(status))
	{
		m_pos = endPos;
		m_needsUpdate = true;
		return true;
	}

	return false;
}

bool Navigator::ContainsPolyRef(dtPolyRef ref)
{
	for (int i = 0; i < m_polyPathLen; ++i)
	{
		if (m_polyPath[i] == ref)
			return true;
	}

	return false;
}

dtStatus Navigator::FindNearestPolyRef(
	const glm::vec3& pos,
	dtPolyRef* nearestRef,
	glm::vec3* nearestPos)
{
	if (!m_query)
	{
		return DT_FAILURE | DT_INVALID_PARAM;
	}

	return m_query->findNearestPoly(
		glm::value_ptr(pos), glm::value_ptr(m_extents),
		m_filter.get(), nearestRef, glm::value_ptr(*nearestPos));
}

void Navigator::Reset()
{
	m_polyPath.reset();
	m_polyPathLen = 0;
	m_polyPathReserved = 0;
	m_needsUpdate = false;
}
