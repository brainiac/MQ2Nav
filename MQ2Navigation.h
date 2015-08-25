//
// MQ2Navigation.h
//

#pragma once

#include "MQ2Plugin.h"

#include <memory>

//----------------------------------------------------------------------------

class dtNavMesh;
class CEQDraw;
class MQ2NavigationPlugin;

extern std::unique_ptr<MQ2NavigationPlugin> g_mq2Nav;

extern bool initialized_;

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
	friend class MQ2NavigationType;

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

private:
	void Initialize();
	void Shutdown();

	//----------------------------------------------------------------------------

	bool LoadNavigationMesh();
	int FindPath(double x, double y, double z, float* pPath);
	bool FindPath(double x, double y, double z);
	bool GetDestination(PCHAR szLine, float* destination);

	float GetPathLength(PCHAR szLine);
	float GetPathLength(double X, double Y, double Z);

	void AttemptClick();
	bool ClickNearestClosedDoor(float cDistance = 30);

	void StuckCheck();
	void LookAt(float x, float y, float z);
	void AttemptMovement();
	bool ValidEnd();
	void Stop();

private:
	std::unique_ptr<MQ2NavigationType> m_navigationType;
	std::unique_ptr<CEQDraw> m_pEQDraw;

	// our nav mesh
	std::unique_ptr<dtNavMesh> m_navMesh;

	bool m_initialized = false;

	// behaviors
	bool m_bSpamClick = false;
	bool m_bDoMove = false;

	// current zone data
	PDOOR m_pEndingDoor = nullptr;
	PGROUNDITEM m_pEndingItem = nullptr;

	// navigation
	float m_destination[3];
	float m_currentPath[MAX_POLYS * 3];
	float m_candidatePath[MAX_POLYS * 3];

	int m_currentPathCursor = 0;
	int m_currentPathSize = 0;
};
