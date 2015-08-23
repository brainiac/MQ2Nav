// MQ2Navigation.cpp : Defines the entry point for the DLL application.
//

// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup, do NOT do it in DllMain.

#include "../MQ2Plugin.h"

PreSetup("MQ2Navigation");
PLUGIN_VERSION(1.20);

void PluginOn();
void PluginOff();

extern bool _DEBUG_LOG;                            /* generate debug messages      */
extern char _DEBUG_LOG_FILE[260];                  /* log file path        */


#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourCommon.h"
#include "EQDraw.h"
#include "meshext.h"

#include "MQ2NavUtil.h"
#include "MQ2NavSettings.h"
#include "MQ2NavKeyBinds.h"
#include "MQ2NavWaypoints.h"

#define debug_log(a); debug_log_proc(a, __FILE__, __LINE__);
extern void debug_log_proc(char *text, char *sourcefile, int sourceline);

namespace mq2nav {

CEQDraw* m_pEQDraw = new CEQDraw();

PDOOR m_pEndingDoor;
PGROUNDITEM m_pEndingItem;

bool m_bSpamClick = false;
bool m_bDoMove = false;

float m_destination[3];
float m_currentPath[MAX_POLYS * 3];
float m_candidatePath[MAX_POLYS * 3];
PSPAWNINFO MyTarget = NULL;

int m_currentPathCursor;
int m_currentPathSize;

dtNavMesh* m_navMesh;

bool initialized_;


void AttemptClick()
{
	static clock_t clickGovernor = time(NULL);
	if ((!m_bDoMove && !m_bSpamClick) || clickGovernor + 0.5 > time(NULL) || (GetCharInfo2()->pInventoryArray && GetCharInfo2()->pInventoryArray->Inventory.Cursor)) return;
	clickGovernor = time(NULL);
	if (m_pEndingDoor && GetDistance(m_pEndingDoor->X, m_pEndingDoor->Y) < 25)
		m_pEQDraw->Click(m_pEndingDoor);
	else if (m_pEndingItem && GetDistance(m_pEndingItem->X, m_pEndingItem->Y) < 25)
		m_pEQDraw->Click(m_pEndingItem);
}

bool ClickNearestClosedDoor(FLOAT cDistance)
{
	if (!ppSwitchMgr || !pSwitchMgr) return false;
	PDOORTABLE pDoorTable = (PDOORTABLE)pSwitchMgr;
	PDOOR pDoor = NULL;
	_SPAWNINFO *pChar = GetCharInfo()->pSpawn;
	for (DWORD index = 0; index < pDoorTable->NumEntries; index++) {
		if (pDoorTable->pDoor[index]->Z <= pChar->Z + gZFilter &&
			pDoorTable->pDoor[index]->Z >= pChar->Z - gZFilter) {
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

void StuckCheck()
{
	static time_t StuckMonitor = time(NULL);
	static float StuckMonitorX = 0;
	static float StuckMonitorY = 0;
	time_t now = time(NULL);
	if (StuckMonitor + 0.1 < now) {
		StuckMonitor = now;
		if (GetCharInfo()) {
			if (GetCharInfo()->pSpawn->SpeedMultiplier != -10000 &&
				FindSpeed(GetCharInfo()->pSpawn) &&
				GetDistance(StuckMonitorX, StuckMonitorY) < FindSpeed(GetCharInfo()->pSpawn) / 600 &&
				!ClickNearestClosedDoor(25) &&
				!GetCharInfo()->pSpawn->Levitate &&
				!GetCharInfo()->pSpawn->UnderWater &&
				!GetCharInfo()->Stunned &&
				m_bDoMove) {
				MQ2Globals::ExecuteCmd(FindMappableCommand("JUMP"), 1, 0);
				MQ2Globals::ExecuteCmd(FindMappableCommand("JUMP"), 0, 0);
			}
			StuckMonitorX = GetCharInfo()->pSpawn->X;
			StuckMonitorY = GetCharInfo()->pSpawn->Y;
		}
	}
}

void LookAt(FLOAT X, FLOAT Y, FLOAT Z)
{
	gFaceAngle = (atan2(X - GetCharInfo()->pSpawn->X, Y - GetCharInfo()->pSpawn->Y)  * 256.0f / PI);
	if (gFaceAngle >= 512.0f) gFaceAngle -= 512.0f;
	if (gFaceAngle<0.0f) gFaceAngle += 512.0f;
	((PSPAWNINFO)pCharSpawn)->Heading = (FLOAT)gFaceAngle;
	gFaceAngle = 10000.0f;
	if (GetCharInfo()->pSpawn->UnderWater == 5 || GetCharInfo()->pSpawn->FeetWet == 5)
		GetCharInfo()->pSpawn->CameraAngle = (FLOAT)(atan2(Z - GetCharInfo()->pSpawn->Z, (FLOAT)GetDistance(GetCharInfo()->pSpawn->X, GetCharInfo()->pSpawn->Y, X, Y)) * 256.0f / PI);
	else if (GetCharInfo()->pSpawn->Levitate == 2)
		if (Z < GetCharInfo()->pSpawn->Z - 5)
			GetCharInfo()->pSpawn->CameraAngle = -45.0f;
		else if (Z > GetCharInfo()->pSpawn->Z + 5)
			GetCharInfo()->pSpawn->CameraAngle = 45.0f;
		else
			GetCharInfo()->pSpawn->CameraAngle = 0.0f;
	else
		GetCharInfo()->pSpawn->CameraAngle = 0.0f;
	gLookAngle = 10000.0f;
}

void AttemptMovement()
{
	static clock_t pathfindingTimer = clock();
	if (m_bDoMove && clock() - pathfindingTimer > PATHFINDING_DELAY) {
		m_bDoMove = FindPath(m_destination[0], m_destination[1], m_destination[2]);
		pathfindingTimer = clock();
	}
	if (!m_bDoMove) return;
	//WriteChatf("AttemptMovement - cursor = %d, size = %d, doMove = %d", m_currentPathCursor , m_currentPathSize, m_bDoMove ? 1 : 0);
	if (m_currentPathCursor >= m_currentPathSize || ValidEnd()) {
		Stop();
	}
	else if (m_currentPathSize) {
		if (!GetCharInfo()->pSpawn->SpeedRun) MQ2Globals::ExecuteCmd(FindMappableCommand("FORWARD"), 1, 0);
		if (GetDistance(m_currentPath[m_currentPathCursor * 3], m_currentPath[m_currentPathCursor * 3 + 2]) < WAYPOINT_PROGRESSION_DISTANCE) m_currentPathCursor++;
		LookAt(m_currentPath[m_currentPathCursor * 3], m_currentPath[m_currentPathCursor * 3 + 2], m_currentPath[m_currentPathCursor * 3 + 1] + 0.4f);
	}
}

bool IsInt(char* buffer)
{
	if (!*buffer) return false;
	for (int i = 0; i < strlen(buffer); i++)
		if (!(isdigit(buffer[i]) || buffer[i] == '.' || (buffer[i] == '-' && i == 0))) return false;
	return true;
}

#define MAXPASSLEN 40
dtNavMesh* LoadMesh()
{
	bool UseRaw = false;
	int libpasslen;
	char libpass[MAXPASSLEN + 1];
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
	if (!GetCharInfo())
		return NULL;
	int ZoneID;
	ZoneID = GetCharInfo()->zoneId;
	if (ZoneID > MAX_ZONES)
		ZoneID &= 0x7FFF;
	if (ZoneID <= 0 || ZoneID >= MAX_ZONES)
		return NULL;
	static const int NAVMESHSET_MAGIC = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T';
	static const int NAVMESHSET_VERSION = 1;
	static struct NavMeshSetHeader
	{
		int magic;
		int version;
		int numTiles;
		dtNavMeshParams params;
	};
	static struct NavMeshTileHeader
	{
		dtTileRef tileRef;
		int dataSize;
	};
	m_currentPathSize = 0;
	m_currentPathCursor = 0;


	//if (!GetCharInfo() || !m_pEQDraw->LoadDoorAdjustments()) return NULL;
	if (!GetCharInfo()) return NULL;
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
	WriteChatf("\arMQ2Navigation\ax::\atPreparing \ao%s\ax", rarfile);
	if (!urarlib_get(&outbuf, &outsize, outfile, rarfile, libpass)) {
		if (outbuf)
			free(outbuf);
		UseRaw = true;
	}
	if (!UseRaw && !outsize) {
		if (outbuf)
			free(outbuf);
		DebugSpewAlways("MQ2Navigation::Failed to load mesh file (%s).", GetShortZone(ZoneID));
		WriteChatf("\arMQ2Navigation\ax::\ayFailed to load mesh file \ax(\am%s\ax).", GetShortZone(ZoneID));
		return NULL;
	}
	FILE *fp = NULL;
	if (UseRaw) {
		WriteChatf("\arMQ2Navigation\ax::\atNo .ncr mesh file, trying to use \ao%s \axinstead.", buffer);
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
			WriteChatf("\arMQ2Navigation\ax::\ayFailed to load mesh file \ax(\am%s\ax).", GetShortZone(ZoneID));
			return NULL;
		}
	}
	// file is decompressed & decrypted in memory now (or uncompressed one loaded), let's use it
	WriteChatf("\arMQ2Navigation\ax::\agComplete.");
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
		return NULL;
	}
	if (header.version != NAVMESHSET_VERSION) {
		debug_log("Header.version bad!");
		if (fp)
			fclose(fp);
		if (outbuf)
			free(outbuf);
		return NULL;
	}
	dtNavMesh* navmesh = new dtNavMesh;
	if (!navmesh || !navmesh->init(&header.params))
	{
		debug_log("Header.params bad!");
		if (fp)
			fclose(fp);
		if (outbuf)
			free(outbuf);
		return NULL;
	}
	// Read tiles.
	int passtile = 0, failtile = 0;
	debug_log("LoadMesh() - filling tiles");
	for (int i = 0; i < header.numTiles; ++i) {
		if (0 == (i % 100)) {
			DebugSpewAlways("LoadMesh() - tile #%d", i);
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
		if (navmesh->addTile(data, tileHeader.dataSize, DT_TILE_FREE_DATA, tileHeader.tileRef, 0))
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
	WriteChatf("\arMQ2Navigation\ax::\atTiles loaded\ax: %s%d\ax/%s%d", (header.numTiles && !failtile) ? "\ag" : "\ay", passtile, header.numTiles ? "\ag" : "\ay", header.numTiles);
	return navmesh;
}

bool LoadNavMesh()
{
	m_navMesh = LoadMesh();
	return m_navMesh != NULL;
}

bool GetDestination(PCHAR szLine, float* destination)
{
	bool result = true;
	CHAR buffer[MAX_STRING] = { 0 };
	GetArg(buffer, szLine, 1);
	if (!strcmp(buffer, "target") && pTarget) {
		PSPAWNINFO target = (PSPAWNINFO)pTarget;
		//WriteChatf("[MQ2Navigation] locating target: %s", target->Name);
		destination[0] = target->X;
		destination[1] = target->Y;
		destination[2] = target->Z;
	}
	else if (!strcmp(buffer, "door") && pDoorTarget) {
		//WriteChatf("[MQ2Navigation] locating door target: %s", pDoorTarget->Name);
		destination[0] = pDoorTarget->X;
		destination[1] = pDoorTarget->Y;
		destination[2] = pDoorTarget->Z;
		GetArg(buffer, szLine, 2);
		//TODO: move this somewhere else
		if (!strcmp(buffer, "click")) m_pEndingDoor = pDoorTarget;
	}
	else if (!strcmp(buffer, "item") && pGroundTarget) {
		//WriteChatf("[MQ2Navigation] locating item target: %s", pGroundTarget->Name);
		destination[0] = pGroundTarget->X;
		destination[1] = pGroundTarget->Y;
		destination[2] = pGroundTarget->Z;
		GetArg(buffer, szLine, 2);
		//TODO: move this somewhere else
		if (!strcmp(buffer, "click")) m_pEndingItem = pGroundTarget;
	}
	else if (!strcmp(buffer, "waypoint") || !strcmp(buffer, "wp")) {
		GetArg(buffer, szLine, 2);
		if (0 == *buffer) {
			WriteChatf("[MQ2Navigation] usage: /navigate waypoint <waypoint name>");
			result = false;
		}
		else {
			WriteChatf("[MQ2Navigation] locating  waypoint: %s", buffer);
			std::pair<bool, waypoints::Waypoint> findWP = waypoints::getWaypoint(std::string(buffer));
			if (findWP.first) {
				destination[0] = findWP.second.location.v.X;
				destination[1] = findWP.second.location.v.Y;
				destination[2] = findWP.second.location.v.Z;
			}
			else {
				WriteChatf("[MQ2Navigation] waypoint not found!");
				result = false;
			}
		}
	}
	else {
		float tmpDestination[3] = { 0, 0, 0 };
		//DebugSpewAlways("line: %s", szLine);
		int i = 1;
		for (; i < 4; ++i) {
			char* item = GetArg(buffer, szLine, i);
			if (NULL == item)
				break;
			if (!IsInt(item))
				break;
			tmpDestination[i - 1] = atof(item);
		}
		if (4 == i) {
			//WriteChatf("[MQ2Navigation] locating loc: %.1f, %.1f, %.1f", tmpDestination[0], tmpDestination[1], tmpDestination[2]);
			memcpy(destination, tmpDestination, sizeof(tmpDestination));
		}
		else {
			result = false;
		}
	}
	return result;
}

int FindPath(double X, double Y, double Z, float* pPath)
{
	if (NULL == m_navMesh)
		return 0;
	EQData::_SPAWNINFO* Me = GetCharInfo()->pSpawn;
	if (NULL == Me)
		return 0;

	int numPoints = 0;
	static dtQueryFilter filter;
	static float extents[3] = { 50, 400, 50 }; // Note: X, Z, Y
	float endOffset[3] = { X, Z, Y };
	float startOffset[3] = { Me->X, Me->Z, Me->Y };
	float spos[3];
	float epos[3];
	filter.setIncludeFlags(0x01); // walkable surfacee
	if (dtPolyRef startRef = m_navMesh->findNearestPoly(startOffset, extents, &filter, spos))
		if (dtPolyRef endRef = m_navMesh->findNearestPoly(endOffset, extents, &filter, epos)) {
			dtPolyRef polys[MAX_POLYS];
			int numPolys = m_navMesh->findPath(startRef, endRef, spos, epos, &filter, polys, MAX_POLYS);
			if (numPolys > 0)
				numPoints = m_navMesh->findStraightPath(spos, epos, polys, numPolys,
					pPath, NULL, NULL, MAX_POLYS);
		}
		else
			WriteChatf("[MQ2Navigation] No end reference");
	else
		WriteChatf("[MQ2Navigation] No start reference");

	return numPoints;
}


float GetPathLength(double X, double Y, double Z)
{
	float result = -1.f;
	int numPoints = FindPath(X, Y, Z, m_candidatePath);
	//WriteChatf("MQ2Navigation::GetPathLength - num points: %d", numPoints);
	if (numPoints > 0) {
		result = 0.f;
		for (int i = 0; i < numPoints - 1; ++i) {
			float segment = dtVdist((m_candidatePath + 3 * i), (m_candidatePath + 3 * (i + 1)));
			result += segment;
			//WriteChatf("MQ2Navigation::GetPathLength - segment #%d length: %f - total: %f", i, segment, result);
		}
	}
	return result;
}

float GetPathLength(PCHAR szLine)
{
	float result = -1.f;
	float destination[3];
	if (GetDestination(szLine, destination)) {
		result = GetPathLength(destination[0], destination[1], destination[2]);
	}
	return result;
}

bool FindPath(double X, double Y, double Z)
{
	if (NULL != m_navMesh) {
		//WriteChatf("MQ2Navigation::FindPath - %.1f %.1f %.1f", X, Y, Z);
		int numPoints = FindPath(X, Y, Z, m_currentPath);
		if (numPoints > 0) {
			m_currentPathSize = numPoints;
			m_currentPathCursor = 0;
			//DebugSpewAlways("FindPath - cursor = %d, size = %d", m_currentPathCursor , m_currentPathSize );
			//WriteChatf("FindPath - cursor = %d, size = %d", m_currentPathCursor , m_currentPathSize );
			return true;
		}
	}
	else {
		m_navMesh = LoadMesh();
		return true;
	}
	return false;
}

VOID NavigateCommand(PSPAWNINFO pChar, PCHAR szLine)
{
	m_pEndingDoor = NULL;
	m_pEndingItem = NULL;
	CHAR buffer[MAX_STRING] = { 0 };
	int i = 0;
	m_bDoMove = true;

	//DebugSpewAlways("MQ2Navigation::NavigateCommand - start with arg: %s", szLine);
	bool haveDestination = GetDestination(szLine, m_destination);
	//DebugSpewAlways("MQ2Navigation::NavigateCommand - have destination: %d", haveDestination ? 1 : 0);
	if (!haveDestination) {
		GetArg(buffer, szLine, 1);
		if (!strcmp(buffer, "recordwaypoint") || !strcmp(buffer, "rwp")) {
			GetArg(buffer, szLine, 2);
			if (0 == *buffer) {
				WriteChatf("[MQ2Navigation] usage: /navigate recordwaypoint <waypoint name> <waypoint tag>");
			}
			else {
				std::string waypointName(buffer);
				GetArg(buffer, szLine, 3);
				std::string waypointTag(buffer);
				WriteChatf("[MQ2Navigation] recording waypoint: '%s' with tag: %s", waypointName.c_str(), waypointTag.c_str());
				if (waypoints::addWaypoint(waypointName, waypointTag)) {
					WriteChatf("[MQ2Navigation] overwrote previous waypoint: '%s'", waypointName.c_str());
				}
			}
		}
		else if (!strcmp(buffer, "help")) {
			WriteChatf("[MQ2Navigation] Usage: /navigate [save | load] [target] [X Y Z] [item [click] [once]] [door  [click] [once]] [waypoint <waypoint name>] [stop] [recordwaypoint <name> <tag> ]");
		}
		else if (!strcmp(buffer, "load")) {
			settings::LoadSettings();
		}
		else if (!strcmp(buffer, "save")) {
			settings::SaveSettings();
		}
		Stop();
		return;
	}
	GetArg(buffer, szLine, 3);
	m_bSpamClick = strcmp(buffer, "once");

	if (m_bDoMove = FindPath(m_destination[0], m_destination[1], m_destination[2]))
		EzCommand("/squelch /stick off");
}

bool ValidEnd()
{
	if (m_currentPathCursor >= m_currentPathSize)
		if (EQData::_SPAWNINFO* Me = GetCharInfo()->pSpawn)
			if (GetDistance(m_destination[0], m_destination[1]) < ENDPOINT_STOP_DISTANCE) {
				if (!m_bSpamClick)
					LookAt(m_destination[0], m_destination[1], m_destination[2]);
				return true;
			}
	return false;
}

void Stop()
{
	WriteChatf("[MQ2Navigation] Stopping");
	MQ2Globals::ExecuteCmd(FindMappableCommand("FORWARD"), 0, 0);
	m_currentPathSize = 0;
	m_bDoMove = false;
}



class MQ2NavigationType : public MQ2Type
{
public:
	enum NavigationMembers {
		Active = 1,
		MeshLoaded = 2,
		PathExists = 3,
		PathLength = 4
	};

	MQ2NavigationType() :MQ2Type("Navigation")
	{
		TypeMember(Active);
		TypeMember(MeshLoaded);
		TypeMember(PathExists);
		TypeMember(PathLength);
	}

	bool GetMember(MQ2VARPTR VarPtr, PCHAR Member, PCHAR Index, MQ2TYPEVAR &Dest)
	{
		if (!mmo.Active)
			return false;
		PMQ2TYPEMEMBER pMember = MQ2NavigationType::FindMember(Member);
		if (pMember) switch ((NavigationMembers)pMember->ID) {
		case Active:
			Dest.Type = pBoolType;
			if (!mmo.Active)
				Dest.DWord = false;
			else
				Dest.DWord = m_bDoMove;
			return true;
		case MeshLoaded:
			Dest.Type = pBoolType;
			if (!mmo.Active)
				Dest.DWord = false;
			else
				Dest.DWord = m_navMesh != NULL || (m_navMesh = LoadMesh());
			return true;
		case PathExists:
			Dest.Type = pBoolType;
			Dest.DWord = (GetPathLength(Index) >= 0.f) ? 1 : 0;
			return true;
		case PathLength:
			Dest.Type = pFloatType;
			Dest.Float = GetPathLength(Index);
			return true;
		}
		Dest.Type = pStringType;
		Dest.Ptr = "NULL";
		return true;
	}

	bool ToString(MQ2VARPTR VarPtr, PCHAR Destination)
	{
		if (!mmo.Active)
			return false;
		strcpy(Destination, "TRUE");
		return true;
	}
	bool FromData(MQ2VARPTR &VarPtr, MQ2TYPEVAR &Source) { return false; }
	bool FromString(MQ2VARPTR &VarPtr, PCHAR Source) { return false; }
	~MQ2NavigationType() { }
} *navigationType_;


BOOL dataNavigation(PCHAR szName, MQ2TYPEVAR &Dest)
{
	if (!mmo.Active)
		return false;
	Dest.DWord = 1;
	Dest.Type = navigationType_;
	return true;
}


BOOL init()
{
	BOOL result = !mq2nav::initialized_;
	if (result) {
		DebugSpewAlways("MQ2Navigation::InitializePlugin:LoadSettings");
		mq2nav::settings::LoadSettings();
		DebugSpewAlways("MQ2Navigation::InitializePlugin:FindKeys");
		mq2nav::keybinds::FindKeys();
		DebugSpewAlways("MQ2Navigation::InitializePlugin:DoKeybinds");
		mq2nav::keybinds::DoKeybinds();
		DebugSpewAlways("MQ2Navigation::InitializePlugin:all good");

		m_pEQDraw->Initialize();

		mq2nav::initialized_ = true;
	}
	return result;
}

void deinit()
{
	if (mq2nav::initialized_) {
		mq2nav::keybinds::UndoKeybinds();
		mq2nav::Stop();
		mq2nav::initialized_ = false;
	}
}

} // namespace mq2nav

PLUGIN_API VOID OnPulse(VOID)
{
	if (!mmo.Active)
		return;
	if (mq2nav::util::ValidIngame(TRUE)) {
		mq2nav::AttemptMovement();
		mq2nav::StuckCheck();
		mq2nav::AttemptClick();
	}
}

PLUGIN_API VOID OnBeginZone(VOID)
{
	mq2nav::m_navMesh = NULL;
	mq2nav::waypoints::LoadWaypoints();
}

PLUGIN_API VOID OnEndZone(VOID)
{
	pDoorTarget = NULL;
	pGroundTarget = NULL;
	mq2nav::m_navMesh = NULL;
}

PLUGIN_API VOID SetGameState(DWORD GameState)
{
	if (GameState == GAMESTATE_INGAME) {
		mq2nav::init();
		mq2nav::LoadNavMesh();
		mq2nav::waypoints::LoadWaypoints();
	}
}

void PluginOn()
{
	AddCommand("/navigate", mq2nav::NavigateCommand);
	mq2nav::navigationType_ = new mq2nav::MQ2NavigationType;
	AddMQ2Data("Navigation", mq2nav::dataNavigation);

	if (gGameState == GAMESTATE_INGAME) {
		SetGameState(gGameState);
	}
}

void PluginOff()
{
	//Stop();
	RemoveCommand("/navigate");
	delete mq2nav::navigationType_;
	mq2nav::deinit();
	RemoveMQ2Data("Navigation");
}

PLUGIN_API VOID InitializePlugin(VOID)
{
	DebugSpewAlways("Initializing MQ2Navigation");
	sprintf(_DEBUG_LOG_FILE, "%s\\%s_Debug.log", gszLogPath, PLUGIN_NAME);
	PluginOn();
}

// Called once, when the plugin is to shutdown
PLUGIN_API VOID ShutdownPlugin(VOID)
{
	DebugSpewAlways("Shutting down MQ2Navigation");
	PluginOff();
}

