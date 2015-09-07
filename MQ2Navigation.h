//
// MQ2Navigation.h
//

#pragma once

#include "MQ2Plugin.h"

#include "DetourNavMeshQuery.h"
#include "DetourPathCorridor.h"

#include <memory>
#include <chrono>

#define GLM_FORCE_RADIANS
#include <glm.hpp>

#define PLUGIN_MSG "\ag[MQ2Navigation]\ax "

//----------------------------------------------------------------------------

class dtNavMesh;
class CEQDraw;
class MQ2NavigationPlugin;
class MQ2NavigatinType;
class MQ2NavigationPath;
class MeshLoader;

extern std::unique_ptr<MQ2NavigationPlugin> g_mq2Nav;

//----------------------------------------------------------------------------

class MQ2NavigationType : public MQ2Type
{
public:
	enum NavigationMembers {
		Active = 1,
		MeshLoaded = 2,
		PathExists = 3,
		PathLength = 4,
	};

	MQ2NavigationType(MQ2NavigationPlugin* nav_);
	virtual ~MQ2NavigationType();

	virtual bool GetMember(MQ2VARPTR VarPtr, PCHAR Member, PCHAR Index, MQ2TYPEVAR& Dest) override;
	virtual bool ToString(MQ2VARPTR VarPtr, PCHAR Destination) override;

	virtual bool FromData(MQ2VARPTR& VarPtr, MQ2TYPEVAR& Source) override { return false; }
	virtual bool FromString(MQ2VARPTR& VarPtr, PCHAR Source) override { return false; }

private:
	MQ2NavigationPlugin* m_nav;
};

//----------------------------------------------------------------------------

class MQ2NavigationPlugin
{
public:
	MQ2NavigationPlugin();
	~MQ2NavigationPlugin();

	// standard plugin interface functions
	void OnPulse();
	void OnBeginZone();
	void OnEndZone();
	void SetGameState(DWORD GameState);

	// Handler for /navigate
	void Command_Navigate(PSPAWNINFO pChar, PCHAR szLine);

	// Handler for "Navigation" TLO
	BOOL Data_Navigate(PCHAR szName, MQ2TYPEVAR& Dest);

	//----------------------------------------------------------------------------
	// constants

	static const int MAX_POLYS = 4028;

	// minimum distance to a waypoint for moving to the next waypoint
	static const int WAYPOINT_PROGRESSION_DISTANCE = 5;

	// stopping distance at the final waypoint
	static const int ENDPOINT_STOP_DISTANCE = 15;

	// how often to update the path (in milliseconds)
	static const int PATHFINDING_DELAY_MS = 2000;

	//----------------------------------------------------------------------------

	bool IsActive() const { return m_isActive; }
	bool IsMeshLoaded() const;

	// Load navigation mesh for the current zone
	bool LoadNavigationMesh();

	// Check if a point is pathable (given a coordinate string)
	bool CanNavigateToPoint(PCHAR szLine);

	// Check how far away a point is (given a coordinate string)
	float GetNavigationPathLength(PCHAR szLine);

	// Begin navigating to a point
	void BeginNavigation(const glm::vec3& pos);

private:
	void Initialize();
	void Shutdown();

	//----------------------------------------------------------------------------

	bool ParseDestination(PCHAR szLine, glm::vec3& destination);

	float GetNavigationPathLength(const glm::vec3& pos);

	void AttemptClick();
	bool ClickNearestClosedDoor(float cDistance = 30);

	void StuckCheck();

	void LookAt(const glm::vec3& pos);

	void AttemptMovement();
	void Stop();

	void UpdateNavigationDisplay();

private:
	std::unique_ptr<MQ2NavigationType> m_navigationType;
	std::unique_ptr<CEQDraw> m_pEQDraw;

	// our nav mesh and active path
	std::unique_ptr<MeshLoader> m_meshLoader;
	std::unique_ptr<MQ2NavigationPath> m_activePath;

	bool m_initialized = false;

	// ending criteria (pick up item / click door)
	PDOOR m_pEndingDoor = nullptr;
	PGROUNDITEM m_pEndingItem = nullptr;

	// navigation endpoint behaviors
	bool m_bSpamClick = false;

	// whether the current path is active or not
	bool m_isActive = false;
	glm::vec3 m_currentWaypoint;

	typedef std::chrono::high_resolution_clock clock;

	clock::time_point m_lastClick = clock::now();

	clock::time_point m_stuckTimer = clock::now();
	float m_stuckX = 0;
	float m_stuckY = 0;

	clock::time_point m_pathfindTimer = clock::now();
};

//============================================================================

class MQ2NavigationPath
{
public:
	MQ2NavigationPath(dtNavMesh* navMesh);
	~MQ2NavigationPath();

	//----------------------------------------------------------------------------
	// constants

	static const int MAX_POLYS = 4028 * 4;

	static const int MAX_NODES = 2048 * 4;

	static const int MAX_PATH_SIZE = 2048 * 4;

	//----------------------------------------------------------------------------

	const glm::vec3& GetDestination() const { return m_destination; }

	int GetPathSize() const { return m_currentPathSize; }
	int GetPathIndex() const { return m_currentPathCursor; }

	bool FindPath(const glm::vec3& pos);

	void UpdatePath();

	// Check if we are at the end if our path
	inline bool IsAtEnd() const { return m_currentPathCursor >= m_currentPathSize; }

	inline glm::vec3 GetNextPosition() const
	{
		return GetPosition(m_currentPathCursor);
	}
	inline const float* GetRawPosition(int index) const
	{
		assert(index < m_currentPathSize);
		return &m_currentPath[index * 3];
	}
	inline glm::vec3 GetPosition(int index) const
	{
		const float* rawcoord = GetRawPosition(index);
		return glm::vec3(rawcoord[0], rawcoord[1], rawcoord[2]);
	}

	inline void Increment() { ++m_currentPathCursor; }

	const float* GetCurrentPath() const { return &m_currentPath[0]; }

private:
	void FindPathInternal(const glm::vec3& pos);

	glm::vec3 m_destination;

	float m_currentPath[MAX_POLYS * 3];
	unsigned char m_cornerFlags[MAX_POLYS];

	int m_currentPathCursor = 0;
	int m_currentPathSize = 0;

	// the plugin owns the mesh
	dtNavMesh* m_navMesh;

	// we own the query
	std::unique_ptr<dtNavMeshQuery> m_query;
	std::unique_ptr<dtPathCorridor> m_corridor;

	dtQueryFilter m_filter;

	float m_extents[3] = { 50, 400, 50 }; // note: X, Z, Y
};