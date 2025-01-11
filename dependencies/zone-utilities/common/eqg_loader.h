
#pragma once

#include "eqg_geometry.h"
#include "eqg_terrain.h"
#include "light.h"
#include "pfs.h"
#include "placeable.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <mq/api/Abilities.h>

namespace EQEmu
{
	struct SEQZoneParameters;
}

namespace EQEmu::EQG
{
	class TerrainArea;
	class Terrain;
}

namespace EQEmu {

struct SZONInstanceNew;
class BufferReader;

class EQGLoader
{
public:
	EQGLoader();
	~EQGLoader();

	bool Load(PFS::Archive* archive);

	std::vector<std::shared_ptr<EQG::Geometry>> models;
	std::vector<std::shared_ptr<Placeable>> placeables;
	std::vector<std::shared_ptr<EQG::TerrainArea>> areas;
	std::vector<std::shared_ptr<Light>> lights;
	std::shared_ptr<EQG::Terrain> terrain;
	std::shared_ptr<EQG::Geometry> terrain_model;

	std::vector<std::string> mesh_names;
	std::vector<std::string> actor_tags;

	static std::vector<std::string> ParseConfigFile(const char* buffer, size_t size);

	PFS::Archive* GetArchive() const { return m_archive; }

private:
	bool ParseFile(const std::string& fileName);

	bool ParseZone(const std::vector<char>& buffer, const std::string& tag);
	bool ParseZoneV2(const std::vector<char>& buffer, const std::string& tag);
	bool ParseModel(const std::vector<char>& buffer, const std::string& fileName, const std::string& tag);
	bool ParseTerrain(const std::vector<char>& buffer, const std::string& fileName, const std::string& tag);

	bool ParseTerrainProject(const std::vector<char>& buffer);
	void LoadZoneParameters(const char* buffer, size_t size, SEQZoneParameters& params);
	bool LoadWaterSheets(std::shared_ptr<EQG::Terrain>& terrain);
	bool LoadInvisibleWalls(std::shared_ptr<EQG::Terrain>& terrain);

	PFS::Archive* m_archive = nullptr;
	std::string m_fileName;
};

bool LoadEQGModel(PFS::Archive& archive, const std::string& model, std::shared_ptr<EQG::Geometry>& model_out);
bool LoadEQGModel(const std::vector<char>& buffer, const std::string& model, std::shared_ptr<EQG::Geometry>& model_out);


} // namespace EQEmu


using PlaceablePtr = std::shared_ptr<EQEmu::Placeable>;
using PlaceableGroupPtr = std::shared_ptr<EQEmu::PlaceableGroup>;
using EQGGeometryPtr = std::shared_ptr<EQEmu::EQG::Geometry>;
using TerrainPtr = std::shared_ptr<EQEmu::EQG::Terrain>;
using TerrainAreaPtr = std::shared_ptr<EQEmu::EQG::TerrainArea>;
