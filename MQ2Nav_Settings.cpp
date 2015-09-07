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

void LoadSettings()
{
	WriteChatf(PLUGIN_MSG "Loading settings...");

	char szTemp[MAX_STRING] = { 0 };
	char szTempF[MAX_STRING] = { 0 };
	bool bRewriteIni = false; // re-save if bad values were read

	// default settings
#if 0
	GetPrivateProfileString("Defaults", "AllowMove",
	                        ftoa(SET->AllowMove, szTempF),
	                        szTemp, MAX_STRING, INIFileName);
	if ((float)atof(szTemp) >= 10.0f)
	{
		g_settings.AllowMove = (float)atof(szTemp);
	}
	else
	{
		bRewriteIni = true;
	}
#endif

	GetPrivateProfileString("Defaults", "AutoBreak",
		g_settings.autobreak ? "on" : "off",
		szTemp, MAX_STRING, INIFileName);
	g_settings.autobreak = (!strnicmp(szTemp, "on", 3));

#if 0
	GetPrivateProfileString("Defaults", "AutoSave",
		SET->AutoSave    ? "on" : "off",
		szTemp, MAX_STRING, INIFileName);
#endif
}

void SaveSettings()
{
	WriteChatf(PLUGIN_MSG "Saving settings...");
	char szTemp[MAX_STRING] = { 0 };

	// default settings
	WritePrivateProfileString("Defaults", "AutoBreak", g_settings.autobreak ? "on" : "off", INIFileName);

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
