//
// MQ2Navigation.cpp
//

#include "MQ2Navigation.h"
#include "RenderHandler.h"
#include "ImGuiRenderer.h"
#include "MQ2Nav_Hooks.h"
#include "MeshLoader.h"
#include "ModelLoader.h"
#include "NavMeshRenderer.h"
#include "MQ2Nav_Util.h"
#include "MQ2Nav_Settings.h"
#include "MQ2Nav_KeyBinds.h"
#include "Waypoints.h"
#include "DetourNavMesh.h"
#include "DetourCommon.h"
#include "EQDraw.h"

#include <set>

//============================================================================

static bool IsInt(char* buffer)
{
	if (!*buffer) return false;
	for (int i = 0; i < strlen(buffer); i++)
		if (!(isdigit(buffer[i]) || buffer[i] == '.' || (buffer[i] == '-' && i == 0))) return false;
	return true;
}

//============================================================================

#pragma region MQ2NavigationType
MQ2NavigationType::MQ2NavigationType(MQ2NavigationPlugin* nav_)
  : MQ2Type("Navigation")
  , m_nav(nav_)
{
	TypeMember(Active);
	TypeMember(MeshLoaded);
	TypeMember(PathExists);
	TypeMember(PathLength);
}

MQ2NavigationType::~MQ2NavigationType()
{
}

bool MQ2NavigationType::GetMember(MQ2VARPTR VarPtr, PCHAR Member, PCHAR Index, MQ2TYPEVAR &Dest)
{
	PMQ2TYPEMEMBER pMember = MQ2NavigationType::FindMember(Member);
	if (pMember) switch ((NavigationMembers)pMember->ID) {
	case Active:
		Dest.Type = pBoolType;
		Dest.DWord = m_nav->IsActive();
		return true;
	case MeshLoaded:
		Dest.Type = pBoolType;
		Dest.DWord = m_nav->IsMeshLoaded();
		return true;
	case PathExists:
		Dest.Type = pBoolType;
		Dest.DWord = m_nav->CanNavigateToPoint(Index);
		return true;
	case PathLength:
		Dest.Type = pFloatType;
		Dest.Float = m_nav->GetNavigationPathLength(Index);
		return true;
	}
	Dest.Type = pStringType;
	Dest.Ptr = "NULL";
	return true;
}

bool MQ2NavigationType::ToString(MQ2VARPTR VarPtr, PCHAR Destination)
{
	strcpy(Destination, "TRUE");
	return true;
}

static void NavigateCommand(PSPAWNINFO pChar, PCHAR szLine)
{
	if (g_mq2Nav)
		g_mq2Nav->Command_Navigate(pChar, szLine);
}

static BOOL NavigateData(PCHAR szName, MQ2TYPEVAR& Dest)
{
	if (g_mq2Nav)
		return g_mq2Nav->Data_Navigate(szName, Dest);

	return FALSE;
}
#pragma endregion

//----------------------------------------------------------------------------

#pragma region MQ2Navigation Plugin Class
MQ2NavigationPlugin::MQ2NavigationPlugin()
  : m_navigationType(new MQ2NavigationType(this))
  , m_meshLoader(new MeshLoader())
  , m_modelLoader(new ModelLoader())
  , m_pEQDraw(new CEQDraw)
{
	Initialize();

	if (m_initialized)
	{
		AddCommand("/navigate", NavigateCommand);
		AddMQ2Data("Navigation", NavigateData);

		SetGameState(gGameState);
	}
}

MQ2NavigationPlugin::~MQ2NavigationPlugin()
{
	if (m_initialized)
	{
		RemoveCommand("/navigate");
		RemoveMQ2Data("Navigation");
	}

	Shutdown();
}

void MQ2NavigationPlugin::OnPulse()
{
	if (!m_initialized)
		Initialize();

	m_meshLoader->Process();
	m_modelLoader->Process();

	if (m_initialized && mq2nav::ValidIngame(TRUE))
	{
		AttemptMovement();
		StuckCheck();
		AttemptClick();
	}
}

void MQ2NavigationPlugin::OnBeginZone()
{
	UpdateCurrentZone();
}

void MQ2NavigationPlugin::OnEndZone()
{
	pDoorTarget = nullptr;
	pGroundTarget = nullptr;
}

void MQ2NavigationPlugin::SetGameState(DWORD GameState)
{
	if (GameState == GAMESTATE_INGAME) {
		UpdateCurrentZone();
	}
	else {
		// don't unload the mesh until we finish zoning. We might zone
		// into the same zone (succor), so no use unload the mesh until
		// after loading completes.
		if (GameState != GAMESTATE_ZONING) {
			m_meshLoader->Reset();
		}
	}
}

void MQ2NavigationPlugin::OnAddGroundItem(PGROUNDITEM pGroundItem)
{
}

void MQ2NavigationPlugin::OnRemoveGroundItem(PGROUNDITEM pGroundItem)
{
}

void MQ2NavigationPlugin::UpdateCurrentZone()
{
	if (PCHARINFO pChar = GetCharInfo())
	{
		int zoneId = pChar->zoneId;

		zoneId &= 0x7FFF;
		if (zoneId >= MAX_ZONES)
			return;

		DebugSpewAlways("Switching to zone: %d", zoneId);

		mq2nav::LoadWaypoints(zoneId);
		m_meshLoader->SetZoneId(zoneId);
		m_modelLoader->SetZoneId(zoneId);
	}
	else
	{
		// reset things,
		DebugSpewAlways("Resetting Zone");

		m_modelLoader->Reset();
	}
}

void MQ2NavigationPlugin::Initialize()
{
	if (m_initialized)
		return;
	if (!InitializeHooks())
		return;

	g_renderHandler.reset(new RenderHandler());
	m_render = g_renderHandler;

	HWND eqhwnd = *reinterpret_cast<HWND*>(EQADDR_HWND);
	g_imguiRenderer.reset(new ImGuiRenderer(eqhwnd, g_pDevice));
	g_renderHandler->AddRenderable(g_imguiRenderer);
	g_navMeshRenderer.reset(new NavMeshRenderer(m_meshLoader.get(), g_pDevice));
	g_renderHandler->AddRenderable(g_navMeshRenderer);

	mq2nav::LoadSettings();
	mq2nav::keybinds::FindKeys();
	mq2nav::keybinds::DoKeybinds();

	m_meshLoader->SetAutoReload(mq2nav::GetSettings().autoreload);
	m_pEQDraw->Initialize();

	m_modelLoader->Initialize();

	m_initialized = true;
}

void MQ2NavigationPlugin::Shutdown()
{
	if (m_initialized)
	{
		m_modelLoader->Shutdown();

		mq2nav::keybinds::UndoKeybinds();
		Stop();

		g_renderHandler->RemoveRenderable(g_navMeshRenderer);
		g_navMeshRenderer.reset();

		g_renderHandler->RemoveRenderable(g_imguiRenderer);
		g_imguiRenderer.reset();

		g_renderHandler.reset();

		ShutdownHooks();

		m_initialized = false;
	}
}

//----------------------------------------------------------------------------

BOOL MQ2NavigationPlugin::Data_Navigate(PCHAR szName, MQ2TYPEVAR& Dest)
{
	Dest.DWord = 1;
	Dest.Type = m_navigationType.get();
	return true;
}

void MQ2NavigationPlugin::Command_Navigate(PSPAWNINFO pChar, PCHAR szLine)
{
	m_pEndingDoor = nullptr;
	m_pEndingItem = nullptr;
	CHAR buffer[MAX_STRING] = { 0 };
	int i = 0;

	//DebugSpewAlways("MQ2Navigation::NavigateCommand - start with arg: %s", szLine);
	glm::vec3 destination;
	bool haveDestination = ParseDestination(szLine, destination);

	//DebugSpewAlways("MQ2Navigation::NavigateCommand - have destination: %d", haveDestination ? 1 : 0);

	if (!haveDestination)
	{
		GetArg(buffer, szLine, 1);

		// reload nav mesh
		if (!strcmp(buffer, "reload")) {
			m_meshLoader->LoadNavMesh();
		}
		else if (!strcmp(buffer, "recordwaypoint") || !strcmp(buffer, "rwp")) {
			GetArg(buffer, szLine, 2);
			if (0 == *buffer) {
				WriteChatf(PLUGIN_MSG "usage: /navigate recordwaypoint <waypoint name> <waypoint tag>");
			}
			else {
				std::string waypointName(buffer);
				GetArg(buffer, szLine, 3);
				std::string waypointTag(buffer);
				WriteChatf(PLUGIN_MSG "recording waypoint: '%s' with tag: %s", waypointName.c_str(), waypointTag.c_str());
				if (mq2nav::AddWaypoint(waypointName, waypointTag)) {
					WriteChatf(PLUGIN_MSG "overwrote previous waypoint: '%s'", waypointName.c_str());
				}
			}
		}
		else if (!strcmp(buffer, "help")) {
			WriteChatf(PLUGIN_MSG "Usage: /navigate [save | load | reload] [target] [X Y Z] [item [click] [once]] [door  [click] [once]] [waypoint <waypoint name>] [stop] [recordwaypoint <name> <tag> ]");
		}
		else if (!strcmp(buffer, "load")) {
			mq2nav::LoadSettings();
		}
		else if (!strcmp(buffer, "save")) {
			mq2nav::SaveSettings();
		}
		else if (!strcmp(buffer, "autoreload")) {
			mq2nav::GetSettings().autoreload = !mq2nav::GetSettings().autoreload;
			m_meshLoader->SetAutoReload(mq2nav::GetSettings().autoreload);
			WriteChatf(PLUGIN_MSG "Autoreload is now %s", mq2nav::GetSettings().autoreload ? "\agon\ax" : "\aroff\ax");
			mq2nav::SaveSettings(false);
		}
		Stop();
		return;
	}

	// we were given a destination. Check if we should click once at the end.
	GetArg(buffer, szLine, 3);
	m_bSpamClick = strcmp(buffer, "once");

	BeginNavigation(destination);

	if (m_isActive)
		EzCommand("/squelch /stick off");

	// TODO: Fixme
	//mq2nav::keybinds::bDoMove = m_isActive;
	mq2nav::keybinds::stopNavigation = std::bind(&MQ2NavigationPlugin::Stop, this);
}

void MQ2NavigationPlugin::BeginNavigation(const glm::vec3& pos)
{
	// first clear existing state
	m_isActive = false;
	g_renderHandler->ClearNavigationPath();
	m_activePath.reset();

	if (!m_meshLoader->IsNavMeshLoaded())
	{
		WriteChatf(PLUGIN_MSG "\arCannot navigate - No mesh file loaded.");
		return;
	}

	m_activePath.reset(new MQ2NavigationPath(m_meshLoader->GetNavMesh()));
	g_renderHandler->SetNavigationPath(m_activePath.get());

	m_activePath->FindPath(pos);
	m_isActive = m_activePath->GetPathSize() > 0;
}

bool MQ2NavigationPlugin::IsMeshLoaded() const
{
	return m_meshLoader->IsNavMeshLoaded();
}


//============================================================================

void MQ2NavigationPlugin::AttemptClick()
{
	if (!m_isActive && !m_bSpamClick)
		return;

	// don't execute if we've got an item on the cursor.
	if (GetCharInfo2()->pInventoryArray && GetCharInfo2()->pInventoryArray->Inventory.Cursor)
		return;

	clock::time_point now = clock::now();

	// only execute every 500 milliseconds
	if (now < m_lastClick + std::chrono::milliseconds(500))
		return;
	m_lastClick = now;

	if (m_pEndingDoor && GetDistance(m_pEndingDoor->X, m_pEndingDoor->Y) < 25)
	{
		m_pEQDraw->Click(m_pEndingDoor);
	}
	else if (m_pEndingItem && GetDistance(m_pEndingItem->X, m_pEndingItem->Y) < 25)
	{
		m_pEQDraw->Click(m_pEndingItem);
	}
}

bool MQ2NavigationPlugin::ClickNearestClosedDoor(float cDistance)
{
	if (!ppSwitchMgr || !pSwitchMgr) return false;

	PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;
	PDOOR pDoor = NULL;
	PSPAWNINFO pChar = GetCharInfo()->pSpawn;

	for (DWORD index = 0; index < pDoorTable->NumEntries; index++)
	{
		if (pDoorTable->pDoor[index]->Z <= pChar->Z + gZFilter &&
			pDoorTable->pDoor[index]->Z >= pChar->Z - gZFilter)
		{
			FLOAT Distance = GetDistance(pDoorTable->pDoor[index]->X, pDoorTable->pDoor[index]->Y);
			if (Distance < cDistance) {
				pDoor = pDoorTable->pDoor[index];
				cDistance = Distance;
			}
		}
	}
	if (pDoor) {
		m_pEQDraw->Click(pDoor);
		return true;
	}
	return false;
}

void MQ2NavigationPlugin::StuckCheck()
{
	clock::time_point now = clock::now();

	// check every 100 ms
	if (now > m_stuckTimer + std::chrono::milliseconds(100))
	{
		m_stuckTimer = now;
		if (GetCharInfo())
		{
			if (GetCharInfo()->pSpawn->SpeedMultiplier != -10000
				&& FindSpeed(GetCharInfo()->pSpawn)
				&& (GetDistance(m_stuckX, m_stuckY) < FindSpeed(GetCharInfo()->pSpawn) / 600)
				&& !ClickNearestClosedDoor(25)
				&& !GetCharInfo()->pSpawn->Levitate
				&& !GetCharInfo()->pSpawn->UnderWater
				&& !GetCharInfo()->Stunned
				&& m_isActive)
			{
				MQ2Globals::ExecuteCmd(FindMappableCommand("JUMP"), 1, 0);
				MQ2Globals::ExecuteCmd(FindMappableCommand("JUMP"), 0, 0);
			}

			m_stuckX = GetCharInfo()->pSpawn->X;
			m_stuckY = GetCharInfo()->pSpawn->Y;
		}
	}
}

void MQ2NavigationPlugin::LookAt(const glm::vec3& pos)
{
	gFaceAngle = (atan2(pos.x - GetCharInfo()->pSpawn->X, pos.y - GetCharInfo()->pSpawn->Y)  * 256.0f / PI);
	if (gFaceAngle >= 512.0f) gFaceAngle -= 512.0f;
	if (gFaceAngle<0.0f) gFaceAngle += 512.0f;

	((PSPAWNINFO)pCharSpawn)->Heading = (FLOAT)gFaceAngle;

	// This is a sentinel value telling MQ2 to not adjust the face angle
	gFaceAngle = 10000.0f;

	if (GetCharInfo()->pSpawn->UnderWater == 5 || GetCharInfo()->pSpawn->FeetWet == 5)
	{
		FLOAT distance = (FLOAT)GetDistance(GetCharInfo()->pSpawn->X, GetCharInfo()->pSpawn->Y, pos.x, pos.y);
		GetCharInfo()->pSpawn->CameraAngle = (FLOAT)(atan2(pos.z - GetCharInfo()->pSpawn->Z, distance) * 256.0f / PI);
	}
	else if (GetCharInfo()->pSpawn->Levitate == 2)
	{
		if (pos.z < GetCharInfo()->pSpawn->Z - 5)
			GetCharInfo()->pSpawn->CameraAngle = -45.0f;
		else if (pos.z > GetCharInfo()->pSpawn->Z + 5)
			GetCharInfo()->pSpawn->CameraAngle = 45.0f;
		else
			GetCharInfo()->pSpawn->CameraAngle = 0.0f;
	}
	else
		GetCharInfo()->pSpawn->CameraAngle = 0.0f;

	// this is a sentinel value telling MQ2 to not adjust the look angle
	gLookAngle = 10000.0f;
}

void MQ2NavigationPlugin::AttemptMovement()
{
	if (m_isActive)
	{
		clock::time_point now = clock::now();

		if (now - m_pathfindTimer > std::chrono::milliseconds(PATHFINDING_DELAY_MS))
		{
			WriteChatf(PLUGIN_MSG "Recomputing Path...");

			// update path
			m_activePath->UpdatePath();
			m_isActive = m_activePath->GetPathSize() > 0;
			//mq2nav::keybinds::bDoMove = m_isActive;

			m_pathfindTimer = now;
		}
	}

	// if no active path, then leave
	if (!m_isActive) return;

	//WriteChatf(PLUGIN_MSG "AttemptMovement - cursor = %d, size = %d, isActive = %d",
	//	m_activePath->GetPathIndex() , m_activePath->GetPathSize(), m_isActive ? 1 : 0);
	const glm::vec3& dest = m_activePath->GetDestination();
	float distanceToTarget = GetDistance(dest.x, dest.z);
	//PSPAWNINFO me = GetCharInfo()->pSpawn;
	//WriteChatf(PLUGIN_MSG "Distance from target: %.2f. I am at: %.2f %.2f %.2f", distanceToTarget,
	//	me->X, me->Y, me->Z);

	if (m_activePath->IsAtEnd())
	{
		DebugSpewAlways("[MQ2Navigation] Reached destination at: %.2f %.2f %.2f",
			dest.x, dest.z, dest.y);

		if (PSPAWNINFO me = GetCharInfo()->pSpawn)
		{
			if (distanceToTarget < ENDPOINT_STOP_DISTANCE
				&& !m_bSpamClick)
			{
				//LookAt(dest);
			}
		}

		Stop();
	}
	else if (m_activePath->GetPathSize() > 0)
	{
		//if (!GetCharInfo()->pSpawn->SpeedRun)
		//	MQ2Globals::ExecuteCmd(FindMappableCommand("FORWARD"), 1, 0);

		glm::vec3 nextPosition = m_activePath->GetNextPosition();

		if (GetDistance(nextPosition.x, nextPosition.z) < WAYPOINT_PROGRESSION_DISTANCE)
		{
			m_activePath->Increment();

			if (!m_activePath->IsAtEnd())
			{
				m_activePath->UpdatePath();
				nextPosition = m_activePath->GetNextPosition();
			}
		}

		if (m_currentWaypoint != nextPosition)
		{
			m_currentWaypoint = nextPosition;
			DebugSpewAlways("[MQ2Navigation] Moving Towards: %.2f %.2f %.2f", nextPosition.x, nextPosition.z, nextPosition.y);
		}

		//glm::vec3 eqPoint(nextPosition.x, nextPosition.z, nextPosition.y);
		//LookAt(eqPoint);
	}
}

bool MQ2NavigationPlugin::ParseDestination(PCHAR szLine, glm::vec3& destination)
{
	bool result = true;
	CHAR buffer[MAX_STRING] = { 0 };
	GetArg(buffer, szLine, 1);

	if (!strcmp(buffer, "target") && pTarget)
	{
		PSPAWNINFO target = (PSPAWNINFO)pTarget;
		//WriteChatf("[MQ2Navigation] locating target: %s", target->Name);
		destination.x = target->X;
		destination.y = target->Y;
		destination.z = target->Z;
	}
	else if (!strcmp(buffer, "door") && pDoorTarget)
	{
		//WriteChatf("[MQ2Navigation] locating door target: %s", pDoorTarget->Name);
		destination.x = pDoorTarget->X;
		destination.y = pDoorTarget->Y;
		destination.z = pDoorTarget->Z;
		GetArg(buffer, szLine, 2);

		//TODO: move this somewhere else
		if (!strcmp(buffer, "click"))
			m_pEndingDoor = pDoorTarget;
	}
	else if (!strcmp(buffer, "item") && pGroundTarget)
	{
		//WriteChatf("[MQ2Navigation] locating item target: %s", pGroundTarget->Name);
		destination.x = pGroundTarget->X;
		destination.y = pGroundTarget->Y;
		destination.z = pGroundTarget->Z;
		GetArg(buffer, szLine, 2);

		//TODO: move this somewhere else
		if (!strcmp(buffer, "click"))
			m_pEndingItem = pGroundTarget;
	}
	else if (!strcmp(buffer, "waypoint") || !strcmp(buffer, "wp"))
	{
		GetArg(buffer, szLine, 2);

		if (0 == *buffer) {
			WriteChatf(PLUGIN_MSG "usage: /navigate waypoint <waypoint name>");
			result = false;
		}
		else
		{
			WriteChatf(PLUGIN_MSG "locating  waypoint: %s", buffer);

			mq2nav::Waypoint wp;
			if (mq2nav::GetWaypoint(std::string(buffer), wp))
			{
				destination.x = wp.location.x;
				destination.y = wp.location.y;
				destination.z = wp.location.z;
			}
			else {
				WriteChatf(PLUGIN_MSG "waypoint not found!");
				result = false;
			}
		}
	}
	else
	{
		glm::vec3 tmpDestination(0, 0, 0);
		
		//DebugSpewAlways("line: %s", szLine);
		int i = 0;
		for (; i < 3; ++i) {
			char* item = GetArg(buffer, szLine, i + 1);
			if (NULL == item)
				break;
			if (!IsInt(item))
				break;
			tmpDestination[i] = atof(item);
		}
		if (i == 3) {
			//WriteChatf("[MQ2Navigation] locating loc: %.1f, %.1f, %.1f", tmpDestination[0], tmpDestination[1], tmpDestination[2]);
			destination = tmpDestination;
		}
		else {
			result = false;
		}
	}
	return result;
}

float MQ2NavigationPlugin::GetNavigationPathLength(const glm::vec3& pos)
{
	float result = -1.f;

	MQ2NavigationPath path(m_meshLoader->GetNavMesh());
	path.FindPath(pos);

	//WriteChatf("MQ2Navigation::GetPathLength - num points: %d", numPoints);
	int numPoints = path.GetPathSize();
	if (numPoints > 0)
	{
		result = 0;

		for (int i = 0; i < numPoints - 1; ++i)
		{
			const float* first = path.GetRawPosition(i);
			const float* second = path.GetRawPosition(i + 1);

			float segment = dtVdist(first, second);
			result += segment;
			//WriteChatf("MQ2Navigation::GetPathLength - segment #%d length: %f - total: %f", i, segment, result);
		}
	}
	return result;
}

float MQ2NavigationPlugin::GetNavigationPathLength(PCHAR szLine)
{
	float result = -1.f;
	glm::vec3 destination;

	if (ParseDestination(szLine, destination)) {
		result = GetNavigationPathLength(destination);
	}
	return result;
}

bool MQ2NavigationPlugin::CanNavigateToPoint(PCHAR szLine)
{
	glm::vec3 destination;
	bool result = false;

	if (ParseDestination(szLine, destination)) {
		MQ2NavigationPath path(m_meshLoader->GetNavMesh());
		path.FindPath(destination);
		result = path.GetPathSize() > 0;
	}

	return result;
}

void MQ2NavigationPlugin::Stop()
{
	if (m_isActive)
	{
		//WriteChatf(PLUGIN_MSG "Stopping navigation");
		//MQ2Globals::ExecuteCmd(FindMappableCommand("FORWARD"), 0, 0);
	}

	g_renderHandler->ClearNavigationPath();
	m_activePath.reset();
	m_isActive = false;

	m_pEndingDoor = nullptr;
	m_pEndingItem = nullptr;

	//mq2nav::keybinds::bDoMove = m_isActive;
	mq2nav::keybinds::stopNavigation = nullptr;
}
#pragma endregion

//============================================================================

#pragma region MQ2NavigationPath
MQ2NavigationPath::MQ2NavigationPath(dtNavMesh* navMesh)
	: m_navMesh(navMesh)
{
	m_filter.setIncludeFlags(0x01); // walkable surface
}

MQ2NavigationPath::~MQ2NavigationPath()
{
}

//----------------------------------------------------------------------------

bool MQ2NavigationPath::FindPath(const glm::vec3& pos)
{
	if (nullptr != m_navMesh)
	{
		// WriteChatf("MQ2Navigation::FindPath - %.1f %.1f %.1f", X, Y, Z);
		FindPathInternal(pos);

		if (m_currentPathSize > 0)
		{
			// DebugSpewAlways("FindPath - cursor = %d, size = %d", m_currentPathCursor , m_currentPathSize);
			return true;
		}
	}
	return false;
}

void MQ2NavigationPath::FindPathInternal(const glm::vec3& pos)
{
	if (nullptr == m_navMesh)
		return;

	m_currentPathCursor = 0;
	m_currentPathSize = 0;
	m_destination = pos;

	m_query.reset(new dtNavMeshQuery);
	m_query->init(m_navMesh, 10000 /* MAX_NODES */);

	UpdatePath();
}

void MQ2NavigationPath::UpdatePath()
{
	if (m_navMesh == nullptr)
		return;
	if (m_query == nullptr)
		return;

	PSPAWNINFO me = GetCharInfo()->pSpawn;
	if (me == nullptr)
		return;

	float startOffset[3] = { me->X, me->Z, me->Y };
	float endOffset[3] = { m_destination.x, m_destination.z, m_destination.y };
	float spos[3];
	float epos[3];

	m_currentPathCursor = 0;
	m_currentPathSize = 0;
	return;

	dtPolyRef startRef, endRef;
	m_query->findNearestPoly(startOffset, m_extents, &m_filter, &startRef, spos);

	if (!startRef)
	{
		WriteChatf(PLUGIN_MSG "No start reference");
		return;
	}

	dtPolyRef polys[MAX_POLYS];
	int numPolys = 0;

	if (!m_corridor)
	{
		// initialize planning
		m_corridor.reset(new dtPathCorridor);
		m_corridor->init(MAX_PATH_SIZE);

		m_corridor->reset(startRef, startOffset);

		m_query->findNearestPoly(endOffset, m_extents, &m_filter, &endRef, epos);

		if (!endRef)
		{
			WriteChatf(PLUGIN_MSG "No end reference");
			return;
		}

		dtStatus status = m_query->findPath(startRef, endRef, spos, epos, &m_filter, polys, &numPolys, MAX_POLYS);
		if (status & DT_OUT_OF_NODES)
			DebugSpewAlways("findPath from %.2f,%.2f,%.2f to %.2f,%.2f,%.2f failed: out of nodes",
				startOffset[0], startOffset[1], startOffset[2],
				endOffset[0], endOffset[1], endOffset[2]);
		if (status & DT_PARTIAL_RESULT)
			DebugSpewAlways("findPath from %.2f,%.2f,%.2f to %.2f,%.2f,%.2f returned a partial result.",
				startOffset[0], startOffset[1], startOffset[2],
				endOffset[0], endOffset[1], endOffset[2]);

		m_corridor->setCorridor(endOffset, polys, numPolys);
	}
	else
	{
		// this is an update
		m_corridor->movePosition(startOffset, m_query.get(), &m_filter);
	}

	m_corridor->optimizePathTopology(m_query.get(), &m_filter);

	m_currentPathSize = m_corridor->findCorners(m_currentPath,
		m_cornerFlags, polys, 10, m_query.get(), &m_filter);

	//if (numPolys > 0)
	//{
	//	m_query->findStraightPath(spos, epos, polys, numPolys, m_currentPath,
	//		0, 0, &m_currentPathSize, MAX_POLYS, 0);
	//}
}


#pragma endregion
