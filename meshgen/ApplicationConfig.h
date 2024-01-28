//
// EQConfig.h
//


// This class serves as the storage for configuration regarding everquest
// and the generation of navmeshes!

#pragma once

#include <glm/glm.hpp>

#include <map>
#include <string>
#include <set>
#include <vector>

class ApplicationConfig
{
public:
	ApplicationConfig();
	~ApplicationConfig();

	void Initialize();

	const std::string& GetExecutablePath() const { return m_exePath; }
	const std::string& GetSettingsFileName() const { return m_settingsFile; }

	void SetEverquestPath(const std::string& everquestPath);
	const std::string& GetEverquestPath() const { return m_everquestPath; }

	// Set the MacroQuest path, and update all the other paths that are relative to it.
	void SetMacroQuestPath(const std::string& macroquestPath);
	const std::string& GetMacroQuestPath() const { return m_mqPath; }

	bool IsMacroQuestPathValid() const { return m_mqPathValid; }

	// These paths are based on MacroQuest.ini settings found in the MacroQuest folder.
	// If we didn't find a MacroQuest.ini then we use the defaults.
	const std::string& GetResourcesPath() const { return m_resourcesPath; }
	const std::string& GetLogsPath() const { return m_logsPath; }
	const std::string& GetConfigPath() const { return m_configPath; }

	// This is the path to the mesh directory. It is relative to the resources path
	const std::string& GetOutputPath() const { return m_outputPath; }

	bool SelectEverquestPath();
	bool SelectMacroQuestPath();

	bool GetSavedWindowDimensions(glm::ivec4& rect) const;
	void SetSavedWindowDimensions(const glm::ivec4& rect);

	bool GetUseMaxExtents() const { return m_useMaxExtents; }
	void SetUseMaxExtents(bool use);


	// loaded maps, keyed by their expansion group. Data is loaded from Zones.ini
	using ZoneNamePair = std::pair<std::string /*shortName*/, std::string /*longName*/>;
	using ZoneList = std::set<ZoneNamePair>;

	// map expansion, and its maps
	using Expansion = std::pair<std::string, ZoneList>;
	using MapList = std::vector<Expansion>;

	const MapList& GetMapList() const { return m_loadedMaps; }

	std::string GetLongNameForShortName(const std::string& shortName);
	const std::map<std::string, std::string>& GetAllMaps() const { return m_mapNames; }
	bool AreMapsLoaded() const { return m_mapsLoaded; }

private:
	bool LoadZones();

	std::string m_exePath;
	std::string m_everquestPath;
	std::string m_mqPath;
	std::string m_outputPath;
	std::string m_settingsFile;
	std::string m_resourcesPath;
	std::string m_logsPath;
	std::string m_configPath;
	bool m_mqPathValid = false;

	glm::ivec4 m_savedDimensions;
	bool m_useMaxExtents = true;

	MapList m_loadedMaps;
	std::map<std::string, std::string> m_mapNames;
	bool m_mapsLoaded = false;
};

extern ApplicationConfig g_config;
