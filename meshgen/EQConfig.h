//
// EQConfig.h
//


// This class serves as the storage for configuration regarding everquest
// and the generation of navmeshes!

#pragma once

#include <map>
#include <string>
#include <set>
#include <vector>

class EQConfig
{
public:
	EQConfig();
	~EQConfig();

	const std::string& GetEverquestPath() const { return m_everquestPath; }
	const std::string& GetOutputPath() const { return m_outputPath; }
	const std::string& GetMacroQuestPath() const { return m_mqPath; }

	void SelectEverquestPath();
	void SelectOutputPath();

	bool GetUseMaxExtents() const { return m_useMaxExtents; }
	void SetUseMaxExtents(bool use) { m_useMaxExtents = use; SaveConfigToIni(); }

	// loaded maps, keyed by their expansion group. Data is loaded from Zones.ini
	using ZoneNamePair = std::pair<std::string /*shortName*/, std::string /*longName*/>;
	using ZoneList = std::set<ZoneNamePair>;

	// map expansion, and its maps
	using Expansion = std::pair<std::string, ZoneList>;
	using MapList = std::vector<Expansion>;

	const MapList& GetMapList() const { return m_loadedMaps; }

	std::string GetLongNameForShortName(const std::string& shortName);

	const std::map<std::string, std::string>& GetAllMaps() const { return m_mapNames; }

private:
	void LoadConfigFromIni();
	void SaveConfigToIni();

	void LoadZones();

	std::string m_everquestPath;
	std::string m_mqPath;
	std::string m_outputPath;
	bool m_useMaxExtents = true;

	MapList m_loadedMaps;

	std::map<std::string, std::string> m_mapNames;
};
