#include "pch.h"
#include "eqg_global_data.h"
#include "str_util.h"

#include <filesystem>

// TODO: This is used for parsing INI, find a better way!
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace fs = std::filesystem;

namespace eqg {

DataManager g_dataManager;

DataManager::DataManager()
{
}

DataManager::~DataManager()
{
}

void DataManager::Init(const std::string& dataPath)
{
	m_dataPath = dataPath;
	m_spellPointModsPath = (fs::path(m_dataPath) / "Resources" / "SPOffsets.ini").string();
	m_modelDataPath = (fs::path(m_dataPath) / "Resources" / "moddat.ini").string();
}

static float GetPrivateProfileFloat(const char* Section, const char* Key, const float DefaultValue, const std::string& iniFileName)
{
	const std::string strDefaultValue = std::to_string(DefaultValue);
	constexpr size_t Size = 100;
	char Return[Size] = {};
	::GetPrivateProfileStringA(Section, Key, strDefaultValue.c_str(), Return, Size, iniFileName.c_str());
	return str_to_float(Return, DefaultValue);
}

inline bool GetPrivateProfileBool(const char* Section, const char* Key, const bool DefaultValue, const std::string& iniFileName)
{
	constexpr size_t Size = 10;
	char Return[Size] = {};
	::GetPrivateProfileStringA(Section, Key, DefaultValue ? "true" : "false", Return, Size, iniFileName.c_str());
	return str_to_bool(Return, DefaultValue);
}


SpellPointMods DataManager::GetSpellPointMods(std::string_view tag) const
{
	char sectionName[64];
	strncpy_s(sectionName, tag.data(), tag.size());

	SpellPointMods mods;
	mods.pos.x = GetPrivateProfileFloat(sectionName, "XOffset", 0.6f, m_spellPointModsPath);
	mods.pos.y = GetPrivateProfileFloat(sectionName, "YOffset", 0.0f, m_spellPointModsPath);
	mods.pos.z = GetPrivateProfileFloat(sectionName, "ZOffset", 0.0f, m_spellPointModsPath);
	mods.rot.x = GetPrivateProfileFloat(sectionName, "HOffset", 0.0f, m_spellPointModsPath);
	mods.rot.y = GetPrivateProfileFloat(sectionName, "POffset", 0.0f, m_spellPointModsPath);
	mods.rot.z = GetPrivateProfileFloat(sectionName, "ROffset", 0.0f, m_spellPointModsPath);
	mods.scale.x = GetPrivateProfileFloat(sectionName, "XScale", 1.0f, m_spellPointModsPath);
	mods.scale.y = GetPrivateProfileFloat(sectionName, "YScale", 1.0f, m_spellPointModsPath);
	mods.scale.z = GetPrivateProfileFloat(sectionName, "ZScale", 1.0f, m_spellPointModsPath);

	// Convert from 512 unit rotations to radians.
	mods.rot = glm::radians((mods.rot / 512.0f) * 360.0f);

	return mods;
}

bool DataManager::GetDisableAttachments(std::string_view tag) const
{
	char sectionName[64];
	strncpy_s(sectionName, tag.data(), tag.size());

	return GetPrivateProfileBool(sectionName, "DisAtt", false, m_modelDataPath);
}

bool DataManager::GetDisableShieldAttachments(std::string_view tag) const
{
	char sectionName[64];
	strncpy_s(sectionName, tag.data(), tag.size());
	return GetPrivateProfileBool(sectionName, "DisShlAtt", false, m_modelDataPath);
}

bool DataManager::GetDisablePrimaryAttachments(std::string_view tag) const
{
	char sectionName[64];
	strncpy_s(sectionName, tag.data(), tag.size());
	return GetPrivateProfileBool(sectionName, "DisPriAtt", false, m_modelDataPath);
}

bool DataManager::GetDisableSecondaryAttachments(std::string_view tag) const
{
	char sectionName[64];
	strncpy_s(sectionName, tag.data(), tag.size());
	return GetPrivateProfileBool(sectionName, "DisSecAtt", false, m_modelDataPath);
}

} // namespace eqg
