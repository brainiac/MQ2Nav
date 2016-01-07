//
// Waypoints.cpp
//

#include "Waypoints.h"

namespace mq2nav {

//----------------------------------------------------------------------------

std::string Waypoint::getDescription() const
{
	std::ostringstream ostr;

	ostr << location.x << " " << location.y << " " << location.z << " ";
	ostr << tag;

	return ostr.str();
}

bool Waypoint::readFromDescription(const std::string& description)
{
	std::istringstream istr(description);
	istr >> std::skipws;

	glm::vec3 pos;
	
	istr >> pos.x;
	if (istr.bad()) return false;
	
	istr >> pos.y;
	if (istr.bad()) return false;
	
	istr >> pos.z;
	if (istr.bad()) return false;

	istr >> this->tag;
	this->location = pos;

	return true;
}

//----------------------------------------------------------------------------

typedef std::map<std::string, Waypoint> tZoneData;
tZoneData g_currentZoneData;

void LoadWaypoints(int zoneId)
{
	PCHAR currentZone = GetShortZone(zoneId);
	WriteChatf(PLUGIN_MSG "Loading waypoints for zone: %s", currentZone);
	g_currentZoneData.clear();

	CHAR pchKeys[MAX_STRING * 10] = { 0 };
	CHAR pchValue[MAX_STRING];
	Waypoint wp;

	if (GetPrivateProfileString(currentZone, NULL, "", pchKeys, MAX_STRING * 10, INIFileName))
	{
		PCHAR pKeys = pchKeys;
		while (pKeys[0])
		{
			GetPrivateProfileString(currentZone, pKeys, "", pchValue, MAX_STRING, INIFileName);
			WriteChatf(PLUGIN_MSG "Waypoint: %s -> %s", pKeys, pchValue);
			if (0 != pchValue[0] && wp.readFromDescription(std::string(pchValue))) {
				g_currentZoneData[std::string(pKeys)] = wp;
			}
			else {
				WriteChatf(PLUGIN_MSG "Invalid waypoint entry: %s", pKeys);
			}
			pKeys += strlen(pKeys) + 1;
		}
	}
}

void SaveWaypoints(int zoneId)
{
	PCHAR currentZone = GetShortZone(zoneId);
	WriteChatf(PLUGIN_MSG "Saving waypoints for zone: %s", currentZone);

	for (const auto& elem : g_currentZoneData)
	{
		WritePrivateProfileString(currentZone, elem.first.c_str(),
			elem.second.getDescription().c_str(), INIFileName);
	}
}

bool GetWaypoint(const std::string& name, Waypoint& wp)
{
	auto findIt = g_currentZoneData.find(name);

	bool result = (findIt != g_currentZoneData.end());

	wp = result ? findIt->second : Waypoint();
	return result;
}

// returns true if waypoint was replaced, false if it was inserted
bool AddWaypoint(const std::string& name, const std::string& tag)
{
	bool result = true;
	PSPAWNINFO pCharacterSpawn = GetCharInfo() ? GetCharInfo()->pSpawn : NULL;

	if (pCharacterSpawn != nullptr)
	{
		// get coords
		glm::vec3 playerLoc = { pCharacterSpawn->X, pCharacterSpawn->Y, pCharacterSpawn->Z };
		Waypoint wp(playerLoc, tag);

		result = (g_currentZoneData.find(name) != g_currentZoneData.end());

		// store them
		g_currentZoneData[name] = std::move(wp);

		// todo, store this globally
		int zoneId = GetCharInfo()->zoneId;
		zoneId = zoneId & 0x7fff;

		// save data
		SaveWaypoints(zoneId);
	}

	return true;
}

} // namespace mq2nav
