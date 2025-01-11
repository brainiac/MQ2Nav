#pragma once

#include "eqg_structs.h"
#include "eqg_loader.h"
#include "placeable.h"

#include <memory>
#include <map>

namespace EQEmu::PFS
{
	class Archive;
}

namespace EQEmu
{
	class BufferReader;
	class Light;
}

namespace EQEmu::EQG {

class Terrain;
class WaterSheet;

class InvisWall
{
public:
	InvisWall(std::string_view name_)
		: name(name_) {}

	std::string name;

	float wall_top_height;
	std::vector<glm::vec3> verts;
};

class WaterSheet
{
public:
	void SetMinX(float x) { min_x = x; }
	void SetMinY(float y) { min_y = y; }
	void SetMaxX(float x) { max_x = x; }
	void SetMaxY(float y) { max_y = y; }
	void SetZHeight(float z) { z_height = z; }
	void SetTile(bool v) { tile = v; }
	void SetIndex(int i) { index = i; }

	float GetMinX() { return min_x; }
	float GetMinY() { return min_y; }
	float GetMaxX() { return max_x; }
	float GetMaxY() { return max_y; }
	float GetZHeight() { return z_height; }
	bool GetTile() { return tile; }
	int GetIndex() { return index; }

	float min_x = 0.0f;
	float min_y = 0.0f;
	float max_x = 0.0f;
	float max_y = 0.0f;
	float z_height;
	bool tile;
	int index;
};

struct TerrainObjectGroupDefinitionObjectElement
{
	std::string name;
	glm::vec3 position;
	glm::vec3 orientation;
	float scale;
	int object_id = 0;

	glm::mat4x4 transform;

	struct ElementFile
	{
		std::string tag;
		std::string fileName;
	};

	std::vector<ElementFile> files;

	bool Load(const std::vector<std::string>& tokens, size_t& k);
};

struct TerrainObjectGroupDefinitionAreaElement
{
	std::string name;
	glm::vec3 position;
	glm::vec3 orientation;
	glm::vec3 extents;

	glm::mat4x4 transform;

	bool Load(const std::vector<std::string>& tokens, size_t& k);
};

struct TerrainObjectGroupDefinition
{
	std::string name;
	std::vector<std::shared_ptr<TerrainObjectGroupDefinitionObjectElement>> objects;
	std::vector<std::shared_ptr<TerrainObjectGroupDefinitionAreaElement>> areas;

	bool Load(EQEmu::PFS::Archive* archive, const std::string& group_name);
};

struct TerrainObjectGroup;

class TerrainArea
{
public:
	std::string name;
	std::string shape;
	uint32_t type = 0;

	glm::vec3 position;
	glm::vec3 orientation;
	glm::vec3 extents;

	glm::mat4x4 transform;

	glm::vec3 scale; // should probably just remove

	TerrainObjectGroup* group = nullptr;
};

struct TerrainObject
{
	int object_id = -1;
	std::string name;
	std::string ecosystem;
	uint8_t shade_factor = 255;

	glm::vec3 position;
	glm::vec3 orientation;
	glm::vec3 scale;

	glm::mat4x4 transform;

	uint32_t* rgbLitData = nullptr;
	TerrainObjectGroup* group = nullptr;
};

class TerrainTile;

struct TerrainObjectGroup
{
	std::string name;

	glm::vec3 position;
	glm::vec3 orientation;
	glm::vec3 scale;

	glm::mat4x4 transform;

	std::vector<std::shared_ptr<TerrainObject>> objects;
	std::vector<std::shared_ptr<TerrainArea>> areas;

	void Initialize(TerrainObjectGroupDefinition* definition);

	TerrainObjectGroupDefinition* m_definition = nullptr;
};

class TerrainTile
{
	friend class Terrain;

public:
	TerrainTile(Terrain* parent);
	~TerrainTile();

	glm::vec3 GetPosInTile(const glm::vec3& pos) const;

	// Tile coordinates in tile grid
	glm::ivec2 tile_loc;

	// Actual world coordinates of tile.
	glm::vec3 tile_pos;
	uint32_t flora_seed = 0;
	bool flat = false;
	glm::mat4x4 tile_transform;

	std::vector<float> height_field;
	std::vector<uint32_t> vertex_color;
	std::vector<uint32_t> baked_lighting;
	std::vector<uint8_t> quad_flags;

	float base_water_level = -1000.0f;
	int water_data_index = 1;
	bool has_water_sheet = false;
	float water_sheet_min_x = 0.0f;
	float water_sheet_max_x = 0.0f;
	float water_sheet_min_y = 0.0f;
	float water_sheet_max_y = 0.0f;
	float lava_level = -1000.0f;
	WaterSheet* water_sheet = nullptr;

	std::vector<std::shared_ptr<EQG::TerrainArea>> m_areas;
	std::vector<std::shared_ptr<TerrainObject>> m_objects;
	std::vector<std::shared_ptr<TerrainObjectGroup>> m_groups;
	std::vector<std::shared_ptr<Light>> m_lights;

	Terrain* terrain = nullptr;

	bool Load(BufferReader& reader, int version);
};

class Terrain
{
	friend class TerrainTile;

public:
	Terrain(const SEQZoneParameters& params);
	~Terrain();

	bool Load(EQEmu::PFS::Archive* archive);

	void AddTile(const std::shared_ptr<TerrainTile>& t) { tiles.push_back(t); }
	void AddWaterSheet(const std::shared_ptr<WaterSheet>& s) { water_sheets.push_back(s); }
	void AddInvisWall(std::shared_ptr<InvisWall> w) { invis_walls.push_back(w); }

	std::vector<std::shared_ptr<TerrainTile>>& GetTiles() { return tiles; }
	uint32_t GetQuadsPerTile() { return m_params.quads_per_tile; }
	float GetUnitsPerVertex() { return m_params.units_per_vert; }
	std::vector<std::shared_ptr<WaterSheet>>& GetWaterSheets() { return water_sheets; }
	std::vector<std::shared_ptr<InvisWall>>& GetInvisWalls() { return invis_walls; }

	const SEQZoneParameters& GetParams() const { return m_params; }

	TerrainObjectGroupDefinition* GetObjectGroupDefinition(const std::string& name);

private:
	void LoadWaterSheets();
	void LoadInvisibleWalls();

	SEQZoneParameters m_params;
	EQEmu::PFS::Archive* m_archive = nullptr;

public:
	uint32_t version = 0;
	float zone_min_x = 0.0f;
	float zone_min_y = 0.0f;
	uint32_t quad_count = 0;
	uint32_t vert_count = 0;

	std::vector<std::shared_ptr<TerrainTile>> tiles;

	std::vector<std::shared_ptr<WaterSheet>> water_sheets;
	std::vector<std::shared_ptr<InvisWall>> invis_walls;

	std::vector<std::shared_ptr<TerrainArea>> areas;
	std::vector<std::shared_ptr<Light>> lights;

	std::vector<std::shared_ptr<Placeable>> objects;

	std::vector<std::shared_ptr<TerrainObjectGroupDefinition>> group_definitions;
};

} // namespace EQEmu::EQG
