
#pragma once

#include "eqg_geometry.h"
#include "eqg_terrain.h"
#include "light.h"
#include "placeable.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace eqg {

class Archive;
class BufferReader;
class Light;
class Placeable;
class Terrain;
class TerrainArea;
struct SEQZoneParameters;
struct SZONInstanceNew;

class EQGLoader
{
public:
	EQGLoader();
	~EQGLoader();

	bool Load(Archive* archive);

	std::vector<std::shared_ptr<Geometry>> models;
	std::vector<std::shared_ptr<Placeable>> placeables;
	std::vector<std::shared_ptr<TerrainArea>> areas;
	std::vector<std::shared_ptr<Light>> lights;
	std::shared_ptr<Terrain> terrain;
	std::shared_ptr<Geometry> terrain_model;

	std::vector<std::string> mesh_names;
	std::vector<std::string> actor_tags;

	static std::vector<std::string> ParseConfigFile(const char* buffer, size_t size);

	Archive* GetArchive() const { return m_archive; }

private:
	bool ParseFile(const std::string& fileName);

	bool ParseZone(const std::vector<char>& buffer, const std::string& tag);
	bool ParseZoneV2(const std::vector<char>& buffer, const std::string& tag);
	bool ParseModel(const std::vector<char>& buffer, const std::string& fileName, const std::string& tag);
	bool ParseTerrain(const std::vector<char>& buffer, const std::string& fileName, const std::string& tag);

	bool ParseTerrainProject(const std::vector<char>& buffer);
	void LoadZoneParameters(const char* buffer, size_t size, SEQZoneParameters& params);
	bool LoadWaterSheets(std::shared_ptr<Terrain>& terrain);
	bool LoadInvisibleWalls(std::shared_ptr<Terrain>& terrain);

	Archive* m_archive = nullptr;
	std::string m_fileName;
};

bool LoadEQGModel(Archive& archive, const std::string& model, std::shared_ptr<Geometry>& model_out);
bool LoadEQGModel(const std::vector<char>& buffer, const std::string& model, std::shared_ptr<Geometry>& model_out);

} // namespace eqg


using PlaceablePtr = std::shared_ptr<eqg::Placeable>;
using EQGGeometryPtr = std::shared_ptr<eqg::Geometry>;
using TerrainPtr = std::shared_ptr<eqg::Terrain>;
using TerrainAreaPtr = std::shared_ptr<eqg::TerrainArea>;
