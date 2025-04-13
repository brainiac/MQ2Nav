#pragma once

#include <string>

namespace eqg {

struct SpellPointMods
{
	glm::vec3 pos;
	glm::vec3 rot;
	glm::vec3 scale;
};

class DataManager
{
public:
	DataManager();
	~DataManager();

	void Init(const std::string& dataPath);

	// SPOffsets.ini
	SpellPointMods GetSpellPointMods(std::string_view tag) const;

	// moddat.ini
	bool GetDisableAttachments(std::string_view tag) const;
	bool GetDisableShieldAttachments(std::string_view tag) const;
	bool GetDisablePrimaryAttachments(std::string_view tag) const;
	bool GetDisableSecondaryAttachments(std::string_view tag) const;

private:
	std::string m_dataPath;
	std::string m_spellPointModsPath;
	std::string m_modelDataPath;
};

extern DataManager g_dataManager;

} // namespace eqg
