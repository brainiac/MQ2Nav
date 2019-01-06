//
// MQ2Navigation.cpp
//

#include "MQ2Navigation.h"

#include "common/NavMesh.h"
#include "plugin/ImGuiRenderer.h"
#include "plugin/KeybindHandler.h"
#include "plugin/ModelLoader.h"
#include "plugin/PluginHooks.h"
#include "plugin/PluginSettings.h"
#include "plugin/NavigationPath.h"
#include "plugin/NavigationType.h"
#include "plugin/NavMeshLoader.h"
#include "plugin/NavMeshRenderer.h"
#include "plugin/RenderHandler.h"
#include "plugin/SwitchHandler.h"
#include "plugin/UiController.h"
#include "plugin/Utilities.h"
#include "plugin/Waypoints.h"

#include <imgui.h>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <boost/lexical_cast.hpp>
#include <chrono>
#include <set>

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")

using namespace std::chrono_literals;

//============================================================================

std::unique_ptr<RenderHandler> g_renderHandler;
std::unique_ptr<ImGuiRenderer> g_imguiRenderer;

//============================================================================

static void NavigateCommand(PSPAWNINFO pChar, PCHAR szLine)
{
	if (g_mq2Nav)
	{
		g_mq2Nav->Command_Navigate(szLine);
	}
}

static void ClickSwitch(CVector3 pos, EQSwitch* pSwitch)
{
	pSwitch->UseSwitch(GetCharInfo()->pSpawn->SpawnID, 0xFFFFFFFF, 0, nullptr);
}

void ClickDoor(PDOOR pDoor)
{
	CVector3 click;
	click.Y = pDoor->X;
	click.X = pDoor->Y;
	click.Z = pDoor->Z;
	
	ClickSwitch(click, (EQSwitch*)pDoor);
}

static void ClickGroundItem(PGROUNDITEM pGroundItem)
{
	CHAR szName[MAX_STRING] = { 0 };
	GetFriendlyNameForGroundItem(pGroundItem, szName, sizeof(szName));

	CHAR Command[MAX_STRING] = { 0 };
	sprintf_s(Command, "/itemtarget \"%s\"", szName);
	HideDoCommand((PSPAWNINFO)pLocalPlayer, Command, FALSE);

	sprintf_s(Command, "/click left item");
	HideDoCommand((PSPAWNINFO)pLocalPlayer, Command, FALSE);
}

//----------------------------------------------------------------------------

#define MsgHeader "[MQ2Nav]"

void PluginContext::Log(LogLevel level, const char* szFormat, ...)
{
	va_list vaList;
	va_start(vaList, szFormat);
	int len = _vscprintf(szFormat, vaList) + 1;// _vscprintf doesn't count // terminating '\0'  

	size_t headerlen = strlen(MsgHeader) + 1;
	size_t thelen = len + headerlen + 32;
	char *szOutput = (char *)LocalAlloc(LPTR, thelen);
	strcpy_s(szOutput, thelen, MsgHeader " ");
	vsprintf_s(szOutput + headerlen, thelen - headerlen, szFormat, vaList);
	strcat_s(szOutput, thelen, "\n");
	OutputDebugString(szOutput);
	LocalFree(szOutput);
}

#undef MsgHeader

//============================================================================

#pragma region MQ2Navigation Plugin Class
MQ2NavigationPlugin::MQ2NavigationPlugin()
	: m_context(new PluginContext)
{
}

MQ2NavigationPlugin::~MQ2NavigationPlugin()
{
}

void MQ2NavigationPlugin::Plugin_OnPulse()
{
	if (!m_initialized)
	{
		// don't try to initialize until we get into the game
		if (GetGameState() != GAMESTATE_INGAME)
			return;

		if (m_retryHooks)
		{
			m_retryHooks = false;
			Plugin_Initialize();
		}
		return;
	}

	for (const auto& m : m_modules)
	{
		m.second->OnPulse();
	}

	if (m_initialized && nav::ValidIngame(TRUE))
	{
		AttemptMovement();
		StuckCheck();
		//AttemptClick();
	}
}

void MQ2NavigationPlugin::Plugin_OnBeginZone()
{
	if (!m_initialized)
		return;

	UpdateCurrentZone();

	// stop active path if one exists
	m_isActive = false;
	m_isPaused = false;
	m_activePath.reset();
	m_mapLine->SetNavigationPath(m_activePath.get());
	Get<SwitchHandler>()->SetActive(m_isActive);

	for (const auto& m : m_modules)
	{
		m.second->OnBeginZone();
	}
}

void MQ2NavigationPlugin::Plugin_OnEndZone()
{
	if (!m_initialized)
		return;

	for (const auto& m : m_modules)
	{
		m.second->OnEndZone();
	}

	pDoorTarget = nullptr;
	pGroundTarget = nullptr;
}

void MQ2NavigationPlugin::Plugin_SetGameState(DWORD GameState)
{
	if (!m_initialized)
	{
		if (m_retryHooks) {
			m_retryHooks = false;
			Plugin_Initialize();
		}
		return;
	}

	if (GameState == GAMESTATE_INGAME) {
		UpdateCurrentZone();
	}
	else if (GameState == GAMESTATE_CHARSELECT) {
		SetCurrentZone(-1);
	}

	for (const auto& m : m_modules)
	{
		m.second->SetGameState(GameState);
	}
}

void MQ2NavigationPlugin::Plugin_OnAddGroundItem(PGROUNDITEM pGroundItem)
{
}

void MQ2NavigationPlugin::Plugin_OnRemoveGroundItem(PGROUNDITEM pGroundItem)
{
	// if the item we targetted for navigation has been removed, clear it out
	if (m_pEndingItem == pGroundItem)
	{
		m_pEndingItem = nullptr;
	}
}

void MQ2NavigationPlugin::Plugin_Initialize()
{
	if (m_initialized)
		return;

	HookStatus status = InitializeHooks();
	if (status != HookStatus::Success)
	{
		m_retryHooks = (status == HookStatus::MissingDevice);
		m_initializationFailed = (status == HookStatus::MissingOffset);
		return;
	}

	nav::LoadSettings();

	InitializeRenderer();

	AddModule<KeybindHandler>();

	NavMesh* mesh = AddModule<NavMesh>(m_context.get(),
		GetDataDirectory());
	AddModule<NavMeshLoader>(m_context.get(), mesh);

	AddModule<ModelLoader>();
	AddModule<NavMeshRenderer>();
	AddModule<UiController>();
	AddModule<SwitchHandler>();

	for (const auto& m : m_modules)
	{
		m.second->Initialize();
	}

	// get the keybind handler and connect it to our keypress handler
	auto keybindHandler = Get<KeybindHandler>();
	m_keypressConn = keybindHandler->OnMovementKeyPressed.Connect(
		[this]() { OnMovementKeyPressed(); });

	// initialize mesh loader's settings
	auto meshLoader = Get<NavMeshLoader>();
	meshLoader->SetAutoReload(nav::GetSettings().autoreload);

	m_initialized = true;

	Plugin_SetGameState(gGameState);

	AddCommand("/navigate", NavigateCommand);

	InitializeMQ2NavMacroData();

	auto ui = Get<UiController>();
	m_updateTabConn = ui->OnTabUpdate.Connect([=](TabPage page) { OnUpdateTab(page); });

	m_mapLine = std::make_shared<NavigationMapLine>();
}

void MQ2NavigationPlugin::Plugin_Shutdown()
{
	if (!m_initialized)
		return;
	
	RemoveCommand("/navigate");
	ShutdownMQ2NavMacroData();

	Stop();

	// shut down all of the modules
	for (const auto& m : m_modules)
	{
		m.second->Shutdown();
	}

	// delete all of the modules
	m_modules.clear();

	ShutdownRenderer();
	ShutdownHooks();
	
	m_initialized = false;
}

std::string MQ2NavigationPlugin::GetDataDirectory() const
{
	// the root path is where we look for all of our mesh files
	return std::string(gszINIPath) + "\\MQ2Nav";
}

//----------------------------------------------------------------------------

void MQ2NavigationPlugin::InitializeRenderer()
{
	g_renderHandler = std::make_unique<RenderHandler>();

	HWND eqhwnd = *reinterpret_cast<HWND*>(EQADDR_HWND);
	g_imguiRenderer = std::make_unique<ImGuiRenderer>(eqhwnd, g_pDevice);
}

void MQ2NavigationPlugin::ShutdownRenderer()
{
	g_imguiRenderer.reset();
	g_renderHandler.reset();
}

//----------------------------------------------------------------------------

void MQ2NavigationPlugin::Command_Navigate(const char* szLine)
{
	CHAR buffer[MAX_STRING] = { 0 };
	int i = 0;

	GetArg(buffer, szLine, 1);

	// parse /nav ui
	if (!_stricmp(buffer, "ui")) {
		nav::GetSettings().show_ui = !nav::GetSettings().show_ui;
		nav::SaveSettings(false);
		return;
	}
	
	// parse /nav pause
	if (!_stricmp(buffer, "pause"))
	{
		if (!m_isActive) 
		{
			WriteChatf(PLUGIN_MSG "Navigation must be active to pause");
			return;
		}
		m_isPaused = !m_isPaused;
		if (m_isPaused)
		{
			WriteChatf(PLUGIN_MSG "Pausing Navigation");
			MQ2Globals::ExecuteCmd(FindMappableCommand("FORWARD"), 0, 0);
		}
		else
			WriteChatf(PLUGIN_MSG "Resuming Navigation");
		
		return;
	}

	// parse /nav stop
	if (!_stricmp(buffer, "stop"))
	{
		if (m_isActive)
			Stop();
		else
			WriteChatf(PLUGIN_MSG "\arNo navigation path currently active");

		return;
	}

	// parse /nav reload
	if (!_stricmp(buffer, "reload"))
	{
		Get<NavMeshLoader>()->LoadNavMesh();
		return;
	}

	// parse /nav recordwaypoint or /nav rwp
	if (!_stricmp(buffer, "recordwaypoint") || !_stricmp(buffer, "rwp"))
	{
		GetArg(buffer, szLine, 2);
		std::string waypointName(buffer);
		GetArg(buffer, szLine, 3);
		std::string desc(buffer);

		if (waypointName.empty())
		{
			WriteChatf(PLUGIN_MSG "Usage: /nav rwp <waypoint name> <waypoint description>");
			return;
		}

		if (PSPAWNINFO pChar = (GetCharInfo() ? GetCharInfo()->pSpawn : NULL))
		{
			glm::vec3 loc = GetSpawnPosition(pChar);

			nav::AddWaypoint(nav::Waypoint{ waypointName, loc, desc });
			WriteChatf(PLUGIN_MSG "Recorded waypoint: %s at %.2f %.2f %.2f", waypointName.c_str(), loc.y, loc.x, loc.z);
		}

		return;
	}

	if (!_stricmp(buffer, "listwp"))
	{
		WriteChatf(PLUGIN_MSG "\ag%d\ax waypoint(s) for \ag%s\ax:", nav::g_waypoints.size(), GetShortZone(m_zoneId));

		for (const nav::Waypoint& wp : nav::g_waypoints)
		{
			WriteChatf(PLUGIN_MSG "  \at%s\ax: \a-w%s\ax \ay(%.2f, %.2f, %.2f)",
				wp.name.c_str(), wp.description.c_str(), wp.location.y, wp.location.x, wp.location.z);
		}
		return;
	}

	// parse /nav load
	if (!_stricmp(buffer, "load"))
	{
		nav::LoadSettings(true);
		return;
	}
	
	// parse /nav save
	if (!_stricmp(buffer, "save"))
	{
		nav::SaveSettings(true);
		return;
	}

	// parse /nav help
	if (!_stricmp(buffer, "help"))
	{
		WriteChatf(PLUGIN_MSG "Usage:");
		WriteChatf(PLUGIN_MSG "\ag/nav [save | load]\ax - save/load settings");
		WriteChatf(PLUGIN_MSG "\ag/nav reload\ax - reload navmesh");
		WriteChatf(PLUGIN_MSG "\ag/nav recordwaypoint|rwp \"<waypoint name>\" [\"<waypoint description>\"]\ax - create a waypoint at current location");
		WriteChatf(PLUGIN_MSG "\ag/nav listwp\ax - list waypoints");

		WriteChatf(PLUGIN_MSG "\aoNavigation Options:\ax");
		WriteChatf(PLUGIN_MSG "\ag/nav target\ax - navigate to target");
		WriteChatf(PLUGIN_MSG "\ag/nav id #\ax - navigate to target with ID = #");
		WriteChatf(PLUGIN_MSG "\ag/nav loc[yxz] Y X Z\ax - navigate to coordinates");
		WriteChatf(PLUGIN_MSG "\ag/nav locxyz X Y Z\ax - navigate to coordinates");
		WriteChatf(PLUGIN_MSG "\ag/nav locyx Y X\ax - navigate to 2d coordinates");
		WriteChatf(PLUGIN_MSG "\ag/nav locxy X Y\ax - navigate to 2d coordinates");
		WriteChatf(PLUGIN_MSG "\ag/nav item [click]\ax - navigate to item (and click it)");
		WriteChatf(PLUGIN_MSG "\ag/nav door [item_name | id #] [click]\ax - navigate to door/object (and click it)");
		WriteChatf(PLUGIN_MSG "\ag/nav spawn <spawn search>\ax - navigate to spawn via spawn search query");
		WriteChatf(PLUGIN_MSG "\ag/nav waypoint|wp <waypoint>\ax - navigate to waypoint");
		WriteChatf(PLUGIN_MSG "\ag/nav stop\ax - stop navigation");
		WriteChatf(PLUGIN_MSG "\ag/nav pause\ax - pause navigation");
		return;
	}
	
	// all thats left is a navigation command. leave if it isn't a valid one.
	auto destination = ParseDestination(szLine, NotifyType::All);
	if (!destination->valid)
		return;

	BeginNavigation(destination);
}

//----------------------------------------------------------------------------

void MQ2NavigationPlugin::UpdateCurrentZone()
{
	int zoneId = -1;

	if (PCHARINFO pChar = GetCharInfo())
	{
		zoneId = pChar->zoneId;

		zoneId &= 0x7FFF;
		if (zoneId >= MAX_ZONES)
			zoneId = -1;
	}

	SetCurrentZone(zoneId);
}

void MQ2NavigationPlugin::SetCurrentZone(int zoneId)
{
	if (m_zoneId != zoneId)
	{
		m_zoneId = zoneId;

		if (m_zoneId == -1)
			DebugSpewAlways("Resetting Zone");
		else
			DebugSpewAlways("Switching to zone: %d", m_zoneId);

		nav::LoadWaypoints(m_zoneId);

		for (const auto& m : m_modules)
		{
			m.second->SetZoneId(m_zoneId);
		}
	}
}

void MQ2NavigationPlugin::BeginNavigation(const std::shared_ptr<DestinationInfo>& destInfo)
{
	assert(destInfo);

	// first clear existing state
	m_isActive = false;
	m_isPaused = false;
	m_pEndingDoor = nullptr;
	m_pEndingItem = nullptr;
	m_activePath.reset();
	m_mapLine->SetNavigationPath(m_activePath.get());
	Get<SwitchHandler>()->SetActive(m_isActive);

	if (!destInfo->valid)
		return;

	if (!Get<NavMesh>()->IsNavMeshLoaded())
	{
		WriteChatf(PLUGIN_MSG "\arCannot navigate - No mesh file loaded.");
		return;
	}

	if (destInfo->clickType != ClickType::None)
	{
		m_pEndingDoor = destInfo->pDoor;
		m_pEndingItem = destInfo->pGroundItem;
	}

	m_activePath = std::make_shared<NavigationPath>(destInfo);
	if (m_activePath->FindPath())
	{
		m_activePath->SetShowNavigationPaths(true);
		m_isActive = m_activePath->GetState() != NavigationState::Inactive;
	}

	m_mapLine->SetNavigationPath(m_activePath.get());
	Get<SwitchHandler>()->SetActive(m_isActive);

	if (m_isActive)
	{
		EzCommand("/squelch /stick off");
	}
}

bool MQ2NavigationPlugin::IsMeshLoaded() const
{
	if (NavMesh* mesh = Get<NavMesh>())
	{
		return mesh->IsNavMeshLoaded();
	}

	return false;
}

void MQ2NavigationPlugin::OnMovementKeyPressed()
{
	if (m_isActive)
	{
		if (nav::GetSettings().autobreak)
		{
			Stop();
		}
		else if (nav::GetSettings().autopause)
		{
			m_isPaused = true;
		}
	}
}

//============================================================================

void MQ2NavigationPlugin::AttemptClick()
{
	if (!m_isActive)
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
		ClickDoor(m_pEndingDoor);
	}
	else if (m_pEndingItem && GetDistance(m_pEndingItem->X, m_pEndingItem->Y) < 25)
	{
		ClickGroundItem(m_pEndingItem);
	}
}

void MQ2NavigationPlugin::StuckCheck()
{
	if (m_isPaused)
		return;

	if (!nav::GetSettings().attempt_unstuck)
		return;

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
				&& !GetCharInfo()->pSpawn->mPlayerPhysicsClient.Levitate
				&& !GetCharInfo()->pSpawn->UnderWater
				&& !GetCharInfo()->Stunned
				&& m_isActive)
			{
				int jumpCmd = FindMappableCommand("JUMP");
				MQ2Globals::ExecuteCmd(jumpCmd, 1, 0);
				MQ2Globals::ExecuteCmd(jumpCmd, 0, 0);
			}

			m_stuckX = GetCharInfo()->pSpawn->X;
			m_stuckY = GetCharInfo()->pSpawn->Y;
		}
	}
}

static glm::vec3 s_lastFace;

void MQ2NavigationPlugin::LookAt(const glm::vec3& pos)
{
	if (m_isPaused)
		return;

	PSPAWNINFO pSpawn = GetCharInfo()->pSpawn;
	
	gFaceAngle = glm::atan(pos.x - pSpawn->X, pos.y - pSpawn->Y) * 256.0f / glm::pi<float>();
	if (gFaceAngle >= 512.0f) gFaceAngle -= 512.0f;
	if (gFaceAngle < 0.0f) gFaceAngle += 512.0f;

	((PSPAWNINFO)pCharSpawn)->Heading = (FLOAT)gFaceAngle;

	// This is a sentinel value telling MQ2 to not adjust the face angle
	gFaceAngle = 10000.0f;

	s_lastFace = pos;

	if (pSpawn->UnderWater == 5 || pSpawn->FeetWet == 5)
	{
		glm::vec3 spawnPos{ pSpawn->X, pSpawn->Y, pSpawn->Z };
		float distance = glm::distance(spawnPos, pos);

		pSpawn->CameraAngle = glm::atan(pos.z - pSpawn->Z, distance) * 256.0f / glm::pi<float>();
	}
	else if (pSpawn->mPlayerPhysicsClient.Levitate == 2)
	{
		if (pos.z < pSpawn->FloorHeight)
			pSpawn->CameraAngle = -45.0f;
		else if (pos.z > pSpawn->Z + 5)
			pSpawn->CameraAngle = 45.0f;
		else
			pSpawn->CameraAngle = 0.0f;
	}
	else
		pSpawn->CameraAngle = 0.0f;

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
			//WriteChatf(PLUGIN_MSG "Recomputing Path...");

			// update path
			m_activePath->UpdatePath();
			m_isActive = m_activePath->GetState() != NavigationState::Inactive;

			m_pathfindTimer = now;
		}
	}

	Get<SwitchHandler>()->SetActive(m_isActive);

	// if no active path, then leave
	if (!m_isActive) return;

	const glm::vec3& dest = m_activePath->GetDestination();
	float distanceToTarget = GetDistance(dest.x, dest.y);

	if (m_activePath->IsAtEnd())
	{
		WriteChatf(PLUGIN_MSG "\agReached destination at: %.2f %.2f %.2f",
			dest.y, dest.x, dest.z);

		LookAt(dest);

		if (m_pEndingItem || m_pEndingDoor)
		{
			AttemptClick();
		}

		Stop();
	}
	else if (m_activePath->GetState() != NavigationState::Inactive)
	{
		if (!m_isPaused)
		{
			//if (!GetCharInfo()->pSpawn->SpeedRun)
			//	MQ2Globals::ExecuteCmd(FindMappableCommand("FORWARD"), 1, 0);
		}

		//glm::vec3 nextPosition = m_activePath->GetNextPosition();

		//float distanceFromNextPosition = GetDistance(nextPosition.x, nextPosition.z);

		//if (distanceFromNextPosition < WAYPOINT_PROGRESSION_DISTANCE)
		{
			//m_activePath->Increment();

			//if (!m_activePath->IsAtEnd())
			{
				m_activePath->UpdatePath();
				//nextPosition = m_activePath->GetRoute()->;
			}
		}

		//if (m_currentWaypoint != nextPosition)
		//{
		//	m_currentWaypoint = nextPosition;
		//	DebugSpewAlways("[MQ2Nav] Moving Towards: %.2f %.2f %.2f", nextPosition.x, nextPosition.z, nextPosition.y);
		//}

		//glm::vec3 eqPoint(nextPosition.x, nextPosition.z, nextPosition.y);
		//LookAt(eqPoint);
	}
}

PDOOR ParseDoorTarget(char* buffer, const char* szLine, int& argIndex)
{
	PDOOR pDoor = pDoorTarget;

	// short circuit if the argument is "click"
	GetArg(buffer, szLine, argIndex);

	int len = strlen(buffer);
	if (len == 0 || !_stricmp(buffer, "click"))
		return pDoor;

	// follows similarly to DoorTarget
	if (!ppSwitchMgr || !pSwitchMgr) return pDoor;
	PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;

	// look up door by id
	if (!_stricmp(buffer, "id"))
	{
		argIndex++;
		GetArg(buffer, szLine, argIndex++);

		// bad param - no id provided
		if (!buffer[0])
			return nullptr;

		int id = atoi(buffer);
		for (int i = 0; i < pDoorTable->NumEntries; i++)
		{
			if (pDoorTable->pDoor[i]->ID == id)
				return pDoorTable->pDoor[i];
		}

		return nullptr;
	}

	// its not id and its not click. Its probably the name of the door!
	// find the closest door that matches. If text is 'nearest' then pick
	// the nearest door.
	float distance = FLT_MAX;
	bool searchAny = !_stricmp(buffer, "nearest");
	int id = -1;
	argIndex++;

	PSPAWNINFO pSpawn = GetCharInfo()->pSpawn;

	for (int i = 0; i < pDoorTable->NumEntries; i++)
	{
		PDOOR door = pDoorTable->pDoor[i];
		// check if name matches and if it falls within the zfilter
		if ((searchAny || !_strnicmp(door->Name, buffer, len))
			&& ((gZFilter >= 10000.0f) || ((pDoor->Z <= pSpawn->Z + gZFilter)
				&& (pDoor->Z >= pSpawn->Z - gZFilter))))
		{
			id = door->ID;
			float d = Get3DDistance(pSpawn->X, pSpawn->Y, pSpawn->Z,
				door->X, door->Y, door->Z);
			if (d < distance)
			{
				pDoor = door;
				distance = d;
			}
		}
	}

	return pDoor;
}

glm::vec3 GetSpawnPosition(PSPAWNINFO pSpawn)
{
	if (pSpawn)
	{
		bool use_floor_height = nav::GetSettings().use_spawn_floor_height;

		return glm::vec3{ pSpawn->X, pSpawn->Y, use_floor_height ? pSpawn->FloorHeight : pSpawn->Z };
	}

	return glm::vec3{};
}

std::shared_ptr<DestinationInfo> ParseDestination(const char* szLine, NotifyType notify)
{
	CHAR buffer[MAX_STRING] = { 0 };
	GetArg(buffer, szLine, 1);

	std::shared_ptr<DestinationInfo> result = std::make_shared<DestinationInfo>();
	result->command = szLine;

	if (!GetCharInfo() || !GetCharInfo()->pSpawn)
	{
		if (notify == NotifyType::Errors || notify == NotifyType::All)
			WriteChatf(PLUGIN_MSG "\arCan only navigate when in game!");
		return result;
	}

	// parse /nav target
	if (!_stricmp(buffer, "target"))
	{
		if (pTarget)
		{
			PSPAWNINFO target = (PSPAWNINFO)pTarget;

			result->pSpawn = target;
			result->eqDestinationPos = GetSpawnPosition(target);
			result->valid = true;

			if (notify == NotifyType::All)
				WriteChatf(PLUGIN_MSG "Navigating to target: %s", target->Name);
		}
		else if (notify == NotifyType::Errors || notify == NotifyType::All)
			WriteChatf(PLUGIN_MSG "\arYou need a target");

		return result;
	}
	
	// parse /nav id #
	if (!_stricmp(buffer, "id"))
	{
		GetArg(buffer, szLine, 2);
		DWORD reqId = 0;

		try { reqId = boost::lexical_cast<DWORD>(buffer); }
		catch (const boost::bad_lexical_cast&)
		{
			if (notify == NotifyType::Errors || notify == NotifyType::All)
				WriteChatf(PLUGIN_MSG "\arBad spawn id!");
			return result;
		}

		PSPAWNINFO target = (PSPAWNINFO)GetSpawnByID(reqId);
		if (!target)
		{
			if (notify == NotifyType::Errors || notify == NotifyType::All)
				WriteChatf(PLUGIN_MSG "\arCould not find spawn matching id %d", reqId);
			return result;
		}

		if (notify == NotifyType::All)
			WriteChatf(PLUGIN_MSG "Navigating to spawn: %s (%d)", target->Name, target->SpawnID);

		result->eqDestinationPos = GetSpawnPosition(target);
		result->pSpawn = target;
		result->type = DestinationType::Spawn;
		result->valid = true;

		return result;
	}

	// parse /nav spawn <text>
	if (!_stricmp(buffer, "spawn"))
	{
		SEARCHSPAWN sSpawn;
		ClearSearchSpawn(&sSpawn);

		PCHAR text = GetNextArg(szLine, 2);
		ParseSearchSpawn(text, &sSpawn);

		if (PSPAWNINFO pSpawn = SearchThroughSpawns(&sSpawn, (PSPAWNINFO)pCharSpawn))
		{
			result->eqDestinationPos = GetSpawnPosition(pSpawn);
			result->pSpawn = pSpawn;
			result->type = DestinationType::Spawn;
			result->valid = true;

			if (notify == NotifyType::All)
				WriteChatf(PLUGIN_MSG "Navigating to spawn: %s (%d)", pSpawn->Name, pSpawn->SpawnID);
		}
		else
		{
			if (notify == NotifyType::Errors || notify == NotifyType::All)
				WriteChatf(PLUGIN_MSG "\arCould not find spawn matching search '%s'", szLine);
		}

		return result;
	}

	// parse /nav door [click [once]]
	if (!_stricmp(buffer, "door") || !_stricmp(buffer, "item"))
	{
		int idx = 2;

		if (!_stricmp(buffer, "door"))
		{
			PDOOR theDoor = ParseDoorTarget(buffer, szLine, idx);

			if (!theDoor)
			{
				if (notify == NotifyType::Errors || notify == NotifyType::All)
					WriteChatf(PLUGIN_MSG "\arNo door found or bad door target!");
				return result;
			}

			result->type = DestinationType::Door;
			result->pDoor = theDoor;
			result->eqDestinationPos = { theDoor->X, theDoor->Y, theDoor->Z };
			result->valid = true;

			if (notify == NotifyType::All)
				WriteChatf(PLUGIN_MSG "Navigating to door: %s", theDoor->Name);
		}
		else
		{
			if (!pGroundTarget)
			{
				if (notify == NotifyType::Errors || notify == NotifyType::All)
					WriteChatf(PLUGIN_MSG "\arNo ground item target!");
				return result;
			}

			result->type = DestinationType::GroundItem;
			result->pGroundItem = pGroundTarget;
			result->eqDestinationPos = { pGroundTarget->X, pGroundTarget->Y, pGroundTarget->Z };
			result->valid = true;

			if (notify == NotifyType::All)
				WriteChatf(PLUGIN_MSG "Navigating to ground item: %s", pGroundTarget->Name);
		}

		// check for click and once
		GetArg(buffer, szLine, idx++);

		if (!_stricmp(buffer, "click"))
		{
			result->clickType = ClickType::Once;
		}

		return result;
	}

	// parse /nav waypoint
	if (!_stricmp(buffer, "waypoint") || !_stricmp(buffer, "wp"))
	{
		GetArg(buffer, szLine, 2);

		nav::Waypoint wp;
		if (nav::GetWaypoint(buffer, wp))
		{
			result->type = DestinationType::Waypoint;
			result->eqDestinationPos = { wp.location.x, wp.location.y, wp.location.z };
			result->valid = true;

			if (notify == NotifyType::All)
				WriteChatf(PLUGIN_MSG "Navigating to waypoint: %s", buffer);
		}
		else if (notify == NotifyType::Errors || notify == NotifyType::All)
			WriteChatf(PLUGIN_MSG "\arWaypoint '%s' not found!", buffer);

		return result;
	}

	// parse /nav x y z
	if (!_stricmp(buffer, "loc")
		|| !_stricmp(buffer, "locxyz")
		|| !_stricmp(buffer, "locyxz")
		|| !_stricmp(buffer, "locxy")
		|| !_stricmp(buffer, "locyx"))
	{
		glm::vec3 tmpDestination;
		bool noflip = !_stricmp(buffer, "locxyz") || !_stricmp(buffer, "locxy");
		if (!_stricmp(buffer, "locyx") || !_stricmp(buffer, "locxy"))
		{
			PCHARINFO pChar = GetCharInfo();

			// this will get overridden if we provide a height arg
			tmpDestination[2] = pChar->pSpawn->Z;
			result->heightType = HeightType::Nearest;
		}

		int i = 0;
		for (; i < 3; ++i)
		{
			char* item = GetArg(buffer, szLine, i + 2);
			if (!item)
				break;

			try { tmpDestination[i] = boost::lexical_cast<double>(item); }
			catch (const boost::bad_lexical_cast&) { break; }
		}
		if (i == 3 || (i == 2 && result->heightType == HeightType::Nearest))
		{
			if (notify == NotifyType::All)
			{
				if (result->heightType == HeightType::Nearest)
				{
					WriteChatf(PLUGIN_MSG "Navigating to loc: %.2f, %.2f, Nearest To (%.2f)",
						tmpDestination.x, tmpDestination.y, tmpDestination.z);
				}
				else
				{
					WriteChatf(PLUGIN_MSG "Navigating to loc: %.2f, %.2f, %.2f",
						tmpDestination.x, tmpDestination.y, tmpDestination.z);
				}
			}

			// swap the x/y coordinates for silly eq coordinate system
			if (!noflip)
				std::swap(tmpDestination.x, tmpDestination.y);

			result->type = DestinationType::Location;
			result->eqDestinationPos = tmpDestination;
			result->valid = true;
		}
		else if (notify == NotifyType::Errors || notify == NotifyType::All)
			WriteChatf(PLUGIN_MSG "\arInvalid location: %s", szLine);

		return result;
	}

	if (notify == NotifyType::Errors || notify == NotifyType::All)
		WriteChatf(PLUGIN_MSG "\arInvalid nav destination: %s", szLine);
	return result;
}

float MQ2NavigationPlugin::GetNavigationPathLength(const std::shared_ptr<DestinationInfo>& info)
{
	NavigationPath path(info);

	if (path.FindPath())
		return path.GetPathTraversalDistance();

	return -1;
}

float MQ2NavigationPlugin::GetNavigationPathLength(const char* szLine)
{
	float result = -1.f;

	auto dest = ParseDestination(szLine);
	if (dest->valid)
	{
		result = GetNavigationPathLength(dest);
	}

	return result;
}

bool MQ2NavigationPlugin::CanNavigateToPoint(const char* szLine)
{
	bool result = false;
	auto dest = ParseDestination(szLine);
	if (dest->valid)
	{
		NavigationPath path(dest);

		if (path.FindPath())
		{
			result = path.GetPathSize() > 0;
		}
	}

	return result;
}

void MQ2NavigationPlugin::Stop()
{
	if (m_isActive)
	{
		WriteChatf(PLUGIN_MSG "Stopping navigation");
		MQ2Globals::ExecuteCmd(FindMappableCommand("FORWARD"), 0, 0);
	}

	m_activePath.reset();
	m_mapLine->SetNavigationPath(m_activePath.get());
	m_isActive = false;
	m_isPaused = false;

	Get<SwitchHandler>()->SetActive(m_isActive);

	m_pEndingDoor = nullptr;
	m_pEndingItem = nullptr;
}
#pragma endregion

//----------------------------------------------------------------------------

void MQ2NavigationPlugin::OnUpdateTab(TabPage tabId)
{
	if (tabId == TabPage::Navigation)
	{
		ImGui::TextColored(ImColor(255, 255, 0), "Type /nav ui to toggle this window");

		auto charInfo = GetCharInfo();
		if (!charInfo)
			return;

		glm::vec3 myPos(charInfo->pSpawn->Y, charInfo->pSpawn->X, charInfo->pSpawn->Z);
		ImGui::Text("Loc: %.2f %.2f %.2f", myPos.x, myPos.y, myPos.z);


		if (ImGui::Checkbox("Pause navigation", &m_isPaused)) {
			if (m_isPaused)
				MQ2Globals::ExecuteCmd(FindMappableCommand("FORWARD"), 0, 0);
		}

		if (ImGui::CollapsingHeader("Pathing Debug"))
		{
			bool settingsChanged = false;
			auto& settings = nav::GetSettings();

			if (ImGui::Checkbox("Render pathing debug draw", &settings.debug_render_pathing))
				settingsChanged = true;

			if (settingsChanged)
				nav::SaveSettings();

			if (m_activePath )
			{
				auto dest = m_activePath->GetDestination();

				ImGui::LabelText("Current Waypoint", "(%.2f, %.2f, %.2f,",
					m_currentWaypoint.x, m_currentWaypoint.y, m_currentWaypoint.z);
				ImGui::LabelText("Distance to Waypoint", "%.2f", glm::distance(m_currentWaypoint, myPos));

				ImGui::LabelText("Destination", "(%.2f, %.2f, %.2f)", dest.x, dest.y, dest.z);
				ImGui::LabelText("Distance", "%.2f", glm::distance(dest, myPos));

				ImGui::Text("Path Nodes");
				ImGui::Separator();

				ImGui::BeginChild("PathNodes");
				int i = 0;

				for (const auto& node : *m_activePath->GetRoute())
				{
					ImColor color(255, 255, 255);

					if (i == 0)
						color = ImColor(255, 255, 0);
					if (i == m_activePath->GetPathSize() - 1)
						color = ImColor(255, 0, 0);

					if (node.flags() & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
					{
						color = ImColor(0, 255, 255);
					}

					const auto& pos = node.pos();
					ImGui::TextColored(color, "%04d: (%.2f, %.2f, %.2f)", i,
						pos[0], pos[1], pos[2]);

					if (node.flags() & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
					{
						ImGui::SameLine();
						ImGui::TextColored(ImColor(128, 128, 128), "(offmesh link)");
					}
				}
				ImGui::EndChild();
			}
			else {
				ImGui::LabelText("Destination", "<none>");
				ImGui::LabelText("Distance", "");
			}

			ImGui::Separator();
			ImGui::LabelText("Ending Door", "%s", m_pEndingDoor ? m_pEndingDoor->Name : "<none>");
			ImGui::LabelText("Ending Item", "%s", m_pEndingItem ? m_pEndingItem->Name : "<none>");
			ImGui::LabelText("Is Active", "%s", m_isActive ? "true" : "false");
			ImGui::LabelText("Current Waypoint", "(%.2f, %.2f, %.2f)", m_currentWaypoint.x, m_currentWaypoint.y, m_currentWaypoint.z);
			ImGui::LabelText("Stuck Data", "(%.2f, %.2f) %d", m_stuckX, m_stuckY, m_stuckTimer.time_since_epoch());
			ImGui::LabelText("Last Click", "%d", m_lastClick.time_since_epoch() / 1000000);
			ImGui::LabelText("Pathfind Timer", "%d", m_pathfindTimer.time_since_epoch() / 1000000);
		}
		if (ImGui::CollapsingHeader("Door Handler Debug"))
		{
			Get<SwitchHandler>()->DebugUI();
		}
		ImGui::Separator();

		auto navmeshRenderer = Get<NavMeshRenderer>();
		navmeshRenderer->OnUpdateUI();

		if (m_activePath) m_activePath->RenderUI();
	}
}

//----------------------------------------------------------------------------

NavigationMapLine::NavigationMapLine()
{
	m_enabled = nav::GetSettings().map_line_enabled;
	SetColor(nav::GetSettings().map_line_color);
	SetLayer(nav::GetSettings().map_line_layer);
}

void NavigationMapLine::SetEnabled(bool enabled)
{
	if (m_enabled == enabled)
		return;

	m_enabled = enabled;

	if (m_enabled)
	{
		RebuildLine();
	}
	else
	{
		Clear();
	}
}

void NavigationMapLine::SetNavigationPath(NavigationPath* path)
{
	if (m_path == path)
		return;

	m_path = path;
	m_updateConn.Disconnect();
	
	if (m_path)
	{
		m_updateConn = m_path->PathUpdated.Connect([this]() { RebuildLine(); });
		RebuildLine();
	}
	else
	{
		Clear();
	}
}

void NavigationMapLine::RebuildLine()
{
	if (!m_enabled || !m_path)
		return;

	Clear();

	auto& route = *m_path->GetRoute();

	for (const auto& node : route)
	{
		AddPoint(node.pos());
	}
}

//============================================================================
