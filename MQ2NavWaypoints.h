// MQ2NavSettings.h : Defines the entry point for the DLL application.
//

// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup, do NOT do it in DllMain.

#ifndef __MQ2NAVWAYPOINTS_H
#define __MQ2NAVWAYPOINTS_H


#include "../MQ2Plugin.h"
#include "MQ2Navigation.h"

#include <string>
#include <map>
#include <sstream>

namespace mq2nav {
namespace waypoints {
	// ----------------------------------------

	union Coordinates{
		FLOAT array[3];
		struct V3F{
			FLOAT X;
			FLOAT Y;
			FLOAT Z;
		};
		V3F v;
		Coordinates() {
			memset(this, 0, sizeof(Coordinates));
		}
	};

	struct Waypoint {

		Waypoint()
			:	location(),
				tag() {}

		Waypoint(const Coordinates& p_location, const std::string& p_tag)
			:	location(p_location),
				tag(p_tag) {}

		std::string getDescription() const {
			std::ostringstream ostr;
			ostr << location.v.X << " " << location.v.Y << " " << location.v.Z << " ";
			ostr << tag;
			return ostr.str();
		}
		bool readFromDescription(const std::string& description) {
			//CHAR parseTag[256];
			//FLOAT x, y, z;
			//bool result = (4 == sscanf_s(description.c_str(), "%f %f %f %s", &x, &y, &z, parseTag));
			//if (result) {
			//	tag = string(parseTag);
			//	location.v.X = x;
			//	location.v.Y = y;
			//	location.v.Z = z;
			//}
			bool result = false;
			std::istringstream istr(description);
			istr >> std::skipws;
			do {
				FLOAT x, y, z;
				istr >> x;
				if (istr.bad()) break;
				istr >> y;
				if (istr.bad()) break;
				istr >> z;
				if (istr.bad()) break;
				std::string readTag;
				istr >> readTag;

				result = true;
				tag = readTag;
				location.v.X = x;
				location.v.Y = y;
				location.v.Z = z;
			} while(false);

			return result;
		}

		Coordinates location;
		std::string tag;
	};

	typedef std::map<std::string, Waypoint> tZoneData;
	tZoneData currentZoneData_; 



	void LoadWaypoints() {
		PCHAR currentZone = GetShortZone(GetCharInfo()->zoneId);
		WriteChatf("[MQ2Navigation] loading waypoints for zone: %s ...",currentZone);
		currentZoneData_.clear();

		CHAR pchKeys[MAX_STRING*10]={0};
		CHAR pchValue[MAX_STRING];
		Waypoint wp;
		if(GetPrivateProfileString(currentZone, NULL, "" , pchKeys, MAX_STRING * 10, INIFileName)) {
			PCHAR pKeys=pchKeys; 
			while(pKeys[0]) {
				GetPrivateProfileString(currentZone, pKeys, "", pchValue, MAX_STRING, INIFileName);
				WriteChatf("[MQ2Navigation] found waypoint entry: %s -> %s", pKeys, pchValue);
				if(0 != pchValue[0] && wp.readFromDescription(std::string(pchValue))) {
					currentZoneData_[std::string(pKeys)] = wp;
				} else {
					WriteChatf("[MQ2Navigation] invalid waypoint entry: %s", pKeys);
				}
				pKeys += strlen(pKeys)+1;
			}
		}
	}

	void SaveWaypoints() {
		PCHAR currentZone = GetShortZone(GetCharInfo()->zoneId);
		WriteChatf("[MQ2Navigation] saving waypoints for zone: %s ...",currentZone);
		for (tZoneData::const_iterator it = currentZoneData_.begin(), end =currentZoneData_.end();
			it != end; ++it) {
			WritePrivateProfileString(currentZone,   it->first.c_str(), it->second.getDescription().c_str(),   INIFileName);
		}
	}

	std::pair<bool, Waypoint> getWaypoint(const std::string& name) {

		tZoneData::iterator findIt = currentZoneData_.find(name);
		bool result = (findIt != currentZoneData_.end());

		return std::make_pair(result, result ? findIt->second : Waypoint());
	}

	// returns true if waypoint was replaced, false if it was inserted
	bool addWaypoint(const std::string& name, const std::string& tag) {
		bool result = true;
		_SPAWNINFO* pCharacterSpawn = GetCharInfo() ? GetCharInfo()->pSpawn : NULL;
		if (NULL != pCharacterSpawn) {
			// get coords
			Coordinates playerLoc;
			playerLoc.v.X = pCharacterSpawn->X;
			playerLoc.v.Y = pCharacterSpawn->Y;
			playerLoc.v.Z = pCharacterSpawn->Z;
			Waypoint wp(playerLoc, tag);
			// store them
			tZoneData::iterator findIt = currentZoneData_.find(name);
			result = (findIt != currentZoneData_.end());
			if (result) {
				findIt->second = wp;
			} else {
				currentZoneData_.insert(std::make_pair(name, wp));
			}
			// save data
			SaveWaypoints();
		}
		return result;
	}

}
} // namespace mq2nav {


#endif