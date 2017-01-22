//
// Waypoints.cpp
//

#include "Waypoints.h"

#include <imgui.h>

namespace mq2nav {

//----------------------------------------------------------------------------

typedef std::vector<Waypoint> Waypoints;
Waypoints g_waypoints;
std::string g_zoneName;
std::string g_shortZone;
int g_currentZone = 0;

bool DeleteWaypoint(const std::string& name);

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
	g_shortZone = GetShortZone(zoneId);
	g_zoneName = GetFullZone(zoneId);
	g_currentZone = zoneId;

	WriteChatf(PLUGIN_MSG "Loading waypoints for zone: %s", g_shortZone.c_str());
	g_waypoints.clear();

	CHAR pchKeys[MAX_STRING * 10] = { 0 };
	CHAR pchValue[MAX_STRING];
	Waypoint wp;

	if (GetPrivateProfileString(g_shortZone.c_str(), NULL, "", pchKeys, MAX_STRING * 10, INIFileName))
	{
		PCHAR pKeys = pchKeys;
		while (pKeys[0] || pKeys[1])
		{
			GetPrivateProfileString(g_shortZone.c_str(), pKeys, "", pchValue, MAX_STRING, INIFileName);

			std::string name(pKeys);
			bool valid = true;

			// empty/invalid character, delete it
			if (name.empty())
			{
				valid = false;
			}
			else
			{
				bool exists = std::find_if(g_waypoints.begin(), g_waypoints.end(),
					[&name](const Waypoint& wp) { return wp.name == name; }) != g_waypoints.end();

				if (!exists && (pchValue[0] != 0) && wp.Deserialize(name, pchValue))
				{
					g_waypoints.push_back(wp);
				}
				else
				{
					WriteChatf(PLUGIN_MSG "Invalid waypoint entry: %s", pKeys);
					valid = false;
				}
			}

			if (!valid)
			{
				DeleteWaypoint(name);
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

void SaveWaypoint(const Waypoint& wp)
{
	std::string& serialized = wp.Serialize();

	WritePrivateProfileString(g_shortZone.c_str(), wp.name.c_str(),
		serialized.c_str(), INIFileName);
}

bool GetWaypoint(const std::string& name, Waypoint& wp)
{
	auto iter = std::find_if(g_waypoints.begin(), g_waypoints.end(),
		[&name](const Waypoint& wp) { return wp.name == name; });

	bool result = (iter != g_waypoints.end());

	wp = result ? *iter : Waypoint();
	return result;
}

bool DeleteWaypoint(const std::string& name)
{
	auto iter = std::find_if(g_waypoints.begin(), g_waypoints.end(),
		[&name](const Waypoint& wp) { return wp.name == name; });

	if (iter == g_waypoints.end())
		return false;

	g_waypoints.erase(iter);

	WritePrivateProfileString(g_shortZone.c_str(), name.c_str(), NULL, INIFileName);
	return true;
}

bool AddWaypoint(const Waypoint& waypoint)
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

	std::sort(g_waypoints.begin(), g_waypoints.end(),
		[](const Waypoint& a, const Waypoint& b)
	{
		return strcmp(a.name.c_str(), b.name.c_str()) < 0;
	});

	// save data
	SaveWaypoint(waypoint);

	return exists;
}

void DrawSplitter(bool split_vertically, float thickness, float* size0, float* size1, float min_size0, float min_size1)
{
	ImVec2 backup_pos = ImGui::GetCursorPos();
	if (split_vertically)
		ImGui::SetCursorPosY(backup_pos.y + *size0);
	else
		ImGui::SetCursorPosX(backup_pos.x + *size0);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));          // We don't draw while active/pressed because as we move the panes the splitter button will be 1 frame late
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.10f));
	ImGui::Button("##Splitter", ImVec2(!split_vertically ? thickness : -1.0f, split_vertically ? thickness : -1.0f));
	ImGui::PopStyleColor(3);

	ImGui::SetItemAllowOverlap(); // This is to allow having other buttons OVER our splitter. 

	if (ImGui::IsItemActive())
	{
		float mouse_delta = split_vertically ? ImGui::GetIO().MouseDelta.y : ImGui::GetIO().MouseDelta.x;

		// Minimum pane size
		if (mouse_delta < min_size0 - *size0)
			mouse_delta = min_size0 - *size0;
		if (mouse_delta > *size1 - min_size1)
			mouse_delta = *size1 - min_size1;

		// Apply resize
		*size0 += mouse_delta;
		*size1 -= mouse_delta;
	}
	ImGui::SetCursorPos(backup_pos);
}

bool ColoredButton(const char* text, const ImVec2& size, float hue)
{
	ImGui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(hue, 0.6f, 0.6f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(hue, 0.7f, 0.7f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(hue, 0.8f, 0.8f));
	bool clicked = ImGui::Button(text, size);
	ImGui::PopStyleColor(3);
	return clicked;
}

void RenderWaypointsUI()
{
	PSPAWNINFO pCharacterSpawn = GetCharInfo() ? GetCharInfo()->pSpawn : nullptr;
	if (!pCharacterSpawn)
		return;

	static int currentWaypoint = -1;

	static Waypoint editWaypoint;
	static char editWaypointName[256];
	static char editWaypointDescription[256];

	static bool changedForNext = false;

	bool changed = false;
	int waypoints = mq2nav::g_waypoints.size();

	// initialize current waypoint
	if (currentWaypoint == -1) {
		currentWaypoint = 0;
		changed = true;
	}
	if (changedForNext) {
		changed = true;
		changedForNext = false;
	}

	ImGui::Text("Waypoints for");
	ImGui::SameLine(); ImGui::TextColored(ImColor(66, 244, 110), "%s", g_zoneName.c_str());

	ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - 100);
	if (ImGui::Button("New", ImVec2(100, 0))) {
		editWaypoint = Waypoint();
		editWaypointName[0] = 0;
		editWaypointDescription[0] = 0;
	}

	ImVec2 availSize = ImGui::GetContentRegionAvail();

	static float leftPaneSize = 150.0;
	static float rightPaneSize = availSize.x - leftPaneSize - 1;
	DrawSplitter(false, 5.0, &leftPaneSize, &rightPaneSize, 50, 250);

	// the current waypoint being edited

	// Left Pane
	{
		ImGui::BeginChild("WaypointsList", ImVec2(leftPaneSize, ImGui::GetContentRegionAvail().y - 1), true);
		ImGuiListClipper clipper(g_waypoints.size(), ImGui::GetTextLineHeightWithSpacing());
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				ImGui::PushID(i);
				const bool item_selected = (i == currentWaypoint);
				if (ImGui::Selectable(g_waypoints[i].name.c_str(), item_selected))
				{
					currentWaypoint = i;
					changed = true;
				}
				ImGui::PopID();
			}
		}

		ImGui::EndChild();
	} // end of LeftPane

	ImGui::SameLine();

	if (changed)
	{
		editWaypoint = (currentWaypoint < g_waypoints.size())
			? g_waypoints[currentWaypoint] : Waypoint();

		strcpy_s(editWaypointName, editWaypoint.name.c_str());
		strcpy_s(editWaypointDescription, editWaypoint.description.c_str());
	}

	// Right Pane
	{
		ImVec2 rightPaneContentSize = ImGui::GetContentRegionAvail();
		ImGui::BeginChild("WaypointsView", ImVec2(rightPaneContentSize.x, rightPaneContentSize.y - 5));
		ImGui::BeginChild("TopPart", ImVec2(0, -(ImGui::GetItemsLineHeightWithSpacing() + 1)));

		ImGui::Text("Waypoint Name");
		ImGui::PushItemWidth(-1);
		ImGui::InputText("##Name", editWaypointName, 256);
		ImGui::PopItemWidth();

		ImGui::Text("Position");
		ImGui::PushItemWidth(-44);
		glm::vec3 tempPos = editWaypoint.location;
		std::swap(tempPos.x, tempPos.y);
		ImGui::InputFloat3("##Location", &tempPos.x, 2);
		ImGui::PopItemWidth();

		ImGui::SameLine(ImGui::GetWindowWidth() - 41);
		if (ImGui::Button("/loc", ImVec2(40, 0))) {
			editWaypoint.location = { pCharacterSpawn->X, pCharacterSpawn->Y, pCharacterSpawn->Z };
		}

		ImGui::Text("Description");
		ImGui::PushItemWidth(-1);
		if (ImGui::InputTextMultiline("##Description", editWaypointDescription, 256,
			ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 3)))
		{
			editWaypoint.description = editWaypointDescription;
		}
		ImGui::PopItemWidth();

		if (ImGui::Button("Navigate to Waypoint"))
		{
			if (editWaypoint.name.length() > 0)
			{
				std::string command = "waypoint \"" + editWaypoint.name + "\"";

				auto info = ParseDestination(command.c_str(), NotifyType::None);
				g_mq2Nav->BeginNavigation(info);
			}
		}

		ImGui::EndChild(); // end of TopPart

		// Bottom of Right Pane
		//

		if (ColoredButton("Save", ImVec2(60, 0), 0.28))
		{
			std::string newName = editWaypointName;
			if (!newName.empty())
			{
				// TODO: Maybe handle renaming and erroring if waypoint already
				// exists. As this is written now it will overwrite an existing wp.

				// delete old waypoint if name is different.
				if (editWaypoint.name != newName && !editWaypoint.name.empty())
					DeleteWaypoint(editWaypoint.name);

				// save the new waypoint.
				editWaypoint.name = editWaypointName;
				editWaypoint.description = editWaypointDescription;
				AddWaypoint(editWaypoint);

				// Resync current index
				for (int i = 0; i < g_waypoints.size(); i++)
				{
					if (g_waypoints[i].name == newName) {
						currentWaypoint = i;
						changedForNext = true;
						break;
					}
				}
			}
		}
		ImGui::SameLine();

		if (ColoredButton("Delete", ImVec2(60, 0), 0.0))
		{
			if (DeleteWaypoint(editWaypoint.name))
			{
				while (currentWaypoint >= g_waypoints.size() && !g_waypoints.empty())
					currentWaypoint--;

				changedForNext = true;
			}
		}

		ImGui::EndChild();
	} // end of RightPane
}

} // namespace mq2nav
