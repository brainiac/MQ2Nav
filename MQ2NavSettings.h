// MQ2NavSettings.h : Defines the entry point for the DLL application.
//

// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup, do NOT do it in DllMain.

#ifndef __MQ2NAVSETTINGS_H
#define __MQ2NAVSETTINGS_H


#include "../MQ2Plugin.h"

namespace mq2nav {
namespace settings {

	class Data {
	public:
		bool autobreak;


		Data() {
			memset(this, 0, sizeof(Data));
		}
	private:

	};

	Data& instance() {
		static Data data_;
		return data_;
	}

	void LoadSettings() {
		WriteChatf("[MQ2Navigation] loading...");

		char szTemp[MAX_STRING]     = {0};
		char szTempF[MAX_STRING]    = {0};
		bool bRewriteIni            = false; // re-save if bad values were read

		//// default settings
		//GetPrivateProfileString("Defaults", "AllowMove",       ftoa(SET->AllowMove, szTempF),   szTemp, MAX_STRING, INIFileName);
		//if ((float)atof(szTemp) >= 10.0f)
		//{
		//    SET->AllowMove = (float)atof(szTemp);
		//}
		//else
		//{
		//    bRewriteIni = true;
		//}
		GetPrivateProfileString("Defaults", "AutoBreak",       instance().autobreak   ? "on" : "off", szTemp, MAX_STRING, INIFileName);
		instance().autobreak = (!strnicmp(szTemp, "on", 3));
		//GetPrivateProfileString("Defaults", "AutoSave",        SET->AutoSave    ? "on" : "off", szTemp, MAX_STRING, INIFileName);
	}


	void SaveSettings() {
		WriteChatf("[MQ2Navigation] saving...");
		char szTemp[MAX_STRING]     = {0};

		//// default settings
		//WritePrivateProfileString("Defaults",   "AllowMove",              ftoa(SET->AllowMove,      szTemp),     INIFileName);
		WritePrivateProfileString("Defaults",   "AutoBreak",              instance().autobreak       ? "on" : "off",   INIFileName);
		//WritePrivateProfileString("Defaults",   "AutoPauseMsg",           (uiVerbLevel & V_AUTOPAUSE) == V_AUTOPAUSE             ? "on" : "off",      INIFileName);
		//WritePrivateProfileString("Defaults",   "AutoSave",               SET->AutoSave        ? "on" : "off",   INIFileName);
		//WritePrivateProfileString("Defaults",   "AutoUW",                 SET->AutoUW          ? "on" : "off",   INIFileName);
		//WritePrivateProfileString("Defaults",   "BreakKeyboard",          SET->BreakKB         ? "on" : "off",   INIFileName);
		//WritePrivateProfileString("Defaults",   "BreakMouse",             SET->BreakMouse      ? "on" : "off",   INIFileName);
	}

}
} // namespace mq2nav {

#endif