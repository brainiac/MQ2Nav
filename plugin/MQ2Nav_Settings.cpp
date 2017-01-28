//
// MQ2Nav_Settings.cpp
//

#include "MQ2Nav_Settings.h"

namespace mq2nav {

//----------------------------------------------------------------------------

SettingsData g_settings;

SettingsData& GetSettings()
{
	return g_settings;
}

static inline bool LoadBoolSetting(const std::string& name, bool default)
{
	char szTemp[MAX_STRING] = { 0 };

	GetPrivateProfileString("Settings", name.c_str(), default ? "on" : "off",
		szTemp, MAX_STRING, INIFileName);
	return (!_strnicmp(szTemp, "on", 3));
}

static inline void SaveBoolSetting(const std::string& name, bool value)
{
	WritePrivateProfileString("Settings", name.c_str(), value ? "on" : "off", INIFileName);
}

void LoadSettings(bool showMessage/* = true*/)
{
	if (showMessage)
	{
		WriteChatf(PLUGIN_MSG "Loading settings...");
	}

	SettingsData defaults, &settings = g_settings;

	settings.autobreak = LoadBoolSetting("AutoBreak", defaults.autobreak);
	settings.autopause = LoadBoolSetting("AutoPause", defaults.autopause);
	settings.autoreload = LoadBoolSetting("AutoReload", defaults.autoreload);
	settings.show_ui = LoadBoolSetting("ShowUI", defaults.show_ui);
	settings.show_navmesh_overlay = LoadBoolSetting("ShowNavMesh", defaults.show_navmesh_overlay);
	settings.show_nav_path = LoadBoolSetting("ShowNavPath", defaults.show_nav_path);
	settings.attempt_unstuck = LoadBoolSetting("AttemptUnstuck", defaults.attempt_unstuck);

	// debug settings
	settings.debug_render_pathing = LoadBoolSetting("DebugRenderPathing", defaults.debug_render_pathing);

	SaveSettings(false);
}

void SaveSettings(bool showMessage/* = true*/)
{
	if (showMessage)
	{
		WriteChatf(PLUGIN_MSG "Saving settings...");
	}

	// default settings
	SaveBoolSetting("AutoBreak", g_settings.autobreak);
	SaveBoolSetting("AutoPause", g_settings.autopause);
	SaveBoolSetting("AutoReload", g_settings.autoreload);
	SaveBoolSetting("ShowUI", g_settings.show_ui);
	SaveBoolSetting("ShowNavMesh", g_settings.show_navmesh_overlay);
	SaveBoolSetting("ShowNavPath", g_settings.show_nav_path);

	// debug settings
	SaveBoolSetting("DebugRenderPathing", g_settings.debug_render_pathing);
}

//----------------------------------------------------------------------------

} // namespace mq2nav
