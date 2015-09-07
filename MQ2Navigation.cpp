//
// MQ2Navigation.cpp
//

#include "MQ2Navigation.h"
#include "MQ2Nav_Render.h"

extern bool _DEBUG_LOG;                            /* generate debug messages      */
extern char _DEBUG_LOG_FILE[260];                  /* log file path        */


#include "DetourNavMesh.h"
#include "DetourCommon.h"
#include "EQDraw.h"
#include "meshext.h"

#include "MQ2Nav_Util.h"
#include "MQ2Nav_Settings.h"
#include "MQ2Nav_KeyBinds.h"
#include "MQ2Nav_Waypoints.h"

#include <set>

#define debug_log(a); debug_log_proc(a, __FILE__, __LINE__);
extern void debug_log_proc(char *text, char *sourcefile, int sourceline);

//============================================================================

// TODO: Put this in some shared location
static const int NAVMESHSET_MAGIC = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T';
static const int NAVMESHSET_VERSION = 1;

struct NavMeshSetHeader
{
	int magic;
	int version;
	int numTiles;
	dtNavMeshParams params;
};
struct NavMeshTileHeader
{
	dtTileRef tileRef;
	int dataSize;
};

//============================================================================

static bool IsInt(char* buffer)
{
	if (!*buffer) return false;
	for (int i = 0; i < strlen(buffer); i++)
		if (!(isdigit(buffer[i]) || buffer[i] == '.' || (buffer[i] == '-' && i == 0))) return false;
	return true;
}

#define MAXPASSLEN 40
static char libpass[MAXPASSLEN + 1];

static char* GetPassword()
{
	int libpasslen;
	strcpy(libpass, "1111111111111111111111111111111111111111");
	int ff;
	libpasslen = 20;
	for (ff = 0; ff < MAXPASSLEN; ff++) {
		switch (ff) {
		case 0:
			libpass[ff] = 'A' - 1;
			break;
		case 1:
			libpass[ff] = 'A' - 1;
			break;
		case 2:
			libpass[ff] = 'v' - 1;
			break;
		case 3:
			libpass[ff] = '9' - 1;
			break;
		case 4:
			libpass[ff] = '2' - 1;
			break;
		case 5:
			libpass[ff] = '1' - 1;
			break;
		case 6:
			libpass[ff] = '.' - 1;
			break;
		case 7:
			libpass[ff] = 'b' - 1;
			break;
		case 8:
			libpass[ff] = 'Z' - 1;
			break;
		case 9:
			libpass[ff] = '8' - 1;
			break;
		case 10:
			libpass[ff] = '0' - 1;
			break;
		case 11:
			libpass[ff] = 'x' - 1;
			break;
		case 12:
			libpass[ff] = 't' - 1;
			break;
		case 13:
			libpass[ff] = 'r' - 1;
			break;
		case 14:
			libpass[ff] = 'M' - 1;
			break;
		case 15:
			libpass[ff] = 'M' - 1;
			break;
		case 16:
			libpass[ff] = 'B' - 1;
			break;
		case 17:
			libpass[ff] = ',' - 1;
			break;
		case 18:
			libpass[ff] = '5' - 1;
			break;
		case 19:
			libpass[ff] = '!' - 1;
			break;
		default:
			libpass[ff] = 0;
			break;
		}
	}
	for (ff = 0; ff < strlen(libpass); ff++)
		libpass[ff] = libpass[ff] + 1;
	return libpass;
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
		Dest.DWord = m_nav->CheckLoadMesh();
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
  , m_pEQDraw(new CEQDraw)
{
	AddCommand("/navigate", NavigateCommand);
	AddMQ2Data("Navigation", NavigateData);

	SetGameState(gGameState);
}

MQ2NavigationPlugin::~MQ2NavigationPlugin()
{
	RemoveCommand("/navigate");
	RemoveMQ2Data("Navigation");

	Shutdown();
}

void MQ2NavigationPlugin::OnPulse()
{
	if (m_initialized && mq2nav::util::ValidIngame(TRUE))
	{
		AttemptMovement();
		StuckCheck();
		AttemptClick();
	}
}

void MQ2NavigationPlugin::OnBeginZone()
{
	m_navMesh = NULL;
	mq2nav::LoadWaypoints();
}

void MQ2NavigationPlugin::OnEndZone()
{
	pDoorTarget = NULL;
	pGroundTarget = NULL;
	m_navMesh = NULL;
}

void MQ2NavigationPlugin::SetGameState(DWORD GameState)
{
	if (GameState == GAMESTATE_INGAME) {
		Initialize();
		LoadNavigationMesh();
		mq2nav::LoadWaypoints();
	}
	else {
		Shutdown();
	}
}

void MQ2NavigationPlugin::Initialize()
{
	if (m_initialized)
		return;

	DebugSpewAlways("MQ2Navigation::InitializePlugin:LoadSettings");
	mq2nav::LoadSettings();
	DebugSpewAlways("MQ2Navigation::InitializePlugin:FindKeys");
	mq2nav::keybinds::FindKeys();
	DebugSpewAlways("MQ2Navigation::InitializePlugin:DoKeybinds");
	mq2nav::keybinds::DoKeybinds();
	DebugSpewAlways("MQ2Navigation::InitializePlugin:all good");

	m_pEQDraw->Initialize();

	g_render.reset(new MQ2NavigationRender());
	m_initialized = true;
}

void MQ2NavigationPlugin::Shutdown()
{
	if (m_initialized)
	{
		mq2nav::keybinds::UndoKeybinds();
		Stop();

		g_render.reset();
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
	m_isActive = true;

	//DebugSpewAlways("MQ2Navigation::NavigateCommand - start with arg: %s", szLine);
	glm::vec3 destination;
	bool haveDestination = ParseDestination(szLine, destination);

	//DebugSpewAlways("MQ2Navigation::NavigateCommand - have destination: %d", haveDestination ? 1 : 0);

	if (!haveDestination)
	{
		GetArg(buffer, szLine, 1);

		if (!strcmp(buffer, "recordwaypoint") || !strcmp(buffer, "rwp")) {
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
			WriteChatf(PLUGIN_MSG "Usage: /navigate [save | load] [target] [X Y Z] [item [click] [once]] [door  [click] [once]] [waypoint <waypoint name>] [stop] [recordwaypoint <name> <tag> ]");
		}
		else if (!strcmp(buffer, "load")) {
			mq2nav::LoadSettings();
		}
		else if (!strcmp(buffer, "save")) {
			mq2nav::SaveSettings();
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
	mq2nav::keybinds::bDoMove = m_isActive;
	mq2nav::keybinds::stopNavigation = std::bind(&MQ2NavigationPlugin::Stop, this);
}

void MQ2NavigationPlugin::BeginNavigation(const glm::vec3& pos)
{
	// first clear existing state
	m_isActive = false;
	g_render->ClearNavigationPath();
	m_activePath.reset();

	if (!m_navMesh)
		return;

	m_activePath.reset(new MQ2NavigationPath(m_navMesh.get()));
	g_render->SetNavigationPath(m_activePath.get());

	m_activePath->FindPath(pos);
	m_isActive = m_activePath->GetPathSize() > 0;
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
			mq2nav::keybinds::bDoMove = m_isActive;

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
				LookAt(dest);
			}
		}

		Stop();
	}
	else if (m_activePath->GetPathSize() > 0)
	{
		if (!GetCharInfo()->pSpawn->SpeedRun)
			MQ2Globals::ExecuteCmd(FindMappableCommand("FORWARD"), 1, 0);

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

		glm::vec3 eqPoint(nextPosition.x, nextPosition.z, nextPosition.y);
		LookAt(eqPoint);
	}
}

bool MQ2NavigationPlugin::LoadNavigationMesh()
{
	m_navMesh.reset();
	g_render->ClearNavigationPath();
	m_activePath.reset();
	m_isActive = false;

	if (!GetCharInfo())
		return false;

	int ZoneID = GetCharInfo()->zoneId;
	if (ZoneID > MAX_ZONES)
		ZoneID &= 0x7FFF;
	if (ZoneID <= 0 || ZoneID >= MAX_ZONES)
		return false;

	bool UseRaw = false;

	//if (!GetCharInfo() || !m_pEQDraw->LoadDoorAdjustments()) return NULL;

	// Look for the .bin file and use it if it exists (overrides the .ncr file)
	char buffer[MAX_PATH];
	sprintf(buffer, "%s\\MQ2Navigation\\%s.bin", gszINIPath, GetShortZone(GetCharInfo()->zoneId));
	for (int i = 0; i < MAX_PATH; ++i)
		if (buffer[i] == '\\') buffer[i] = '/';

	// open & unrar file in mem here, then set the NavMesh to use that)
	char *outbuf = NULL;
	unsigned long outsize;
	char outfile[MAX_PATH];
	char rarfile[MAX_PATH];
	sprintf(rarfile, "%s\\MQ2Navigation\\%s.ncr", gszINIPath, GetShortZone(ZoneID));
	for (int j = 0; j < MAX_PATH; ++j)
		if (rarfile[j] == '/') rarfile[j] = '\\';

	sprintf(outfile, "%s.bin", GetShortZone(ZoneID));
	char* password = GetPassword();
	WriteChatf(PLUGIN_MSG "Preparing \ao%s\ax", rarfile);
	if (!urarlib_get(&outbuf, &outsize, outfile, rarfile, password)) {
		if (outbuf)
			free(outbuf);
		UseRaw = true;
	}
	if (!UseRaw && !outsize) {
		if (outbuf)
			free(outbuf);
		DebugSpewAlways("MQ2Navigation::Failed to load mesh file (%s).", GetShortZone(ZoneID));
		WriteChatf(PLUGIN_MSG "\ayFailed to load mesh file \ax(\am%s\ax).", GetShortZone(ZoneID));
		return false;
	}

	FILE *fp = NULL;
	if (UseRaw) {
		WriteChatf(PLUGIN_MSG "No .ncr mesh file, trying to use \ao%s \axinstead.", buffer);
		if (outbuf) {
			free(outbuf);
			outbuf = NULL;
		}
		fp = fopen(buffer, "rb");
		if (!fp) {
			for (int k = 0; k < MAX_PATH; ++k)
				if (buffer[k] == '/') buffer[k] = '\\';
			fp = fopen(buffer, "rb");
		}
		if (!fp) {
			DebugSpewAlways("MQ2Navigation::Failed to load mesh file (%s).", GetShortZone(ZoneID));
			WriteChatf(PLUGIN_MSG "\ayFailed to load mesh file \ax(\am%s\ax).", GetShortZone(ZoneID));
			return false;
		}
	}

	// file is decompressed & decrypted in memory now (or uncompressed one loaded), let's use it
	WriteChatf(PLUGIN_MSG "\agComplete.");
	int memfail = 0;
	char *outptr;
	NavMeshSetHeader header;
	if (UseRaw)
		fread(&header, sizeof(NavMeshSetHeader), 1, fp);
	else {
		outptr = &outbuf[0];
		if (memmove(&header, outptr, sizeof(NavMeshSetHeader)))
			outptr = outptr + sizeof(NavMeshSetHeader);
		else
			memfail++;
	}
	if (header.magic != NAVMESHSET_MAGIC) {
		debug_log("Header.magic bad!");
		if (fp)
			fclose(fp);
		if (outbuf)
			free(outbuf);
		return false;
	}
	if (header.version != NAVMESHSET_VERSION) {
		debug_log("Header.version bad!");
		if (fp)
			fclose(fp);
		if (outbuf)
			free(outbuf);
		return false;
	}

	std::unique_ptr<dtNavMesh> navMesh(new dtNavMesh);
	if (!navMesh || !navMesh->init(&header.params))
	{
		debug_log("Header.params bad!");
		if (fp)
			fclose(fp);
		if (outbuf)
			free(outbuf);
		return false;
	}
	// Read tiles.
	int passtile = 0, failtile = 0;
	debug_log("LoadNavigationMesh() - filling tiles");
	for (int i = 0; i < header.numTiles; ++i) {
		if (0 == (i % 100)) {
			DebugSpewAlways("LoadNavigationMesh() - tile #%d", i);
		}

		NavMeshTileHeader tileHeader;
		if (UseRaw)
			fread(&tileHeader, sizeof(tileHeader), 1, fp);
		else {
			if (memmove(&tileHeader, outptr, sizeof(tileHeader)))
				outptr = outptr + sizeof(tileHeader);
			else
				memfail++;
		}
		if (!tileHeader.tileRef || !tileHeader.dataSize)
			break;
		unsigned char* data = new unsigned char[tileHeader.dataSize];
		if (!data)
			break;
		memset(data, 0, tileHeader.dataSize);
		if (UseRaw)
			fread(data, tileHeader.dataSize, 1, fp);
		else {
			if (memmove(data, outptr, tileHeader.dataSize))
				outptr = outptr + tileHeader.dataSize;
			else
				memfail++;
		}
		if (navMesh->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, 0))
			passtile++;
		else
			failtile++;
	}
	if (fp)
		fclose(fp);
	if (outbuf)
		free(outbuf);
	char szTmp[MAX_STRING];
	sprintf(szTmp, "Header info: numTiles=%d, readTiles=%d, failTiles=%d, memFail=%d.", header.numTiles, passtile, failtile, memfail);
	debug_log(szTmp);
	WriteChatf(PLUGIN_MSG "\atTiles loaded\ax: %s%d\ax/%s%d", (header.numTiles && !failtile) ? "\ag" : "\ay", passtile, header.numTiles ? "\ag" : "\ay", header.numTiles);

	m_navMesh = std::move(navMesh);
	return true;
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

	MQ2NavigationPath path(m_navMesh.get());
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
		MQ2NavigationPath path(m_navMesh.get());
		path.FindPath(destination);
		result = path.GetPathSize() > 0;
	}

	return result;
}

void MQ2NavigationPlugin::Stop()
{
	if (m_isActive)
	{
		WriteChatf(PLUGIN_MSG "Stopping");
		MQ2Globals::ExecuteCmd(FindMappableCommand("FORWARD"), 0, 0);
	}

	g_render->ClearNavigationPath();
	m_activePath.reset();
	m_isActive = false;

	mq2nav::keybinds::bDoMove = m_isActive;
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
