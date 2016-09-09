//
// MQ2Navigation.cpp
//

#include "MQ2Navigation.h"
#include "NavigationPath.h"
#include "NavigationType.h"
#include "RenderHandler.h"
#include "ImGuiRenderer.h"
#include "KeybindHandler.h"
#include "MQ2Nav_Hooks.h"
#include "NavMeshLoader.h"
#include "ModelLoader.h"
#include "NavMeshRenderer.h"
#include "MQ2Nav_Util.h"
#include "MQ2Nav_Settings.h"
#include "UiController.h"
#include "Waypoints.h"

#include "DetourCommon.h"

#include <set>

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")

//============================================================================

std::unique_ptr<RenderHandler> g_renderHandler;
std::unique_ptr<ImGuiRenderer> g_imguiRenderer;

//============================================================================

static bool IsInt(char* buffer)
{
	if (!*buffer) return false;
	for (size_t i = 0; i < strlen(buffer); i++)
		if (!(isdigit(buffer[i]) || buffer[i] == '.' || (buffer[i] == '-' && i == 0))) return false;
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

void ClickDoor(PDOOR pDoor)
{
	EQSwitch *pSwitch = (EQSwitch *)pDoor;
	srand((unsigned int)time(0));
	int randclickY = rand() % 5;
	int randclickX = rand() % 5;
	int randclickZ = rand() % 5;
	
	SWITCHCLICK click;
	click.Y = pDoor->Y + randclickY;
	click.X = pDoor->X + randclickX;
	click.Z = pDoor->Z + randclickZ;
	randclickY = rand() % 5;
	randclickX = rand() % 5;
	randclickZ = rand() % 5;
	click.Y1 = click.Y + randclickY;
	click.X1 = click.X + randclickX;
	click.Z1 = click.Z + randclickZ;
	pSwitch->UseSwitch(GetCharInfo()->pSpawn->SpawnID, 0xFFFFFFFF, 0, (DWORD)&click);
}

static void ClickGroundItem(PGROUNDITEM pGroundItem)
{

}

//============================================================================

#pragma region MQ2Navigation Plugin Class
MQ2NavigationPlugin::MQ2NavigationPlugin()
	: m_navigationType(new MQ2NavigationType(this))
{
}

MQ2NavigationPlugin::~MQ2NavigationPlugin()
{
}

void MQ2NavigationPlugin::Plugin_OnPulse()
{
	if (!m_initialized)
		return;

	for (const auto& m : m_modules)
	{
		m.second->OnPulse();
	}

	if (m_initialized && mq2nav::ValidIngame(TRUE))
	{
		AttemptMovement();
		StuckCheck();
		AttemptClick();
	}
}

void MQ2NavigationPlugin::Plugin_OnBeginZone()
{
	if (!m_initialized)
		return;

	UpdateCurrentZone();

	// stop active path if one exists
	m_isActive = false;
	m_activePath.reset();

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
}

void MQ2NavigationPlugin::Plugin_Initialize()
{
	if (m_initialized)
		return;

	HookStatus status = InitializeHooks();
	if (status != HookStatus::Success)
	{
		m_retryHooks = (status == HookStatus::MissingDevice);
		return;
	}

	mq2nav::LoadSettings();

	InitializeRenderer();

	AddModule<KeybindHandler>();
	AddModule<NavMeshLoader>();
	AddModule<ModelLoader>();
	AddModule<NavMeshRenderer>();
	AddModule<UiController>();

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
	meshLoader->SetAutoReload(mq2nav::GetSettings().autoreload);

	m_initialized = true;

	Plugin_SetGameState(gGameState);

	AddCommand("/navigate", NavigateCommand);
	AddMQ2Data("Navigation", NavigateData);

	auto ui = Get<UiController>();
	m_updateTabConn = ui->OnTabUpdate.Connect([=](TabPage page) { OnUpdateTab(page); });
}

void MQ2NavigationPlugin::Plugin_Shutdown()
{
	if (!m_initialized)
		return;
	
	RemoveCommand("/navigate");
	RemoveMQ2Data("Navigation");

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

//----------------------------------------------------------------------------

void MQ2NavigationPlugin::InitializeRenderer()
{
	g_renderHandler.reset(new RenderHandler());

	HWND eqhwnd = *reinterpret_cast<HWND*>(EQADDR_HWND);
	g_imguiRenderer.reset(new ImGuiRenderer(eqhwnd, g_pDevice));
	g_renderHandler->AddRenderable(g_imguiRenderer.get());
}

void MQ2NavigationPlugin::ShutdownRenderer()
{
	g_renderHandler->RemoveRenderable(g_imguiRenderer.get());
	g_imguiRenderer.reset();

	g_renderHandler.reset();
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
	CHAR buffer[MAX_STRING] = { 0 };
	int i = 0;

	GetArg(buffer, szLine, 1);

	if (!_stricmp(buffer, "ui")) {
		mq2nav::GetSettings().show_ui = !mq2nav::GetSettings().show_ui;
		mq2nav::SaveSettings(false);
		return;
	}
	else if (!_stricmp(buffer, "pause")) {
		m_isPaused = !m_isPaused;
		return;
	}

	m_pEndingDoor = nullptr;
	m_pEndingItem = nullptr;

	//DebugSpewAlways("MQ2Nav::NavigateCommand - start with arg: %s", szLine);
	glm::vec3 destination;
	bool haveDestination = ParseDestination(szLine, destination);

	//DebugSpewAlways("MQ2Nav::NavigateCommand - have destination: %d", haveDestination ? 1 : 0);

	if (!haveDestination)
	{
		// reload nav mesh
		if (!strcmp(buffer, "reload")) {
			Get<NavMeshLoader>()->LoadNavMesh();
		}
		else if (!_stricmp(buffer, "recordwaypoint") || !_stricmp(buffer, "rwp")) {
			GetArg(buffer, szLine, 2);
			if (0 == *buffer) {
				WriteChatf(PLUGIN_MSG "Usage: /navigate recordwaypoint <waypoint name> <waypoint tag>");
			}
			else {
				std::string waypointName(buffer);
				GetArg(buffer, szLine, 3);
				std::string waypointTag(buffer);
				WriteChatf(PLUGIN_MSG "Recording waypoint: '%s' with tag: %s", waypointName.c_str(), waypointTag.c_str());
				if (mq2nav::AddWaypoint(waypointName, waypointTag)) {
					WriteChatf(PLUGIN_MSG "Overwrote previous waypoint: '%s'", waypointName.c_str());
				}
			}
		}
		else if (!_stricmp(buffer, "help")) {
			WriteChatf(PLUGIN_MSG "Usage:");
			WriteChatf(PLUGIN_MSG "\ag/navigate [save | load]\ax - save/load settings");
			WriteChatf(PLUGIN_MSG "\ag/navigate reload\ax - reload navmesh");
			WriteChatf(PLUGIN_MSG "\ag/navigate recordwaypoint <waypoint name> <waypoint tag>\ax - create a waypoint");

			WriteChatf(PLUGIN_MSG "\aoNavigation Options:\ax");
			WriteChatf(PLUGIN_MSG "\ag/navigate target\ax - navigate to target");
			WriteChatf(PLUGIN_MSG "\ag/navigate X Y Z\ax - navigate to coordinates");
			WriteChatf(PLUGIN_MSG "\ag/navigate item [click] [once]\ax - navigate to item (and click it)");
			WriteChatf(PLUGIN_MSG "\ag/navigate door [click] [once]\ax - navigate to door/object (and click it)");
			WriteChatf(PLUGIN_MSG "\ag/navigate waypoint <waypoint>\ax - navigate to waypoint");
			WriteChatf(PLUGIN_MSG "\ag/navigate stop\ax - stop navigation");
			WriteChatf(PLUGIN_MSG "\ag/navigate pause\ax - pause navigation");
		}
		else if (!_stricmp(buffer, "load")) {
			mq2nav::LoadSettings(true);
		}
		else if (!_stricmp(buffer, "save")) {
			mq2nav::SaveSettings(true);
		}
		Stop();
		return;
	}

	// we were given a destination. Check if we should click once at the end.
	GetArg(buffer, szLine, 3);
	m_bSpamClick = (_stricmp(buffer, "once") == 0);

	BeginNavigation(destination);

	if (m_isActive)
		EzCommand("/squelch /stick off");
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
		if (zoneId == m_zoneId)
			return;
	}

	if (m_zoneId != zoneId)
	{
		m_zoneId = zoneId;

		if (m_zoneId == -1)
			DebugSpewAlways("Resetting Zone");
		else
			DebugSpewAlways("Switching to zone: %d", m_zoneId);

		mq2nav::LoadWaypoints(m_zoneId);

		for (const auto& m : m_modules)
		{
			m.second->SetZoneId(m_zoneId);
		}
	}
}

void MQ2NavigationPlugin::BeginNavigation(const glm::vec3& pos)
{
	// first clear existing state
	m_isActive = false;
	m_activePath.reset();

	if (!Get<NavMeshLoader>()->IsNavMeshLoaded())
	{
		WriteChatf(PLUGIN_MSG "\arCannot navigate - No mesh file loaded.");
		return;
	}

	m_activePath = std::make_unique<NavigationPath>();
	m_activePath->FindPath(pos);

	m_isActive = m_activePath->GetPathSize() > 0;
}

bool MQ2NavigationPlugin::IsMeshLoaded() const
{
	return Get<NavMeshLoader>()->IsNavMeshLoaded();
}

void MQ2NavigationPlugin::OnMovementKeyPressed()
{
	if (m_isActive)
	{
		if (mq2nav::GetSettings().autobreak)
		{
			Stop();
		}
		else if (mq2nav::GetSettings().autopause)
		{
			m_isPaused = true;
		}
	}
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
		ClickDoor(m_pEndingDoor);
	}
	else if (m_pEndingItem && GetDistance(m_pEndingItem->X, m_pEndingItem->Y) < 25)
	{
		ClickGroundItem(m_pEndingItem);
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
		ClickDoor(pDoor);
		return true;
	}
	return false;
}

void MQ2NavigationPlugin::StuckCheck()
{
	if (m_isPaused)
		return;

	if (!mq2nav::GetSettings().attempt_unstuck)
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
				&& !ClickNearestClosedDoor(25)
				&& !GetCharInfo()->pSpawn->Levitate
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
	
	gFaceAngle = (atan2(pos.x - pSpawn->X, pos.y - pSpawn->Y)  * 256.0f / PI);
	if (gFaceAngle >= 512.0f) gFaceAngle -= 512.0f;
	if (gFaceAngle<0.0f) gFaceAngle += 512.0f;
	((PSPAWNINFO)pCharSpawn)->Heading = (FLOAT)gFaceAngle;

	// This is a sentinel value telling MQ2 to not adjust the face angle
	gFaceAngle = 10000.0f;

	s_lastFace = pos;

	if (pSpawn->UnderWater == 5 || pSpawn->FeetWet == 5)
	{
		FLOAT distance = (FLOAT)GetDistance(pSpawn->X, pSpawn->Y, pos.x, pos.y);
		pSpawn->CameraAngle = (FLOAT)(atan2(pos.z - pSpawn->Feet, distance) * 256.0f / PI);
	}
	else if (pSpawn->Levitate == 2)
	{
		if (pos.z < pSpawn->Feet)
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
			m_isActive = m_activePath->GetPathSize() > 0;

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
		DebugSpewAlways("[MQ2Nav] Reached destination at: %.2f %.2f %.2f",
			dest.x, dest.z, dest.y);
		WriteChatf(PLUGIN_MSG "\agReached destination at: %.2f %.2f %.2f",
			dest.x, dest.y, dest.z);

		if (PSPAWNINFO me = GetCharInfo()->pSpawn)
		{
			if (distanceToTarget < ENDPOINT_STOP_DISTANCE
				&& !m_bSpamClick)
			{
				LookAt(dest);
			}
		}

		Stop();
	}
	else if (m_activePath->GetPathSize() > 0)
	{
		if (!m_isPaused)
		{
			if (!GetCharInfo()->pSpawn->SpeedRun)
				MQ2Globals::ExecuteCmd(FindMappableCommand("FORWARD"), 1, 0);
		}

		glm::vec3 nextPosition = m_activePath->GetNextPosition();

		float distanceFromNextPosition = GetDistance(nextPosition.x, nextPosition.z);

		if (distanceFromNextPosition < WAYPOINT_PROGRESSION_DISTANCE)
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
			DebugSpewAlways("[MQ2Nav] Moving Towards: %.2f %.2f %.2f", nextPosition.x, nextPosition.z, nextPosition.y);
		}

		glm::vec3 eqPoint(nextPosition.x, nextPosition.z, nextPosition.y);
		LookAt(eqPoint);
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
		//WriteChatf("[MQ2Nav] locating target: %s", target->Name);
		destination.x = target->X;
		destination.y = target->Y;
		destination.z = target->Z;
	}
	else if (!strcmp(buffer, "door") && pDoorTarget)
	{
		//WriteChatf("[MQ2Nav] locating door target: %s", pDoorTarget->Name);
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
		//WriteChatf("[MQ2Nav] locating item target: %s", pGroundTarget->Name);
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
			tmpDestination[i] = static_cast<float>(atof(item));
		}
		if (i == 3) {
			//WriteChatf("[MQ2Nav] locating loc: %.1f, %.1f, %.1f", tmpDestination[0], tmpDestination[1], tmpDestination[2]);
			destination = tmpDestination;
		}
		else {
			result = false;
		}
	}
	if (result)
		m_isPaused = false;

	return result;
}

float MQ2NavigationPlugin::GetNavigationPathLength(const glm::vec3& pos)
{
	float result = -1.f;

	NavigationPath path(Get<NavMeshLoader>()->GetNavMesh() != nullptr);
	path.FindPath(pos);

	//WriteChatf("MQ2Nav::GetPathLength - num points: %d", numPoints);
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
			//WriteChatf("MQ2Nav::GetPathLength - segment #%d length: %f - total: %f", i, segment, result);
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
		NavigationPath path(Get<NavMeshLoader>()->GetNavMesh() != nullptr);
		path.FindPath(destination);
		result = path.GetPathSize() > 0;
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
	m_isActive = false;

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

		if (ImGui::Checkbox("Pause navigation", &m_isPaused)) {
			if (m_isPaused)
				MQ2Globals::ExecuteCmd(FindMappableCommand("FORWARD"), 0, 0);
		}

		if (ImGui::CollapsingHeader("Pathing Debug"))
		{
			bool settingsChanged = false;
			auto& settings = mq2nav::GetSettings();

			if (ImGui::Checkbox("Render pathing debug draw", &settings.debug_render_pathing))
				settingsChanged = true;
			if (ImGui::Checkbox("Use Pathing Corridor (experimental)", &settings.debug_use_pathing_corridor))
				settingsChanged = true;

			if (settingsChanged)
				mq2nav::SaveSettings();

			if (m_activePath) 
			{
				auto charInfo = GetCharInfo();
				glm::vec3 myPos(charInfo->pSpawn->X, charInfo->pSpawn->Z, charInfo->pSpawn->Y);
				auto dest = m_activePath->GetDestination();

				ImGui::LabelText("Position", "(%.2f, %.2f, %.2f)", myPos.x, myPos.y, myPos.z);

				ImGui::LabelText("Current Waypoint", "(%.2f, %.2f, %.2f,",
					m_currentWaypoint.x, m_currentWaypoint.y, m_currentWaypoint.z);
				ImGui::LabelText("Distance to Waypoint", "%.2f", glm::distance(m_currentWaypoint, myPos));

				ImGui::LabelText("Destination", "(%.2f, %.2f, %.2f)", dest.x, dest.y, dest.z);
				ImGui::LabelText("Distance", "%.2f", glm::distance(dest, myPos));

				ImGui::Text("Path Nodes");
				ImGui::Separator();

				ImGui::BeginChild("PathNodes");
				for (int i = 0; i < m_activePath->GetPathSize(); ++i)
				{
					ImColor color(255, 255, 255);

					if (i == 0)
						color = ImColor(255, 255, 0);
					if (i == m_activePath->GetPathIndex())
						color = ImColor(0, 255, 0);
					if (i == m_activePath->GetPathSize() - 1)
						color = ImColor(255, 0, 0);

					auto pos = m_activePath->GetRawPosition(i);
					ImGui::TextColored(color, "%04d: (%.2f, %.2f, %.2f)", i,
						pos[0], pos[1], pos[2]);
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
			ImGui::LabelText("Spam Click", "%s", m_bSpamClick ? "true" : "false");
			ImGui::LabelText("Current Waypoint", "(%.2f, %.2f, %.2f)", m_currentWaypoint.x, m_currentWaypoint.y, m_currentWaypoint.z);
			ImGui::LabelText("Stuck Data", "(%.2f, %.2f) %d", m_stuckX, m_stuckY, m_stuckTimer.time_since_epoch());
			ImGui::LabelText("Last Click", "%d", m_lastClick.time_since_epoch() / 1000000);
			ImGui::LabelText("Pathfind Timer", "%d", m_pathfindTimer.time_since_epoch() / 1000000);
		}

		ImGui::Separator();

		auto navmeshRenderer = Get<NavMeshRenderer>();
		navmeshRenderer->OnUpdateUI();

		if (m_activePath) m_activePath->OnUpdateUI();
	}
}

//============================================================================
