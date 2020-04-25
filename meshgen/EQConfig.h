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
	const std::string& GetMQ2Path() const { return m_mq2Path; }

	void SelectEverquestPath();
	void SelectOutputPath();

	bool GetUseMaxExtents() const { return m_useMaxExtents; }
	void SetUseMaxExtents(bool use) { m_useMaxExtents = use; SaveConfigToIni(); }

	// loaded maps, keyed by their expansion group. Data is loaded from Zones.ini
	typedef std::pair<std::string /*shortName*/, std::string /*longName*/> ZoneNamePair;

	typedef std::set<ZoneNamePair> ZoneList;

	// map expansion, and its maps
	typedef std::pair<std::string, ZoneList> Expansion;

	typedef std::vector<Expansion> MapList;

	const MapList& GetMapList() const { return m_loadedMaps; }

	std::string GetLongNameForShortName(const std::string& shortName);

	const std::map<std::string, std::string>& GetAllMaps() const { return m_mapNames; }

private:
	void LoadConfigFromIni();
	void SaveConfigToIni();

	void LoadZones();

	std::string m_everquestPath;
	std::string m_mq2Path;
	std::string m_outputPath;
	bool m_useMaxExtents = true;

	MapList m_loadedMaps;

	std::map<std::string, std::string> m_mapNames;
};
