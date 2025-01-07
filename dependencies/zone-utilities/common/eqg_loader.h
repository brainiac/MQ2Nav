
#pragma once


#include "eqg_geometry.h"
#include "eqg_region.h"
#include "eqg_terrain.h"
#include "light.h"
#include "pfs.h"
#include "placeable.h"
#include "placeable_group.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace EQEmu {

class EQGLoader
{
public:
	EQGLoader();
	~EQGLoader();

	bool Load(PFS::Archive* archive, const std::string& fileName);

	std::vector<std::shared_ptr<EQG::Geometry>> models;
	std::vector<std::shared_ptr<Placeable>> placeables;
	std::vector<std::shared_ptr<EQG::Region>> regions;
	std::vector<std::shared_ptr<Light>> lights;

private:
	bool GetZon(std::string file, std::vector<char>& buffer);
	bool ParseZon(std::vector<char>& buffer);

	PFS::Archive* m_archive = nullptr;
	std::string m_fileName;
};

class EQG4Loader
{
public:
	bool Load(std::string file, std::shared_ptr<EQG::Terrain>& terrain);

private:
	bool ParseZoneDat(EQEmu::PFS::Archive& archive, std::shared_ptr<EQG::Terrain>& terrain);
	bool ParseWaterDat(EQEmu::PFS::Archive& archive, std::shared_ptr<EQG::Terrain>& terrain);
	bool ParseInvwDat(EQEmu::PFS::Archive& archive, std::shared_ptr<EQG::Terrain>& terrain);
	bool GetZon(std::string file, std::vector<char>& buffer);
	void ParseConfigFile(std::vector<char>& buffer, std::vector<std::string>& tokens);
	bool ParseZon(std::vector<char>& buffer, EQG::Terrain::ZoneOptions& opts);
};

bool LoadEQGModel(EQEmu::PFS::Archive& archive, std::string model, std::shared_ptr<EQG::Geometry>& model_out);


} // namespace EQEmu
