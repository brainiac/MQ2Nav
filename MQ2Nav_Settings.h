//
// MQ2Nav_Settings.h
//

#pragma once

#include "MQ2Navigation.h"

namespace mq2nav
{
	struct SettingsData
	{
		bool autobreak = 0;
	};
	SettingsData& GetSettings();

	// Load settings from the .ini file
	void LoadSettings();

	// Save settings to the .ini file
	void SaveSettings();

} // namespace mq2nav
