//
// Navigator.h
//

#pragma once

#include "common/NavigationRoute.h"
#include "common/NavMesh.h"
#include "common/NavMeshData.h"
#include "common/Utilities.h"

#include <glm/glm.hpp>
#include <DetourNavMeshQuery.h>
#include <memory>

class dtNavMeshQuery;
class dtNavMesh;
class dtPathCorridor;

enum struct NavigatorError
{
	None = 0,
	SourceOffMesh = 2,
	TargetOffMesh = 3,
	PathIncomplete = 4,
	Unexpected = 5,
};

// Navigator is responsible for planning a route to the given destination, continually
// updating the path as movement occurs. Based heavily on DetourPathCorridor, with modifications.

// The corridor is loaded with a path, usually obtained from a #dtNavMeshQuery::findPath() query. The
// corridor is then used to plan local movement, with the corridor automatically updating as needed to
// deal with inaccurate agent locomotion.
//
// Example of a common use case:
//
// -# Construct the corridor object and call #init() to allocate its path buffer.
// -# Obtain a path from a #dtNavMeshQuery object.
// -# Use #reset() to set the agent's current position. (At the beginning of the path.)
// -# Use #setCorridor() to load the path and target.
// -# Use #findCorners() to plan movement. (This handles dynamic path straightening.)
// -# Use #movePosition() to feed agent movement back into the corridor. (The corridor will automatically
//    adjust as needed.)
// -# If the target is moving, use #moveTargetPosition() to update the end of the corridor. 
//    (The corridor will automatically adjust as needed.)
// -# Repeat the previous 3 steps to continue to move the agent.
//
// The corridor position and target are always constrained to the navigation mesh.
//
// One of the difficulties in maintaining a path is that floating point errors, locomotion inaccuracies,
// and/or localsteering can result in the agent crossing the boundary of the path corridor, temporarily
// invalidating the path. This class uses local mesh queries to detect and update the corridor as needed
// to handle these types of issues.
//
// The fact that local mesh queries are used to move the position and target locations results in two
// beahviors that need to be considered:
//
// Every time a move function is used there is a chance that the path will become non-optimial. Basically,
// the further the target is moved from its original location, and the further the position is moved outside
// the original corridor, the more likely the path will become non-optimal. This issue can be addressed by
// periodically running the #optimizePathTopology() and #optimizePathVisibility() methods.
//
// All local mesh queries have distance limitations. (Review the #dtNavMeshQuery methods for details.) So
// the most accurate use case is to move the position and target in small increments. If a large increment
// is used, then the corridor may not be able to accurately find the new location. Because of this limiation,
// if a position is moved in a large increment, then compare the desired and resulting polygon references.
// If the two do not match, then path replanning may be needed.  E.g. If you move the target, check
// #getLastPoly() to see if it is the expected polygon.

class Navigator
{
public:
	Navigator(NavMesh* navMesh, int maxNodes);
	~Navigator();

	//----------------------------------------------------------------------------
	// Path Planning

	// Begin navigating. If successful, use other functions to traverse the path.
	bool FindRoute(
		const glm::vec3& startPos,
		const glm::vec3& endPos,
		bool allowPartial);

	// Behavior:
	//
	// - The movement is constrained to the surface of the navigation mesh.
	// - The corridor is automatically adjusted (shortened or lengthened) in order to remain valid.
	// - The new position will be located in the adjusted corridor's first polygon.
	//
	// The expected use case is that the desired position will be 'near' the current corridor.
	// What is considered 'near' depends on local polygon desntiy, query search extents, etc.
	//
	// The resulting position will differ from the desired position if the desired position is not on the
	// navigatino mesh, or it can't be reached using a local search.
	bool MovePosition(const glm::vec3& position);

	// Behavior:
	//
	// - The movement is constrained to the surface of the navigation mesh.
	// - The corridor is automatically adjusted(shorted or lengthened) in order to remain valid.
	// - The new target will be located in the adjusted corridor's last polygon.
	//
	// The expected use case is that the desired target will be 'near' the current corridor.
	// What is considered 'near' depends on local polygon density, query search extents, etc.
	//
	// The resulting target will differ from the desired target if the desired target is not
	// on the navigation mesh, or it can't be reached using a local search.
	bool MoveTargetPosition(const glm::vec3& position);

	// Inaccurate locomotion or dynamic obstacle avoidance can force the argent position significantly
	// outside the original corridor. Over time this can result in the formation of a non-optimal
	// corridor. Non-optimal paths can also form near the corners of tiles.
	//
	// This function uses an efficient local visibility search to try to optimize the corridor
	// between the current position and @p next.
	//
	// The corridor will change only if @p next is visible from the current position and moving directly
	// toward the point is better than following the existing path.
	//
	// The more inaccurate the agent movement, the more beneficial this function becomes. Simply adjust
	// the frequency of the call to match the needs to the agent.
	//
	// This function is not suitable for long distance searches.
	void OptimizePathVisibility(const glm::vec3& next, const float pathOptimizationRange);

	// Inaccurate locomotion or dynamic obstacle avoidance can force the agent position significantly
	// outside the original corridor. Over time this can result in the formation of a non-optimal
	// corridor. This function will use a local area path search to try to re-optimize the corridor.
	//
	// The more inaccurate the agent movement, the more beneficial this function becomes. Simply adjust
	// the frequency of the call to match the needs to the agent.
	bool OptimizePathTopology();

	// This is the function used to plan local movement within the corridor. One or more corners
	// can be  detected in order to plan movement. It performs essentially the same function as
	// #dtNavMeshQuery::findStraightPath.
	//
	// Due to internal optimizations, the maximum number of corners returned will be (@p maxCorners - 1) 
	// For example: If the buffers are sized to hold 10 corners, the function will never return more than 9 corners. 
	// So if 10 corners are needed, the buffers should be sized for 11 corners.
	//
	// If the target is within range, it will be the last corner and have a polygon reference id of zero.
	bool FindCorners(NavigationBuffer& buffer, int maxCorners = -1);

	bool MoveOverOffMeshConnection(dtPolyRef offMeshConRef, glm::vec3& startPos, glm::vec3& endPos);

	bool ContainsPolyRef(dtPolyRef ref);

	// Get the status of the navigation
	dtStatus GetStatus() const { return m_lastStatus; }

	// Get error reason
	NavigatorError GetError() const { return m_error; }

	bool NeedsUpdate() const { return m_needsUpdate; }

	void SetExtents(const glm::vec3& extents) { m_extents = extents; }

	//----------------------------------------------------------------------------
	// Accessors

	dtQueryFilter& GetQueryFilter() { return *m_filter; }
	const dtQueryFilter& GetQueryFilter() const { return *m_filter; }

	void SetQueryFilter(std::unique_ptr<dtQueryFilter> filter) { m_filter = std::move(filter); }

	inline const glm::vec3& getPos() const { return m_pos; }
	inline const glm::vec3& getTarget() const { return m_target; }

	inline dtPolyRef getFirstPoly() const { return m_polyPathLen ? m_polyPath[0] : 0; }
	inline dtPolyRef getLastPoly() const { return m_polyPathLen ? m_polyPath[m_polyPathLen - 1] : 0; }

	inline const dtPolyRef* getPath() const { return m_polyPath.get(); }
	inline int getPathCount() const { return m_polyPathLen; }

private:
	// Wrapper function for friendly types
	dtStatus FindNearestPolyRef(const glm::vec3& pos, dtPolyRef* nearestRef, glm::vec3* nearestPos);

	// Reset after an error to free resources
	void Reset();

private:
	int m_maxNodes = 0;
	NavMesh* m_navMesh;
	std::shared_ptr<dtNavMeshQuery> m_query;
	std::unique_ptr<dtQueryFilter> m_filter;
	glm::vec3 m_extents;
	dtStatus m_lastStatus = DT_SUCCESS;
	NavigatorError m_error = NavigatorError::None;
	bool m_allowPartialResults = false;
	bool m_needsUpdate = false;

	glm::vec3 m_requestedStartPos, m_requestedEndPos; // original requested start/stop
	glm::vec3 m_pos, m_target; // current/target positions (on mesh)

	std::unique_ptr<dtPolyRef[]> m_polyPath;
	int m_polyPathLen = 0;
	int m_polyPathReserved = 0;
};

//----------------------------------------------------------------------------

// A single one-time use navigation request. Used for previewing paths, really.
struct FindPathOptions
{

};

bool CalculateNavigationRoute(const FindPathOptions& options,
	NavigationRoute& route);

bool withinRadiusOfOffMeshConnection(const glm::vec3& pos, const glm::vec3& connPos,
	dtPolyRef connPoly, dtNavMesh* navMesh);
bool overOffMeshConnectionStart(const glm::vec3& pos, const NavigationBuffer& buffer, dtNavMesh* navMesh,
	dtPolyRef* polyRef);
