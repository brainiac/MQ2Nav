//
// MQ2Nav_Settings.h
//

#pragma once

#include "MQ2Navigation.h"

namespace mq2nav {

struct SettingsData
{
	// auto break navigation if keyboard input is received
	bool autobreak = false;

	// auto pause navigation if keyboard input is received
	bool autopause = false;

	// auto reload navmesh if file changes
	bool autoreload = true;

	// show the MQ2Nav Tools debug ui
	bool show_ui = true;

	// show the navmesh overlay
	bool show_navmesh_overlay = false;

	// show the current navigation path
	bool show_nav_path = true;

	// attempt to get unstuck
	bool attempt_unstuck = false;

	// use floor height for spawn z axis
	bool use_spawn_floor_height = true;

	// use custom extents in findNearestPoly
	bool use_find_polygon_extents = false;
	glm::vec3 find_polygon_extents{ 2, 4, 2 };

	// render pathing 3d debugging
	bool debug_render_pathing = false;

	// use corridor method (buggy, not finished...)
	bool debug_use_pathing_corridor = false;
};
SettingsData& GetSettings();

// Load settings from the .ini file
void LoadSettings(bool showMessage = false);

// Save settings to the .ini file
void SaveSettings(bool showMessage = false);

} // namespace mq2nav
