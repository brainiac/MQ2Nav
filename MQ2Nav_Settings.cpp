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

void LoadSettings(bool showMessage/* = true*/)
{
	if (showMessage)
	{
		WriteChatf(PLUGIN_MSG "Loading settings...");
	}

	SettingsData defaults;
	char szTemp[MAX_STRING] = { 0 };

	GetPrivateProfileString("Settings", "AutoBreak",
		defaults.autobreak ? "on" : "off",
		szTemp, MAX_STRING, INIFileName);
	g_settings.autobreak = (!strnicmp(szTemp, "on", 3));

	GetPrivateProfileString("Settings", "AutoPause",
		defaults.autopause ? "on" : "off",
		szTemp, MAX_STRING, INIFileName);
	g_settings.autopause = (!strnicmp(szTemp, "on", 3));

	GetPrivateProfileString("Settings", "AutoReload",
		defaults.autoreload ? "on" : "off",
		szTemp, MAX_STRING, INIFileName);
	g_settings.autoreload = (!strnicmp(szTemp, "on", 3));

	GetPrivateProfileString("Settings", "ShowUI",
		defaults.show_ui ? "on" : "off",
		szTemp, MAX_STRING, INIFileName);
	g_settings.show_ui = (!strnicmp(szTemp, "on", 3));

	GetPrivateProfileString("Settings", "ShowNavMesh",
		defaults.show_navmesh_overlay ? "on" : "off",
		szTemp, MAX_STRING, INIFileName);
	g_settings.show_navmesh_overlay = (!strnicmp(szTemp, "on", 3));

	GetPrivateProfileString("Settings", "ShowNavPath",
		defaults.show_nav_path ? "on" : "off",
		szTemp, MAX_STRING, INIFileName);
	g_settings.show_nav_path = (!strnicmp(szTemp, "on", 3));

	GetPrivateProfileString("Settings", "AttemptUnstuck",
		defaults.attempt_unstuck ? "on" : "off",
		szTemp, MAX_STRING, INIFileName);
	g_settings.attempt_unstuck = (!strnicmp(szTemp, "on", 3));

	SaveSettings(false);
}

void SaveSettings(bool showMessage/* = true*/)
{
	if (showMessage)
	{
		WriteChatf(PLUGIN_MSG "Saving settings...");
	}

	char szTemp[MAX_STRING] = { 0 };

	// default settings
	WritePrivateProfileString("Settings", "AutoBreak", g_settings.autobreak ? "on" : "off", INIFileName);
	WritePrivateProfileString("Settings", "AutoPause", g_settings.autopause ? "on" : "off", INIFileName);
	WritePrivateProfileString("Settings", "AutoReload", g_settings.autoreload ? "on" : "off", INIFileName);
	WritePrivateProfileString("Settings", "ShowUI", g_settings.show_ui ? "on" : "off", INIFileName);
	WritePrivateProfileString("Settings", "ShowNavMesh", g_settings.show_navmesh_overlay ? "on" : "off", INIFileName);
	WritePrivateProfileString("Settings", "ShowNavPath", g_settings.show_nav_path ? "on" : "off", INIFileName);

#if 0
	WritePrivateProfileString("Defaults",   "AllowMove",              ftoa(SET->AllowMove,      szTemp),     INIFileName);
	WritePrivateProfileString("Defaults",   "AutoPauseMsg",           (uiVerbLevel & V_AUTOPAUSE) == V_AUTOPAUSE             ? "on" : "off",      INIFileName);
	WritePrivateProfileString("Defaults",   "AutoSave",               SET->AutoSave        ? "on" : "off",   INIFileName);
	WritePrivateProfileString("Defaults",   "AutoUW",                 SET->AutoUW          ? "on" : "off",   INIFileName);
	WritePrivateProfileString("Defaults",   "BreakKeyboard",          SET->BreakKB         ? "on" : "off",   INIFileName);
	WritePrivateProfileString("Defaults",   "BreakMouse",             SET->BreakMouse      ? "on" : "off",   INIFileName);
#endif
}

//----------------------------------------------------------------------------

} // namespace mq2nav
