//
// Waypoints.h
//

#pragma once

#include "plugin/MQ2Navigation.h"

#include <glm/vec3.hpp>

#include <string>
#include <map>
#include <sstream>

namespace nav {

// ----------------------------------------

class Waypoint
{
public:
	Waypoint() {}

	Waypoint(const std::string& name_, const glm::vec3& location_, const std::string& description_)
		: location(location_), description(description_), name(name_) {}

	std::string Serialize() const;
	bool Deserialize(const std::string& name, const std::string& data);

	glm::vec3 location = { 0, 0, 0 };
	std::string name;
	std::string description;
};

// Load/Save waypoints from .ini file
void LoadWaypoints(int zoneId);

// Returns true and fills in wp if waypoint with name is found
bool GetWaypoint(const std::string& name, Waypoint& wp);

// Add a new waypoint to the current zone
bool AddWaypoint(const Waypoint& waypoint);

void RenderWaypointsUI();

using Waypoints = std::vector<Waypoint>;

extern Waypoints g_waypoints;

} // namespace nav
