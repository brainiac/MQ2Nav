//
// Waypoints.cpp
//

#include "Waypoints.h"

#include <imgui.h>

namespace mq2nav {

//----------------------------------------------------------------------------

typedef std::vector<Waypoint> Waypoints;
Waypoints g_waypoints;

std::string Waypoint::Serialize() const
{
	std::ostringstream ostr;

	ostr << location.x << " " << location.y << " " << location.z << " ";
	ostr << description;

	return ostr.str();
}

bool Waypoint::Deserialize(const std::string& name, const std::string& data)
{
	std::istringstream istr(data);
	istr >> std::skipws;

	glm::vec3 pos;
	
	istr >> pos.x;
	if (istr.bad()) return false;
	
	istr >> pos.y;
	if (istr.bad()) return false;
	
	istr >> pos.z;
	if (istr.bad()) return false;

	istr >> this->description;
	this->location = pos;

	this->name = name;

	return true;
}

//----------------------------------------------------------------------------

void LoadWaypoints(int zoneId)
{
	PCHAR currentZone = GetShortZone(zoneId);

	WriteChatf(PLUGIN_MSG "Loading waypoints for zone: %s", currentZone);
	g_waypoints.clear();

	CHAR pchKeys[MAX_STRING * 10] = { 0 };
	CHAR pchValue[MAX_STRING];
	Waypoint wp;

	if (GetPrivateProfileString(currentZone, NULL, "", pchKeys, MAX_STRING * 10, INIFileName))
	{
		PCHAR pKeys = pchKeys;
		while (pKeys[0])
		{
			GetPrivateProfileString(currentZone, pKeys, "", pchValue, MAX_STRING, INIFileName);

			std::string name(pKeys);

			bool exists = std::find_if(g_waypoints.begin(), g_waypoints.end(),
				[&name](const Waypoint& wp) { return wp.name == name; }) != g_waypoints.end();

			if (!exists && (pchValue[0] != 0) && wp.Deserialize(name, pchValue))
			{
				g_waypoints.emplace_back(std::move(wp));
			}
			else {
				WriteChatf(PLUGIN_MSG "Invalid waypoint entry: %s", pKeys);
			}
			pKeys += strlen(pKeys) + 1;
		}
	}

	std::sort(g_waypoints.begin(), g_waypoints.end(),
		[](const Waypoint& a, const Waypoint& b)
	{
		return strcmp(a.name.c_str(), b.name.c_str()) < 0;
	});
}

void SaveWaypoints(int zoneId)
{
	PCHAR currentZone = GetShortZone(zoneId);

	for (const auto& elem : g_waypoints)
	{
		std::string& serialized = elem.Serialize();

		WritePrivateProfileString(currentZone, elem.name.c_str(),
			serialized.c_str(), INIFileName);
	}
}

bool GetWaypoint(const std::string& name, Waypoint& wp)
{
	auto iter = std::find_if(g_waypoints.begin(), g_waypoints.end(),
		[&name](const Waypoint& wp) { return wp.name == name; });

	bool result = (iter != g_waypoints.end());

	wp = result ? *iter : Waypoint();
	return result;
}

bool AddWaypoint(Waypoint waypoint)
{
	bool exists = false;

	auto iter = std::find_if(g_waypoints.begin(), g_waypoints.end(),
		[&waypoint](const Waypoint& wp) { return wp.name == waypoint.name; });

	// store them
	if (iter == g_waypoints.end())
	{
		g_waypoints.emplace_back(std::move(waypoint));
	}
	else
	{
		*iter = std::move(waypoint);
		exists = true;
	}

	// todo, store this globally
	int zoneId = GetCharInfo()->zoneId;
	zoneId = zoneId & 0x7fff;

	std::sort(g_waypoints.begin(), g_waypoints.end(),
		[](const Waypoint& a, const Waypoint& b)
	{
		return strcmp(a.name.c_str(), b.name.c_str()) < 0;
	});

	// save data
	SaveWaypoints(zoneId);

	return exists;
}

// returns true if waypoint was replaced, false if it was inserted
bool AddWaypoint(const std::string& name, const std::string& description)
{
	bool exists = false;
	PSPAWNINFO pCharacterSpawn = GetCharInfo() ? GetCharInfo()->pSpawn : NULL;

	if (pCharacterSpawn != nullptr)
	{
		// get coords
		glm::vec3 playerLoc = { pCharacterSpawn->X, pCharacterSpawn->Y, pCharacterSpawn->Z };

		AddWaypoint(Waypoint{ name, playerLoc, description });
	}

	return exists;
}

static bool WaypointItemGetter(void* data, int index, const char** out_text)
{
	if (index < g_waypoints.size())
	{
		*out_text = g_waypoints[index].name.c_str();
		return true;
	}

	return false;
}

void RenderWaypointsUI()
{
	PSPAWNINFO pCharacterSpawn = GetCharInfo() ? GetCharInfo()->pSpawn : nullptr;
	if (!pCharacterSpawn)
		return;

	static int currentWaypoint = 0;
	static Waypoint s_newWaypoint;
	static char newWaypointName[256];
	static char newWaypointDescription[256];

	int waypoints = mq2nav::g_waypoints.size();

	ImGui::Text("Select a waypoint");
	bool changed = ImGui::Combo("Waypoint", &currentWaypoint,
		WaypointItemGetter, nullptr, g_waypoints.size());
	
	ImGui::Separator();

	if (changed && currentWaypoint < g_waypoints.size())
	{
		s_newWaypoint = g_waypoints[currentWaypoint];
		strcpy_s(newWaypointName, s_newWaypoint.name.c_str());
		strcpy_s(newWaypointDescription, s_newWaypoint.description.c_str());
	}

	if (ImGui::InputText("Name", newWaypointName, 256))
		s_newWaypoint.name = newWaypointName;
	ImGui::InputFloat3("Location", &s_newWaypoint.location.x);
	if (ImGui::InputText("Description", newWaypointDescription, 256))
		s_newWaypoint.description = newWaypointDescription;

	if (ImGui::Button("Use Current Loc")) {
		s_newWaypoint.location = { pCharacterSpawn->X, pCharacterSpawn->Y, pCharacterSpawn->Z };
	}
	ImGui::SameLine();
	if (ImGui::Button("New")) {
		s_newWaypoint = Waypoint();
		newWaypointName[0] = 0;
		newWaypointDescription[0] = 0;
	}
	ImGui::SameLine();
	if (ImGui::Button("Save")) {
		if (s_newWaypoint.name.length() > 0)
		{
			AddWaypoint(s_newWaypoint);

			for (int i = 0; i < g_waypoints.size(); i++)
			{
				if (g_waypoints[i].name == s_newWaypoint.name) {
					currentWaypoint = i;
					break;
				}
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Navigate")) {
		if (s_newWaypoint.name.length() > 0)
		{
			g_mq2Nav->BeginNavigation(s_newWaypoint.location);
		}
	}
}

} // namespace mq2nav
