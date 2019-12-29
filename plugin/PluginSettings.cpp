//
// PluginSettings.cpp
//

#include "pch.h"
#include "PluginSettings.h"

#include "plugin/MQ2Navigation.h"
#include "plugin/NavigationPath.h"

#include <boost/lexical_cast.hpp>
#include <spdlog/spdlog.h>

namespace nav {

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

static inline glm::vec3 LoadVec3Setting(const std::string& name, const glm::vec3& defaultValue)
{
	try
	{
		char szTemp[40] = { 0 };
		glm::vec3 value;

		GetPrivateProfileString("Settings", (name + "X").c_str(),
			boost::lexical_cast<std::string>(defaultValue.x).c_str(), szTemp, 40, INIFileName);
		value.x = boost::lexical_cast<float>(szTemp);

		GetPrivateProfileString("Settings", (name + "Y").c_str(),
			boost::lexical_cast<std::string>(defaultValue.y).c_str(), szTemp, 40, INIFileName);
		value.y = boost::lexical_cast<float>(szTemp);

		GetPrivateProfileString("Settings", (name + "Z").c_str(),
			boost::lexical_cast<std::string>(defaultValue.z).c_str(), szTemp, 40, INIFileName);
		value.z = boost::lexical_cast<float>(szTemp);

		return value;
	}
	catch (const boost::bad_lexical_cast&)
	{
		return defaultValue;
	}
}

template <typename T>
static inline T LoadNumberSetting(const std::string& name, T defaultValue)
{
	T value = T{};

	try
	{
		char szTemp[40] = { 0 };

		GetPrivateProfileString("Settings", name.c_str(),
			boost::lexical_cast<std::string>(defaultValue).c_str(), szTemp, 40, INIFileName);

		value = boost::lexical_cast<T>(szTemp);
	}
	catch (const boost::bad_lexical_cast&) {}

	return value;
}

static inline void SaveBoolSetting(const std::string& name, bool value)
{
	WritePrivateProfileString("Settings", name.c_str(), value ? "on" : "off", INIFileName);
}

template <typename T>
static inline void SaveNumberSetting(const std::string& name, T value)
{
	WritePrivateProfileString("Settings", name.c_str(), std::to_string(value).c_str(), INIFileName);
}

static inline void SaveVec3Setting(const std::string& name, const glm::vec3& value)
{
	WritePrivateProfileString("Settings", (name + "X").c_str(),
		boost::lexical_cast<std::string>(value.x).c_str(), INIFileName);
	WritePrivateProfileString("Settings", (name + "Y").c_str(),
		boost::lexical_cast<std::string>(value.y).c_str(), INIFileName);
	WritePrivateProfileString("Settings", (name + "Z").c_str(),
		boost::lexical_cast<std::string>(value.z).c_str(), INIFileName);
}

void LoadSettings(bool showMessage/* = true*/)
{
	if (showMessage)
	{
		SPDLOG_INFO("Loading settings...");
	}

	SettingsData defaults, &settings = g_settings;

	settings.autobreak = LoadBoolSetting("AutoBreak", defaults.autobreak);
	settings.autopause = LoadBoolSetting("AutoPause", defaults.autopause);
	settings.autoreload = LoadBoolSetting("AutoReload", defaults.autoreload);
	settings.show_ui = LoadBoolSetting("ShowUI", defaults.show_ui);
	settings.show_nav_path = LoadBoolSetting("ShowNavPath", defaults.show_nav_path);
	settings.attempt_unstuck = LoadBoolSetting("AttemptUnstuck", defaults.attempt_unstuck);
	settings.use_spawn_floor_height = LoadBoolSetting("UseSpawnFloorHeight", defaults.use_spawn_floor_height);

	settings.use_find_polygon_extents = LoadBoolSetting("UseFindPolygonExtents", defaults.use_find_polygon_extents);
	settings.find_polygon_extents = LoadVec3Setting("FindPolygonExtents", defaults.find_polygon_extents);

	settings.map_line_enabled = LoadBoolSetting("MapLineEnabled", defaults.map_line_enabled);
	settings.map_line_color = LoadNumberSetting("MapLineColor", defaults.map_line_color);
	settings.map_line_layer = LoadNumberSetting("MapLineLayer", defaults.map_line_layer);

	settings.open_doors = LoadBoolSetting("OpenDoors", defaults.open_doors);
	settings.ignore_scripted_doors = LoadBoolSetting("IgnoreScriptedDoors", defaults.ignore_scripted_doors);

	// debug settings
	settings.debug_render_pathing = LoadBoolSetting("DebugRenderPathing", defaults.debug_render_pathing);

	// navigation line settings
	NavigationLine::LineStyle defaultLineStyle;
	gNavigationLineStyle.borderColor = LoadNumberSetting<uint32_t>("VisualNavPathBorderColor", defaultLineStyle.borderColor);
	gNavigationLineStyle.hiddenColor = LoadNumberSetting<uint32_t>("VisualNavPathHiddenColor", defaultLineStyle.hiddenColor);
	gNavigationLineStyle.visibleColor = LoadNumberSetting<uint32_t>("VisualNavPathVisibleColor", defaultLineStyle.visibleColor);
	gNavigationLineStyle.linkColor = LoadNumberSetting<uint32_t>("VisualNavPathLinkColor", defaultLineStyle.linkColor);
	gNavigationLineStyle.opacity = LoadNumberSetting<float>("VisualNavPathVisibleOpacity", defaultLineStyle.opacity);
	gNavigationLineStyle.hiddenOpacity = LoadNumberSetting<float>("VisualNavPathHiddenOpacity", defaultLineStyle.hiddenOpacity);
	gNavigationLineStyle.borderWidth = LoadNumberSetting<float>("VisualNavPathBorderWidth", defaultLineStyle.borderWidth);
	gNavigationLineStyle.lineWidth = LoadNumberSetting<float>("VisualNavPathLineWidth", defaultLineStyle.lineWidth);
}

void SaveSettings(bool showMessage/* = true*/)
{
	if (showMessage)
	{
		SPDLOG_INFO("Saving settings...");
	}

	// default settings
	SaveBoolSetting("AutoBreak", g_settings.autobreak);
	SaveBoolSetting("AutoPause", g_settings.autopause);
	SaveBoolSetting("AutoReload", g_settings.autoreload);
	SaveBoolSetting("ShowUI", g_settings.show_ui);
	SaveBoolSetting("ShowNavPath", g_settings.show_nav_path);
	SaveBoolSetting("UseSpawnFloorHeight", g_settings.use_spawn_floor_height);

	SaveBoolSetting("UseFindPolygonExtents", g_settings.use_find_polygon_extents);
	SaveVec3Setting("FindPolygonExtents", g_settings.find_polygon_extents);

	SaveBoolSetting("OpenDoors", g_settings.open_doors);
	SaveBoolSetting("IgnoreScriptedDoors", g_settings.ignore_scripted_doors);

	SaveBoolSetting("MapLineEnabled", g_settings.map_line_enabled);
	SaveNumberSetting("MapLineColor", g_settings.map_line_color);
	SaveNumberSetting("MapLineLayer", g_settings.map_line_layer);

	// in game nave path
	SaveNumberSetting<uint32_t>("VisualNavPathBorderColor", gNavigationLineStyle.borderColor);
	SaveNumberSetting<uint32_t>("VisualNavPathHiddenColor", gNavigationLineStyle.hiddenColor);
	SaveNumberSetting<uint32_t>("VisualNavPathVisibleColor", gNavigationLineStyle.visibleColor);
	SaveNumberSetting<uint32_t>("VisualNavPathLinkColor", gNavigationLineStyle.linkColor);
	SaveNumberSetting<float>("VisualNavPathVisibleOpacity", gNavigationLineStyle.opacity);
	SaveNumberSetting<float>("VisualNavPathHiddenOpacity", gNavigationLineStyle.hiddenOpacity);
	SaveNumberSetting<float>("VisualNavPathBorderWidth", gNavigationLineStyle.borderWidth);
	SaveNumberSetting<float>("VisualNavPathLineWidth", gNavigationLineStyle.lineWidth);

	// debug settings
	SaveBoolSetting("DebugRenderPathing", g_settings.debug_render_pathing);
}

bool ParseIniCommand(const char* command)
{
	char szKeyName[MAX_STRING];
	char szValue[MAX_STRING];

	GetArg(szKeyName, command, 1);
	GetArg(szValue, command, 2);

	if (szKeyName[0] != 0 && szValue[0] != 0)
	{
		WritePrivateProfileStringA("Settings", szKeyName, szValue, INIFileName);

		// cycle settings to clear any wierd inputs
		LoadSettings(false);
		SaveSettings(false);

		return true;
	}

	return false;
}

bool ReadIniSetting(const char* keyName, char* pBuffer, size_t length)
{
	return GetPrivateProfileString("Settings", keyName, "", pBuffer, length, INIFileName) != 0;
}

//----------------------------------------------------------------------------

} // namespace nav
