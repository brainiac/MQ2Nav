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

	// auto reload navmesh if file changes
	bool autoreload = true;

	// show the MQ2Nav Tools debug ui
	bool show_ui = true;

	// show the navmesh overlay
	bool show_navmesh_overlay = false;

	// show the current navigation path
	bool show_nav_path = true;
};
SettingsData& GetSettings();

// Load settings from the .ini file
void LoadSettings(bool showMessage = false);

// Save settings to the .ini file
void SaveSettings(bool showMessage = false);

} // namespace mq2nav
