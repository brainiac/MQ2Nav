#pragma once

#include "placeable_group.h"
#include "eqg_loader.h"
#include "eqg_region.h"

#include <memory>
#include <map>

namespace EQEmu::EQG {

class InvisWall
{
public:
	void SetName(const std::string& n) { name = n; }
	std::string& GetName() { return name; }

	std::vector<glm::vec3>& GetVerts() { return verts; }

	std::string name;
	std::vector<glm::vec3> verts;
};

class TerrainTile
{
public:
	void SetLocation(float nx, float ny) { x = nx; y = ny; }
	void SetFlat(bool value) { flat = value; }
	void SetBaseWaterLevel(float v) { base_water_level = v; }

	float GetX() { return x; }
	float GetY() { return y; }
	bool IsFlat() { return flat; };
	std::vector<float>& GetFloats() { return floats; }
	std::vector<uint32_t>& GetColors() { return colors; }
	std::vector<uint32_t>& GetColors2() { return colors2; }
	std::vector<uint8_t>& GetFlags() { return flags; }
	float GetBaseWaterLevel() { return base_water_level; }

	float x = 0.0f;
	float y = 0.0f;
	bool flat = false;
	std::vector<float> floats;
	std::vector<uint32_t> colors;
	std::vector<uint32_t> colors2;
	std::vector<uint8_t> flags;
	float base_water_level;
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


class Terrain
{
public:
	struct ZoneOptions
	{
		std::string name;
		int32_t min_lng;
		int32_t max_lng;
		int32_t min_lat;
		int32_t max_lat;
		float min_extents[3];
		float max_extents[3];
		float units_per_vert;
		uint32_t quads_per_tile;
		uint32_t cover_map_input_size;
		uint32_t layer_map_input_size;
		float base_water_level;
	};

	void AddTile(std::shared_ptr<TerrainTile> t) { tiles.push_back(t); }
	void SetQuadsPerTile(uint32_t value) { quads_per_tile = value; }
	void SetUnitsPerVertex(float value) { units_per_vertex = value; }
	void AddWaterSheet(std::shared_ptr<WaterSheet> s) { water_sheets.push_back(s); }
	void AddInvisWall(std::shared_ptr<InvisWall> w) { invis_walls.push_back(w); }
	void AddPlaceableGroup(std::shared_ptr<PlaceableGroup> p) { placeable_groups.push_back(p); }
	void AddRegion(std::shared_ptr<EQG::Region> r) { regions.push_back(r); }
	void SetOpts(const ZoneOptions& opt) { opts = opt; }

	std::vector<std::shared_ptr<TerrainTile>>& GetTiles() { return tiles; }
	uint32_t GetQuadsPerTile() { return quads_per_tile; }
	float GetUnitsPerVertex() { return units_per_vertex; }
	std::vector<std::shared_ptr<WaterSheet>>& GetWaterSheets() { return water_sheets; }
	std::vector<std::shared_ptr<InvisWall>>& GetInvisWalls() { return invis_walls; }
	std::vector<std::shared_ptr<PlaceableGroup>>& GetPlaceableGroups() { return placeable_groups; }
	std::map<std::string, std::shared_ptr<EQG::Geometry>>& GetModels() { return models; }
	std::vector<std::shared_ptr<EQG::Region>>& GetRegions() { return regions; }
	ZoneOptions& GetOpts() { return opts; }

	std::vector<std::shared_ptr<TerrainTile>> tiles;
	uint32_t quads_per_tile;
	float units_per_vertex;

	std::vector<std::shared_ptr<WaterSheet>> water_sheets;
	std::vector<std::shared_ptr<InvisWall>> invis_walls;
	std::vector<std::shared_ptr<PlaceableGroup>> placeable_groups;
	std::map<std::string, std::shared_ptr<EQG::Geometry>> models;
	std::vector<std::shared_ptr<EQG::Region>> regions;
	ZoneOptions opts;
};

} // namespace EQEmu::EQG
