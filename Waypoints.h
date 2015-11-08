//
// Waypoints.h
//

#pragma once

#include "MQ2Navigation.h"

#include <glm/vec3.hpp>

#include <string>
#include <map>
#include <sstream>

namespace mq2nav {

// ----------------------------------------

class Waypoint
{
public:
	Waypoint() {}

	Waypoint(const glm::vec3& location_, const std::string& tag_)
		: location(location_), tag(tag_) {}

	std::string getDescription() const;

	bool readFromDescription(const std::string& description);

	glm::vec3 location = { 0, 0, 0 };
	std::string tag;
};

// Load/Save waypoints from .ini file
void LoadWaypoints(int zoneId);
void SaveWaypoints(int zoneId);

// Returns true and fills in wp if waypoint with name is found
bool GetWaypoint(const std::string& name, Waypoint& wp);

// returns true if waypoint was replaced, false if it was inserted
bool AddWaypoint(const std::string& name, const std::string& tag);

} // namespace mq2nav
