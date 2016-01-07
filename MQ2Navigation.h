//
// MQ2Navigation.h
//

#pragma once

#include "MQ2Plugin.h"

#include "Signal.h"

#include <memory>
#include <chrono>

#define GLM_FORCE_RADIANS
#include <glm.hpp>

#define PLUGIN_MSG "\ag[MQ2Nav]\ax "

//----------------------------------------------------------------------------

class dtNavMesh;
class MQ2NavigationPlugin;
class MQ2NavigationType;
class NavigationPath;
class ModelLoader;
class RenderHandler;
class MeshLoader;

extern std::unique_ptr<MQ2NavigationPlugin> g_mq2Nav;

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
	void OnAddGroundItem(PGROUNDITEM pGroundItem);
	void OnRemoveGroundItem(PGROUNDITEM pGroundItem);

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
	static const int PATHFINDING_DELAY_MS = 200;

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

	MeshLoader* GetMeshLoader() const { return m_meshLoader.get(); }

private:
	void Initialize();
	void Shutdown();

	void UpdateCurrentZone();

	//----------------------------------------------------------------------------

	bool ParseDestination(PCHAR szLine, glm::vec3& destination);

	float GetNavigationPathLength(const glm::vec3& pos);

	void AttemptClick();
	bool ClickNearestClosedDoor(float cDistance = 30);

	void StuckCheck();

	void LookAt(const glm::vec3& pos);

	void AttemptMovement();
	void Stop();

	void OnMovementKeyPressed();

	void UpdateNavigationDisplay();

private:
	std::unique_ptr<MQ2NavigationType> m_navigationType;
	std::shared_ptr<RenderHandler> m_render;

	// our nav mesh and active path
	std::unique_ptr<MeshLoader> m_meshLoader;
	std::unique_ptr<ModelLoader> m_modelLoader;
	std::unique_ptr<NavigationPath> m_activePath;

	bool m_initialized = false;
	int m_zoneId = -1;

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

	Signal<>::ScopedConnection m_keypressConn;
};

extern std::unique_ptr<MQ2NavigationPlugin> g_mq2Nav;

//============================================================================
