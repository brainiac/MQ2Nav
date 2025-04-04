
#include "pch.h"
#include "eqg_terrain.h"

#include "archive.h"
#include "buffer_reader.h"
#include "eqg_material.h"
#include "eqg_resource_manager.h"
#include "eqg_structs.h"
#include "light.h"
#include "log_internal.h"

#include <glm/gtx/matrix_decompose.hpp>

namespace eqg {

constexpr float S3D_UV_TO_FLOAT = 1.0f / 256.0f;
constexpr float S3D_NORM_TO_FLOAT = 1.0f / 127.0f;

constexpr float EQ_TO_RAD = glm::pi<float>() / 256.0f;

TerrainSystem::TerrainSystem(Archive* archive, ResourceManager* resourceMgr)
	: m_archive(archive)
	, m_resourceMgr(resourceMgr)
{
}

TerrainSystem::~TerrainSystem()
{
}

// Given x and y, Interpolate z across a quad defined by 4 points.
static float HeightWithinQuad(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 p4, float x, float y)
{
	int inTriangle = 0;

	glm::vec3 a, b, c;
	glm::vec3 n;

	float fAB = (y - p1.y) * (p2.x - p1.x) - (x - p1.x) * (p2.y - p1.y);
	float fBC = (y - p2.y) * (p3.x - p2.x) - (x - p2.x) * (p3.y - p2.y);
	float fCA = (y - p3.y) * (p1.x - p3.x) - (x - p3.x) * (p1.y - p3.y);

	if ((fAB * fBC >= 0) && (fBC * fCA >= 0))
	{
		inTriangle = 1;
		a = p1;
		b = p2;
		c = p3;
	}

	fAB = (y - p1.y) * (p3.x - p1.x) - (x - p1.x) * (p3.y - p1.y);
	fBC = (y - p3.y) * (p4.x - p3.x) - (x - p3.x) * (p4.y - p3.y);
	fCA = (y - p4.y) * (p1.x - p4.x) - (x - p4.x) * (p1.y - p4.y);


	if ((fAB * fBC >= 0) && (fBC * fCA >= 0))
	{
		inTriangle = 2;
		a = p1;
		b = p3;
		c = p4;
	}

	n.x = (b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y);
	n.y = (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z);
	n.z = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);

	float len = sqrt(n.x * n.x + n.y * n.y + n.z * n.z);

	n.x /= len;
	n.y /= len;
	n.z /= len;

	return (((n.x) * (x - a.x) + (n.y) * (y - a.y)) / -n.z) + a.z;
}

bool TerrainSystem::Load(const char* zonBuffer, size_t size)
{
	if (size < 5)
		return false;

	if (strncmp(zonBuffer, "EQTZP", 5) != 0)
	{
		EQG_LOG_ERROR("Input .zon buffer is not an EQTZP file");
		return false;
	}

	SEQZoneParameters params;
	LoadZoneParameters(zonBuffer, size, params);

	return Load(params);
}

bool TerrainSystem::Load(const SEQZoneParameters& params)
{
	m_params = params;

	EQG_LOG_DEBUG("Parsing zone data from terrain for {}.", m_params.name);

	// Start with water sheets so that we have the data ready when tiles reference it.
	EQG_LOG_DEBUG("Parsing water data file.");
	LoadWaterSheets();

	EQG_LOG_DEBUG("Parsing invisible walls file.");
	LoadInvisibleWalls();

	if (!LoadTiles())
	{
		EQG_LOG_ERROR("Failed to load terrain tiles.");
		return false;
	}

	return true;
}

static std::vector<std::string> ParseConfigFile(const char* buffer, size_t size)
{
	std::vector<std::string> tokens;

	std::string cur;
	for (size_t i = 0; i < size; ++i)
	{
		char c = buffer[i];

		if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f')
		{
			if (cur.size() > 0)
			{
				tokens.push_back(cur);
				cur.clear();
			}
		}
		else
		{
			cur.push_back(c);
		}
	}

	return tokens;
}


void TerrainSystem::LoadZoneParameters(const char* buffer, size_t size, SEQZoneParameters& params)
{
	params.min_lng = -1000000;
	params.min_lat = -1000000;
	params.max_lng = 1000000;
	params.max_lat = 1000000;
	params.units_per_vert = 12.0f;
	params.quads_per_tile = 16;
	params.layer_map_input_size = 1024;
	params.cover_map_input_size = 256;
	params.version = 1;

	std::vector<std::string> tokens = ParseConfigFile(buffer, size);

	for (size_t i = 1; i < tokens.size();)
	{
		std::string_view token = tokens[i];

		if (token.compare("*NAME") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			params.name = tokens[i + 1];
			i += 2;
		}
		else if (token.compare("*MINLNG") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			params.min_lng = std::stoi(tokens[i + 1]);
			i += 2;
		}
		else if (token.compare("*MAXLNG") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			params.max_lng = std::stoi(tokens[i + 1]);
			i += 2;
		}
		else if (token.compare("*MINLAT") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			params.min_lat = std::stoi(tokens[i + 1]);
			i += 2;
		}
		else if (token.compare("*MAXLAT") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			params.max_lat = std::stoi(tokens[i + 1]);
			i += 2;
		}
		else if (token.compare("*MIN_EXTENTS") == 0)
		{
			if (i + 3 >= tokens.size())
			{
				break;
			}

			params.min_extents[0] = std::stof(tokens[i + 1]);
			params.min_extents[1] = std::stof(tokens[i + 2]);
			params.min_extents[2] = std::stof(tokens[i + 3]);
			i += 4;
		}
		else if (token.compare("*MAX_EXTENTS") == 0)
		{
			if (i + 3 >= tokens.size())
			{
				break;
			}

			params.max_extents[0] = std::stof(tokens[i + 1]);
			params.max_extents[1] = std::stof(tokens[i + 2]);
			params.max_extents[2] = std::stof(tokens[i + 3]);
			i += 4;
		}
		else if (token.compare("*UNITSPERVERT") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			params.units_per_vert = std::stof(tokens[i + 1]);
			i += 2;
		}
		else if (token.compare("*QUADSPERTILE") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			params.quads_per_tile = std::stoi(tokens[i + 1]);
			i += 2;
		}
		else if (token.compare("*COVERMAPINPUTSIZE") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			params.cover_map_input_size = std::stoi(tokens[i + 1]);
			i += 2;
		}
		else if (token.compare("*LAYERINGMAPINPUTSIZE") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			params.layer_map_input_size = std::stoi(tokens[i + 1]);
			i += 2;
		}
		else if (token.compare("*VERSION") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			params.version = std::stoi(tokens[i + 1]);
			i += 2;
		}
		else
		{
			++i;
		}
	}

	params.units_per_tile = params.units_per_vert * params.quads_per_tile;
	params.tiles_per_chunk = 16;
	params.units_per_chunk = params.units_per_tile * params.tiles_per_chunk;
	params.verts_per_tile = params.quads_per_tile + 1;
}

bool TerrainSystem::LoadTiles()
{
	std::string filename = m_params.name + ".dat";
	std::vector<char> buffer;

	if (!m_archive->Get(filename, buffer))
	{
		EQG_LOG_ERROR("Failed to open {}.", filename);
		return false;
	}

	BufferReader reader(buffer);

	std::string fallback_detail_map_name;
	uint32_t flags, fallback_detail_repeat, tile_count;

	if (!reader.read_multiple(version, flags, fallback_detail_repeat, fallback_detail_map_name, tile_count))
		return false;

	EQG_LOG_DEBUG("Loading zone terrain {} version {}", filename, version);

	zone_min_x = m_params.min_lng * m_params.quads_per_tile * m_params.units_per_vert;
	zone_min_y = m_params.min_lat * m_params.quads_per_tile * m_params.units_per_vert;
	quad_count = m_params.quads_per_tile * m_params.quads_per_tile;
	vert_count = (m_params.quads_per_tile + 1) * (m_params.quads_per_tile + 1);

	EQG_LOG_TRACE("Parsing zone terrain tiles.");
	for (uint32_t i = 0; i < tile_count; ++i)
	{
		std::shared_ptr<TerrainTile> tile = std::make_shared<TerrainTile>(this);

		if (!tile->Load(reader, version))
		{
			EQG_LOG_ERROR("Failed to parse zone terrain tile {}: {}, {}", i, tile->tile_loc.x, tile->tile_loc.y);
			return false;
		}

		// Add areas and objects from the tile to our collection.
		for (const auto& area : tile->m_areas)
		{
			areas.push_back(area);
		}

		for (const auto& light : tile->m_lights)
		{
			lights.push_back(light);
		}

		for (const auto& object : tile->m_objects)
		{
			auto placeable = std::make_shared<Placeable>();
			placeable->tag = object->name;
			placeable->pos = object->position;
			placeable->rotate = object->orientation;
			placeable->scale = object->scale;
			placeable->terrain_object = object;

			objects.push_back(placeable);
		}

		for (const auto& object_group : tile->m_groups)
		{
			for (const auto& area : object_group->areas)
			{
				areas.push_back(area);
			}

			for (const auto& object : object_group->objects)
			{
				auto placeable = std::make_shared<Placeable>();
				placeable->tag = object->name;
				placeable->pos = object->position;
				placeable->rotate = object->orientation;
				placeable->scale = object->scale;
				placeable->terrain_object = object;

				objects.push_back(placeable);
			}
		}

		tiles.push_back(tile);
	}

	return true;
}

bool InvisibleWall::Load(BufferReader& reader)
{
	reader.read(m_wallTopHeight);

	uint32_t vert_count;
	reader.read(vert_count);

	m_vertices.resize(vert_count);

	if (!reader.read(m_vertices.data(), m_vertices.size()))
		return false;

	return true;
}

bool TerrainSystem::LoadInvisibleWalls()
{
	std::vector<char> buffer;
	if (!m_archive->Get("InvW.dat", buffer))
	{
		return false;
	}

	BufferReader reader(buffer);

	uint32_t count = reader.read<uint32_t>();

	for (uint32_t i = 0; i < count; ++i)
	{
		const char* name = reader.read_cstr();
		std::shared_ptr<InvisibleWall> invisWall = std::make_shared<InvisibleWall>(name);

		if (!invisWall->Load(reader))
			return false;

		invis_walls.push_back(invisWall);
	}

	return true;
}

bool TerrainSystem::LoadWaterSheets()
{
	std::vector<char> dat_buffer;
	if (!m_archive->Get("water.dat", dat_buffer))
	{
		return false;
	}

	std::vector<std::string> tokens = ParseConfigFile(dat_buffer.data(), dat_buffer.size());
	if (tokens.empty())
	{
		return true;
	}

	size_t k = 0;

	std::string_view token = tokens[k++];

	if (token == "*BEGIN_WATERSHEETS")
	{
		while (k < tokens.size())
		{
			token = tokens[k++];

			if (token == "*END_WATERSHEETS")
				break;

			if (token == "*WATERSHEET")
			{
				auto ws = std::make_shared<WaterSheet>(this, tokens[k++]);
				ws->Init(tokens, k);

				m_waterSheets.push_back(ws);
			}
		}
	}

	while (k < tokens.size())
	{
		token = tokens[k++];

		if (token == "*END_WATERSHEETDATA")
			break;

		if (token == "*WATERSHEETDATA")
		{
			auto ws = std::make_shared<WaterSheetData>();
			ws->Init(tokens, k);

			m_waterSheetData.push_back(ws);
		}
	}

	return true;
}

TerrainObjectGroupDefinition* TerrainSystem::GetObjectGroupDefinition(const std::string& name)
{
	for (auto& group : group_definitions)
	{
		if (group->name == name)
			return group.get();
	}

	auto group_definition = std::make_shared<TerrainObjectGroupDefinition>();
	group_definition->Load(m_archive, name);

	group_definitions.push_back(group_definition);
	return group_definition.get();
}

std::shared_ptr<WaterSheetData> TerrainSystem::GetWaterSheetData(uint32_t index) const
{
	for (const auto& pData : m_waterSheetData)
	{
		if (pData->GetIndex() == index)
			return pData;
	}

	return nullptr;
}

glm::vec3 TerrainTile::GetPosInTile(const glm::vec3& pos) const
{
	float adjusted_x = pos.x;
	float adjusted_y = pos.y;
	auto& params = terrain->GetParams();

	if (adjusted_x < 0)
		adjusted_x = adjusted_x + (-(int)(adjusted_x / (params.units_per_vert * params.quads_per_tile)) + 1) * (params.units_per_vert * params.quads_per_tile);
	else
		adjusted_x = fmod(adjusted_x, params.units_per_vert * params.quads_per_tile);

	if (adjusted_y < 0)
		adjusted_y = adjusted_y + (-(int)(adjusted_y / (params.units_per_vert * params.quads_per_tile)) + 1) * (params.units_per_vert * params.quads_per_tile);
	else
		adjusted_y = fmod(adjusted_y, params.units_per_vert * params.quads_per_tile);

	int row_number = (int)(adjusted_y / params.units_per_vert);
	int column = (int)(adjusted_x / params.units_per_vert);
	int quad = row_number * params.quads_per_tile + column;

	float quad_vertex1Z = height_field[quad + row_number];
	float quad_vertex2Z = height_field[quad + row_number + params.quads_per_tile + 1];
	float quad_vertex3Z = height_field[quad + row_number + params.quads_per_tile + 2];
	float quad_vertex4Z = height_field[quad + row_number + 1];

	glm::vec3 p1(row_number * params.units_per_vert, (quad % params.quads_per_tile) * params.units_per_vert, quad_vertex1Z);
	glm::vec3 p2(p1.x + params.units_per_vert, p1.y, quad_vertex2Z);
	glm::vec3 p3(p1.x + params.units_per_vert, p1.y + params.units_per_vert, quad_vertex3Z);
	glm::vec3 p4(p1.x, p1.y + params.units_per_vert, quad_vertex4Z);

	float terrain_height = HeightWithinQuad(p1, p2, p3, p4, adjusted_y, adjusted_x);

	return glm::vec3{ pos.x + tile_pos.x, pos.y + tile_pos.y, pos.z + terrain_height };
}

bool TerrainTile::Load(BufferReader& reader, int version)
{
	if (!reader.read(tile_loc))
		return false;
	tile_loc -= glm::ivec2{ 100000, 100000 };
	if (!reader.read(flora_seed))
		return false;

	const auto& params = terrain->GetParams();

	float tile_start_x = terrain->zone_min_x + (tile_loc.x - params.min_lng) * params.units_per_vert * params.quads_per_tile;
	float tile_start_y = terrain->zone_min_y + (tile_loc.y - params.min_lat) * params.units_per_vert * params.quads_per_tile;
	tile_pos = { tile_start_x, tile_start_y, 0 };
	tile_transform = glm::translate(glm::identity<glm::mat4x4>(), tile_pos);

	height_field.resize(terrain->vert_count);
	vertex_color.resize(terrain->vert_count);
	baked_lighting.resize(terrain->vert_count);
	quad_flags.resize(terrain->quad_count);

	if (!reader.read(height_field.data(), terrain->vert_count))
		return false;
	if (!reader.read(vertex_color.data(), terrain->vert_count))
		return false;
	if (!reader.read(baked_lighting.data(), terrain->vert_count))
		return false;
	if (!reader.read(quad_flags.data(), terrain->quad_count))
		return false;

	bool tile_is_flat = true;
	for (size_t idx = 1; idx < height_field.size(); ++idx)
	{
		if (height_field[idx] != height_field[0])
		{
			tile_is_flat = false;
			break;
		}
	}
	if (tile_is_flat)
	{
		for (size_t idx = 0; idx < quad_flags.size(); ++idx)
		{
			if (quad_flags[idx] & 0x01) // bmQuadExcluded
				tile_is_flat = false;
		}
	}

	this->flat = tile_is_flat;

	if (!reader.read(base_water_level))
		return false;

	if (version >= 21)
	{
		if (!reader.read(water_data_index))
			return false;

		if (!reader.read(has_water_sheet))
			return false;

		if (has_water_sheet)
		{
			if (!reader.read_multiple(water_sheet_min_x, water_sheet_max_x, water_sheet_min_y, water_sheet_max_y))
				return false;

			std::string name = fmt::format("WS_{}_{}", tile_loc.x + 100000, tile_loc.y + 100000);

			auto p_water_sheet = std::make_shared<WaterSheet>(terrain, name, terrain->GetWaterSheetData(water_data_index));
			p_water_sheet->m_minX = tile_start_x + water_sheet_min_x;
			p_water_sheet->m_maxX = tile_start_x + water_sheet_max_x;
			p_water_sheet->m_minY = tile_start_y + water_sheet_min_y;
			p_water_sheet->m_maxY = tile_start_y + water_sheet_max_y;
			p_water_sheet->m_zHeight = base_water_level;

			terrain->m_waterSheets.push_back(p_water_sheet);

			water_sheet = p_water_sheet.get();
		}
	}

	if (!reader.read(lava_level))
		return false;

	// TODO: Handle flora layers
	uint32_t layer_count = 0;
	if (!reader.read(layer_count))
		return false;

	if (layer_count > 0)
	{
		std::string base_material = reader.read_cstr();

		uint32_t overlay_count = 0;
		for (uint32_t layer = 1; layer < layer_count; ++layer)
		{
			const char* material = reader.read_cstr();

			uint32_t detail_mask_dim = reader.read<uint32_t>();
			uint32_t detail_mask_size = detail_mask_dim * detail_mask_dim;

			uint8_t* detail_mask_bytes = reader.read_array<uint8_t>(detail_mask_size);

			++overlay_count;
		}
	}

	int num_objects;
	if (!reader.read(num_objects))
		return false;

	for (int j = 0; j < num_objects; ++j)
	{
		std::string model_name;
		std::string eco_name;
		glm::ivec2 loc;
		glm::vec3 pos, rot, scale;
		uint8_t shade_factor = 255;
		int object_id = -1;

		if (!reader.read_multiple(model_name, eco_name, loc, pos, rot, scale, shade_factor))
			return false;
		loc -= glm::ivec2{ 100000, 100000 };

		if (version >= 22)
		{
			if (!reader.read(object_id))
				return false;
		}

		std::shared_ptr<TerrainObject> obj = std::make_shared<TerrainObject>();
		obj->object_id = object_id;
		obj->name = to_upper_copy(model_name) + "_ACTORDEF";
		obj->ecosystem = eco_name;
		obj->shade_factor = shade_factor;
		obj->position = GetPosInTile(pos);
		obj->orientation = glm::radians(rot);
		obj->scale = scale;

		obj->transform = glm::scale(glm::translate(glm::identity<glm::mat4x4>(), obj->position), obj->scale);
		obj->transform *= glm::mat4_cast(glm::quat{ obj->orientation });

		m_objects.push_back(obj);
	}

	uint32_t areas_count = 0;
	if (!reader.read(areas_count))
		return false;

	for (uint32_t j = 0; j < areas_count; ++j)
	{
		std::string name, shape;
		uint32_t type = 0;
		glm::ivec2 loc;
		glm::vec3 pos, rot, scale, size;

		if (!reader.read_multiple(name, type, shape))
			return false;

		if (!reader.read_multiple(loc, pos, rot, scale, size))
			return false;
		loc -= glm::ivec2{ 100000, 100000 };

		std::shared_ptr<TerrainArea> area = std::make_shared<TerrainArea>();
		area->name = name;
		area->shape = shape;
		area->type = type;
		area->position = GetPosInTile(pos);
		area->orientation = glm::radians(rot);
		area->extents = size * 0.5f;

		area->transform = glm::scale(glm::translate(glm::identity<glm::mat4x4>(), area->position), glm::vec3(area->extents));
		area->transform *= glm::mat4_cast(glm::quat{ area->orientation });

		// Is scale always 1? Scale transform by extents to convert a unit cube into proper area rectangle.
		area->scale = scale;
		if (area->scale != glm::vec3(1.0f, 1.0f, 1.0f))
		{
			EQG_LOG_WARN("Area with scale not handled");
		}

		m_areas.push_back(area);
	}

	// TODO: Load lights
	uint32_t num_lights;
	if (!reader.read(num_lights))
		return false;

	for (uint32_t j = 0; j < num_lights; ++j)
	{
		std::string name, definition;
		bool static_light;
		glm::ivec2 loc;
		glm::vec3 pos, rot, scale;
		float radius;

		if (!reader.read_multiple(name, definition, static_light, loc, pos, rot, scale, radius))
			return false;
		loc -= glm::ivec2{ 100000, 100000 };

		auto newLight = std::make_shared<Light>();
		newLight->name = name;
		newLight->tag = definition;
		newLight->static_light = static_light;
		newLight->pos = GetPosInTile(pos);
		newLight->radius = radius;
		newLight->transform = glm::translate(glm::identity<glm::mat4x4>(), newLight->pos);

		m_lights.push_back(newLight);
	}

	uint32_t num_groups;
	if (!reader.read(num_groups))
		return false;

	for (uint32_t j = 0; j < num_groups; ++j)
	{
		std::string group_name;
		glm::ivec2 loc;
		glm::vec3 pos, rot, scale;
		float z_offset;

		if (!reader.read_multiple(group_name, loc, pos, rot, scale, z_offset))
			return false;
		loc -= glm::ivec2{ 100000, 100000 };

		std::shared_ptr<TerrainObjectGroup> group = std::make_shared<TerrainObjectGroup>();
		group->name = group_name;
		group->position = tile_pos + glm::vec3(pos.x, pos.y, pos.z + (scale.z * z_offset));
		group->orientation = glm::radians(rot);
		group->scale = scale;

		group->transform = glm::scale(glm::translate(glm::identity<glm::mat4x4>(), group->position), group->scale);
		group->transform *= glm::mat4_cast(glm::quat{ group->orientation });

		TerrainObjectGroupDefinition* group_definition = terrain->GetObjectGroupDefinition(group_name);
		if (group_definition != nullptr)
		{
			group->Initialize(group_definition);
		}

		// Keep the group even if we lack a definition. At least this way we can show a marker or something.
		m_groups.push_back(group);
	}

	return true;
}


//============================================================================

void TerrainObjectGroup::Initialize(TerrainObjectGroupDefinition* definition)
{
	m_definition = definition;

	objects.reserve(definition->objects.size());
	for (const auto& object : definition->objects)
	{
		auto newObject = std::make_shared<TerrainObject>();
		newObject->name = to_upper_copy(object->name) + "_ACTORDEF";
		newObject->object_id = object->object_id;
		newObject->group = this;

		// Set positions to be in world space
		newObject->transform = transform * object->transform;

		glm::quat orient;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(newObject->transform, newObject->scale, orient, newObject->position, skew, perspective);
		newObject->orientation = glm::eulerAngles(orient);

		objects.push_back(newObject);
	}

	areas.reserve(definition->areas.size());
	for (const auto& area : definition->areas)
	{
		auto newArea = std::make_shared<TerrainArea>();
		newArea->name = area->name;
		newArea->shape = "Box";
		newArea->type = 0;
		newArea->group = this;

		// Set positions to be in world space
		newArea->transform = transform * area->transform;
		newArea->scale = glm::vec3(1.0f);

		glm::quat orient;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(newArea->transform, newArea->extents, orient, newArea->position, skew, perspective);
		newArea->orientation = glm::eulerAngles(orient);

		areas.push_back(newArea);
	}
}

TerrainTile::TerrainTile(TerrainSystem* terrain_)
	: terrain(terrain_)
{
}

TerrainTile::~TerrainTile()
{
}

//============================================================================

bool TerrainObjectGroupDefinitionObjectElement::Load(const std::vector<std::string>& tokens, size_t& k)
{
	while (k < tokens.size())
	{
		const auto& token = tokens[k++];

		if (token == "*END_OBJECT")
		{
			transform = glm::scale(glm::translate(glm::identity<glm::mat4x4>(), position), glm::vec3(scale));
			transform *= glm::mat4_cast(glm::quat{ orientation });
			return true;
		}

		if (token == "*NAME")
		{
			if (k + 1 >= tokens.size())
				break;

			name = tokens[k++];
		}
		else if (token == "*POSITION")
		{
			if (k + 2 >= tokens.size())
				break;

			position = { std::stof(tokens[k]), std::stof(tokens[k + 1]), std::stof(tokens[k + 2]) };
			k += 3;
		}
		else if (token == "*ROTATION")
		{
			if (k + 2 >= tokens.size())
				break;

			orientation = glm::radians(glm::vec3{ std::stof(tokens[k]), std::stof(tokens[k + 1]), std::stof(tokens[k + 2]) });
			k += 3;
		}
		else if (token == "*SCALE")
		{
			if (k >= tokens.size())
				break;

			scale = std::stof(tokens[k++]);
		}
		else if (token == "*FILE")
		{
			if (k + 1 >= tokens.size())
				break;

			ElementFile file;
			file.tag = tokens[k++];
			file.fileName = tokens[k++];

			files.push_back(file);
		}
	}

	return false;
}

bool TerrainObjectGroupDefinitionAreaElement::Load(const std::vector<std::string>& tokens, size_t& k)
{
	while (k < tokens.size())
	{
		const auto& token = tokens[k++];

		if (token == "*END_AREA")
		{
			transform = glm::scale(glm::translate(glm::identity<glm::mat4x4>(), position), extents);
			transform *= glm::mat4_cast(glm::quat{ orientation });
			return true;
		}

		if (token == "*NAME")
		{
			if (k + 1 >= tokens.size())
				break;

			name = tokens[k++];
		}
		else if (token == "*POSITION")
		{
			if (k + 3 >= tokens.size())
				break;

			position = { std::stof(tokens[k + 1]), std::stof(tokens[k + 2]), std::stof(tokens[k + 3]) };
			k += 3;
		}
		else if (token == "*ROTATION")
		{
			if (k + 3 >= tokens.size())
				break;

			orientation = glm::radians(glm::vec3{ std::stof(tokens[k + 1]), std::stof(tokens[k + 2]), std::stof(tokens[k + 3]) });
			k += 3;
		}
		else if (token == "*EXTENTS")
		{
			if (k + 1 >= tokens.size())
				break;

			extents = { std::stof(tokens[k + 1]), std::stof(tokens[k + 2]), std::stof(tokens[k + 3]) };
			k += 3;
		}
	}

	return false;
}

bool TerrainObjectGroupDefinition::Load(Archive* archive, const std::string& group_name)
{
	this->name = group_name;
	std::string fileName = group_name + ".tog";

	std::vector<char> tog_buffer;
	if (!archive->Get(fileName, tog_buffer))
	{
		return false;
	}
	
	EQG_LOG_TRACE("Loading terrain object group {}.tog.", group_name);

	std::vector<std::string> tokens = ParseConfigFile(tog_buffer.data(), tog_buffer.size());

	for (size_t k = 0; k < tokens.size();)
	{
		const auto& token = tokens[k++];

		if (token == "*BEGIN_OBJECT")
		{
			auto object = std::make_shared<TerrainObjectGroupDefinitionObjectElement>();

			if (object->Load(tokens, k))
				objects.push_back(object);
		}
		else if (token == "*BEGIN_AREA")
		{
			auto area = std::make_shared<TerrainObjectGroupDefinitionAreaElement>();

			if (area->Load(tokens, k))
				areas.push_back(area);
		}
	}

	return true;
}

static uint32_t s_waterSheetDataIndex = 10000;

WaterSheet::WaterSheet(TerrainSystem* terrain, const std::string& name, const std::shared_ptr<WaterSheetData>& data)
	: m_name(name)
	, m_data(data)
	, m_terrain(terrain)
	, m_resourceMgr(terrain->GetResourceManager())
{
	if (m_data == nullptr)
	{
		m_data = std::make_shared<WaterSheetData>(s_waterSheetDataIndex++);
	}
}

bool WaterSheet::Init(const std::vector<std::string>& tokens, size_t& k)
{
	while (k < tokens.size())
	{
		const auto& token = tokens[k++];

		if (token == "*END_SHEET")
		{
			break;
		}

		if (!ParseToken(token, tokens, k))
		{
			m_data->ParseToken(token, tokens, k);
		}
	}

	int units_per_tile = static_cast<int>(m_terrain->GetParams().units_per_tile + 0.5f);
	int sheet_start_x = static_cast<int>(m_minX + 0.5f) % units_per_tile;
	int sheet_start_y = static_cast<int>(m_minY + 0.5f) % units_per_tile;

	if (sheet_start_x < 0)
		sheet_start_x += units_per_tile;
	if (sheet_start_y < 0)
		sheet_start_y += units_per_tile;

	m_definitionName = fmt::format("WS_{}_{}_{}_{}_{}_ACTORDEF", m_data->GetIndex(), sheet_start_x, sheet_start_y,
		static_cast<int>(sheet_start_x + m_maxX - m_minX), static_cast<int>(sheet_start_y + m_maxY - m_minY));

	ActorDefinitionPtr pActorDef = m_resourceMgr->Get<ActorDefinition>(m_definitionName);
	if (!pActorDef)
	{
		pActorDef = CreateActorDefinition();
	}

	// Create a simple actor to represent the placement of the model. Place its center in the center of the tile.
	glm::vec2 center = (glm::vec2(m_maxX, m_maxY) - glm::vec2(m_minX, m_minY)) / 2.0f;
	glm::vec3 orientation(0.0f);
	glm::vec3 position = glm::vec3(glm::vec2(m_minX, m_minY) + center, m_zHeight);

	m_actor = m_resourceMgr->CreateSimpleActor("", pActorDef, position, orientation, 1.0f, eCollisionVolumeNone);
	m_actor->GetSimpleModel()->InitBatchInstances();

	return true;
}

bool WaterSheet::ParseToken(const std::string& token, const std::vector<std::string>& tokens, size_t& k)
{
	if (token == "*MINX")
	{
		if (k < tokens.size())
		{
			m_minX = std::stof(tokens[k++]);
		}

		return true;
	}

	if (token == "*MINY")
	{
		if (k < tokens.size())
		{
			m_minY = std::stof(tokens[k++]);
		}

		return true;
	}
	
	if (token == "*MAXX")
	{
		if (k < tokens.size())
		{
			m_maxX = std::stof(tokens[k++]);
		}

		return true;
	}
	
	if (token == "*MAXY")
	{
		if (k < tokens.size())
		{
			m_maxY = std::stof(tokens[k++]);
		}

		return true;
	}

	if (token == "*ZHEIGHT")
	{
		if (k < tokens.size())
		{
			m_zHeight = std::stof(tokens[k++]);
		}

		return true;
	}

	return false;
}

ActorDefinitionPtr WaterSheet::CreateActorDefinition()
{
	glm::vec2 span = glm::vec2(m_maxX - m_minX, m_maxY - m_minY);
	glm::vec2 half = span * 0.5f;
	glm::ivec2 numQuads;
	int numVertsPerPlane;
	std::vector<SEQMVertex> vertices;

	// Generate geometry for the water sheet
	if (m_data->GetIndex() > 0 && m_data->GetIndex() < 10000)
	{
		numQuads = {
			std::max(static_cast<uint32_t>(span.x / m_terrain->GetParams().units_per_vert), 1u),
			std::max(static_cast<uint32_t>(span.y / m_terrain->GetParams().units_per_vert), 1u)
		};
		numVertsPerPlane = (numQuads.x + 1) * (numQuads.y + 1);

		// Vertices are doubled because we create double-sided water quads.
		vertices.resize(numVertsPerPlane * 2);

		glm::vec2 start = {
			fmodf(m_minX, m_terrain->GetParams().units_per_tile),
			fmodf(m_minY, m_terrain->GetParams().units_per_tile)
		};
		if (start.x < 0) start.x += m_terrain->GetParams().units_per_tile;
		if (start.y < 0) start.y += m_terrain->GetParams().units_per_tile;

		float verts_per_tile_divisor = static_cast<float>(m_terrain->GetParams().verts_per_tile - 1);

		for (int indexX = 0; indexX <= numQuads.x; ++indexX)
		{
			for (int indexY = 0; indexY <= numQuads.y; ++indexY)
			{
				glm::vec2 pos = -half + span * (glm::vec2(indexX, indexY) / glm::vec2(numQuads));

				// Upward facing quad
				uint32_t vertexIndex = indexY * (numQuads.x + 1) + indexX;
				SEQMVertex& vertex = vertices[vertexIndex];
				vertex.pos = glm::vec3(pos, 0.0f);
				vertex.uv = glm::round((pos + half + start) / m_terrain->GetParams().units_per_vert) / verts_per_tile_divisor;
				vertex.uv2 = vertex.uv;
				vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
				vertex.color = 0xffffffff;

				// Downward facing quad
				SEQMVertex& vertex2 = vertices[numVertsPerPlane + vertexIndex];
				vertex2 = vertex;
				vertex2.normal.z = -1.0f;
			}
		}
	}
	else
	{
		int numQuads_ = std::min<int>(34, static_cast<int>(std::max<float>(half.x, half.y) * 2.0f) / 20) + 1;
		numQuads = glm::vec2(numQuads_, numQuads_);
		numVertsPerPlane = (numQuads.x + 1) * (numQuads.y + 1);

		// Vertices are doubled because we create double-sided water quads.
		vertices.resize(numVertsPerPlane * 2);

		glm::vec2 maxUV = glm::vec2(1.0f);

		if (half.x > half.y)
			maxUV.x = half.y / half.x;
		if (half.y > half.x)
			maxUV.y = half.x / half.y;

		for (int indexX = 0; indexX <= numQuads.x; ++indexX)
		{
			for (int indexY = 0; indexY <= numQuads.y; ++indexY)
			{
				uint32_t vertexIndex = indexY * (numQuads.x + 1) + indexX;
				glm::vec2 ratio = glm::vec2(indexX, indexY) / glm::vec2(numQuads);

				// Upward facing quad
				SEQMVertex& vertex = vertices[vertexIndex];
				vertex.pos = glm::vec3(-half + span * ratio, 0.0f);
				vertex.uv = maxUV * ratio;
				vertex.uv2 = vertex.uv;
				vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);
				vertex.color = 0xffffffff;

				// Downward facing quad
				SEQMVertex& vertex2 = vertices[numVertsPerPlane + vertexIndex];
				vertex2 = vertex;
				vertex2.normal.z = -1.0f;
			}
		}
	}

	// double the vertices for double-sided quads
	uint32_t numFacesPerPlane = numQuads.x * numQuads.y * 2;
	std::vector<SEQMFace> faces(numFacesPerPlane * 2);

	for (int indexX = 0; indexX < numQuads.x; ++indexX)
	{
		for (int indexY = 0; indexY < numQuads.y; ++indexY)
		{
			uint32_t indices[4];
			indices[0] = indexX + indexY * (numQuads.x + 1);
			indices[1] = indices[0] + 1;
			indices[2] = indices[0] + numQuads.x + 2;
			indices[3] = indices[0] + numQuads.x + 1;

			// Quad 1, Triangle 1: indices 0, 1, 2
			uint32_t faceIndex = 2 * (indexX + indexY * numQuads.x);
			SEQMFace& face = faces[faceIndex];
			face.vertices[0] = indices[0];
			face.vertices[1] = indices[1];
			face.vertices[2] = indices[2];
			face.material = 0;
			face.flags = EQG_FACEFLAG_PASSABLE;

			// Quad 1, Triangle 2: indices 0, 2, 3
			SEQMFace& face2 = faces[faceIndex + 1];
			face2.vertices[0] = indices[0];
			face2.vertices[1] = indices[2];
			face2.vertices[2] = indices[3];
			face2.material = 0;
			face2.flags = EQG_FACEFLAG_PASSABLE;

			// Quad 2, Triangle 1: indices 0, 2, 1
			SEQMFace& face3 = faces[faceIndex + numFacesPerPlane];
			face3.vertices[0] = numVertsPerPlane + indices[0];
			face3.vertices[1] = numVertsPerPlane + indices[2];
			face3.vertices[2] = numVertsPerPlane + indices[1];
			face3.material = 0;
			face3.flags = EQG_FACEFLAG_PASSABLE;

			// Quad 2, Triangle 2: indices 0, 3, 2
			SEQMFace& face4 = faces[faceIndex + numFacesPerPlane + 1];
			face4.vertices[0] = numVertsPerPlane + indices[0];
			face4.vertices[1] = numVertsPerPlane + indices[3];
			face4.vertices[2] = numVertsPerPlane + indices[2];
			face4.material = 0;
			face4.flags = EQG_FACEFLAG_PASSABLE;
		}
	}

	m_vertices = std::move(vertices);
	m_faces = std::move(faces);

	// Generate a simple model definition.
	MaterialPalettePtr palette = std::make_shared<MaterialPalette>("", 1);
	palette->SetMaterial(0, m_data->CreateMaterial(m_resourceMgr));
	std::string tag = fmt::format("{}_SMD", m_name);

	// Create a SimpleModelDefinition from the liquid data
	SimpleModelDefinitionPtr pModelDef = m_resourceMgr->CreateSimpleModelDefinition();
	if (!pModelDef->InitFromEQMData(
		tag,
		(uint32_t)m_vertices.size(),
		m_vertices.data(),
		(uint32_t)m_faces.size(),
		m_faces.data(),
		0, nullptr,
		0, nullptr,
		palette))
	{
		EQG_LOG_ERROR("Failed to create Simple Model Definition from water sheet. tag={}", tag);
		return nullptr;
	}

	pModelDef->InitStaticData();
	m_resourceMgr->Add(pModelDef);

	// Create an ActorDefinition to represent the model.
	ActorDefinitionPtr pActorDef = std::make_shared<ActorDefinition>(m_definitionName, pModelDef);
	m_resourceMgr->Add(pActorDef);

	return pActorDef;
}

WaterSheetData::WaterSheetData(uint32_t index)
	: m_index(index)
{
}

bool WaterSheetData::Init(const std::vector<std::string>& tokens, size_t& k)
{
	while (k < tokens.size())
	{
		const auto& token = tokens[k++];

		if (token == "*ENDWATERSHEETDATA")
		{
			break;
		}

		ParseToken(token, tokens, k);
	}

	return true;
}

bool WaterSheetData::ParseToken(const std::string& token, const std::vector<std::string>& tokens, size_t& k)
{
	if (token == "*INDEX")
	{
		if (k < tokens.size())
		{
			m_index = (uint32_t)std::stoul(tokens[k++]);
		}

		return true;
	}

	if (token == "*FRESNELBIAS")
	{
		if (k < tokens.size())
		{
			m_fresnelBias = std::stof(tokens[k++]);
		}

		return true;
	}
	
	if (token == "*FRESNELPOWER")
	{
		if (k < tokens.size())
		{
			m_fresnelPower = std::stof(tokens[k++]);
		}

		return true;
	}

	if (token == "*REFLECTIONAMOUNT")
	{
		if (k < tokens.size())
		{
			m_reflectionAmount = std::stof(tokens[k++]);
		}

		return true;
	}

	if (token == "*UVSCALE")
	{
		if (k < tokens.size())
		{
			m_uvScale = std::stof(tokens[k++]);
		}

		return true;
	}

	if (token == "*REFLECTIONCOLOR")
	{
		if (k + 3 < tokens.size())
		{
			m_reflectionColor = glm::vec4(
				std::stof(tokens[k]),
				std::stof(tokens[k+1]),
				std::stof(tokens[k+2]),
				std::stof(tokens[k+3])
			);
			k += 4;
		}

		return true;
	}
	
	if (token == "*WATERCOLOR1")
	{
		if (k + 3 < tokens.size())
		{
			m_waterColor1 = glm::vec4(
				std::stof(tokens[k]),
				std::stof(tokens[k + 1]),
				std::stof(tokens[k + 2]),
				std::stof(tokens[k + 3])
			);
			k += 4;
		}

		return true;
	}
	
	if (token == "*WATERCOLOR2")
	{
		if (k + 3 < tokens.size())
		{
			m_waterColor2 = glm::vec4(
				std::stof(tokens[k]),
				std::stof(tokens[k + 1]),
				std::stof(tokens[k + 2]),
				std::stof(tokens[k + 3])
			);
			k += 4;
		}

		return true;
	}
	
	if (token == "*NORMALMAP")
	{
		if (k < tokens.size())
		{
			m_normalMap = tokens[k++];
		}
		
		return true;
	}

	if (token == "*ENVIRONMENTMAP")
	{
		if (k < tokens.size())
		{
			m_environmentMap = tokens[k++];
		}

		return true;
	}

	return false;
}

MaterialPtr WaterSheetData::CreateMaterial(ResourceManager* resourceMgr)
{
	if (m_material)
	{
		return m_material;
	}

	m_material = std::make_shared<Material>();

	SMaterialInfo info;
	info.tag = fmt::format("WS{}_MAT", m_index);
	info.type = MaterialType_AlphaWater;

	info.params.emplace_back("e_TextureDiffuse0", "Resources\\WaterSwap\\water_c.bmp");
	info.params.emplace_back("e_TextureNormal0", m_normalMap);
	info.params.emplace_back("e_TextureEnvironment0", m_environmentMap);
	info.params.emplace_back("e_fFresnelBias", m_fresnelBias);
	info.params.emplace_back("e_fFresnelPower", m_fresnelPower);
	info.params.emplace_back("e_fReflectionAmount", m_reflectionAmount);
	info.params.emplace_back("e_fReflectionColor", m_reflectionColor);
	info.params.emplace_back("e_fWaterColor1", m_waterColor1);
	info.params.emplace_back("e_fWaterColor2", m_waterColor2);
	info.params.emplace_back("e_fUVScale", m_uvScale);

	m_material->InitFromMaterialInfo(resourceMgr, info);
	resourceMgr->Add(info.tag, m_material);

	return m_material;
}

//=================================================================================================

AreaEnvironment ParseAreaTag(std::string_view areaTag)
{
	if (areaTag.length() < 4)
		return {};

	if (areaTag[0] == 'A')
	{
		if (areaTag.starts_with("AWT"))
		{
			return AreaEnvironment::UnderWater;
		}

		if (areaTag.starts_with("ALV"))
		{
			return AreaEnvironment::UnderLava;
		}

		if (areaTag.starts_with("AVW"))
		{
			return AreaEnvironment::UnderIceWater;
		}

		if (areaTag.starts_with("APK"))
		{
			return AreaEnvironment::Kill;
		}

		if (areaTag.starts_with("ATP"))
		{
			// Old style teleport. No coordinates, just index.
			AreaEnvironment env = AreaEnvironment::TeleportIndex;
			env.teleportIndex = (int16_t)str_to_int(areaTag.substr(3), -1);

			return env;
		}

		if (areaTag.starts_with("ASL"))
		{
			return AreaEnvironment::Slippery;
		}
	}
	else
	{
		AreaEnvironment env;

		if (areaTag.starts_with("DR"))
		{
			env = AreaEnvironment::Type_None;
		}
		else if (areaTag.starts_with("WT"))
		{
			env = AreaEnvironment::UnderWater;
		}
		else if (areaTag.starts_with("LA"))
		{
			env = AreaEnvironment::UnderLava;
		}
		else if (areaTag.starts_with("SL"))
		{
			env = AreaEnvironment::UnderSlime;
		}
		else if (areaTag.starts_with("VW"))
		{
			env = AreaEnvironment::UnderIceWater;
		}
		else if (areaTag.starts_with("W2"))
		{
			env = AreaEnvironment::UnderWater2;
		}
		else if (areaTag.starts_with("W3"))
		{
			env = AreaEnvironment::UnderWater3;
		}

		if (areaTag[2] == 'P')
		{
			env.flags |= AreaEnvironment::Kill;
		}

		if (areaTag.size() >= 5)
		{
			if (areaTag[3] == 'T' && areaTag[4] == 'P')
			{
				env.flags |= AreaEnvironment::Teleport; // Additional info is added elsewhere
			}
		}

		if (areaTag.size() >= 32)
		{
			if (areaTag[31] == 'M')
			{
				// An unused area type.
			}
		}

		if (areaTag.size() >= 33)
		{
			if (areaTag[32] == 'S')
			{
				env.flags |= AreaEnvironment::Slippery;
			}
		}

		if (areaTag.size() >= 34)
		{
			if (areaTag[33] == 'P' || areaTag[33] == 'F')
			{
				// An unused area type
			}
		}

		return env;
	}

	return {};
}

bool ParseAreaTeleportTag(std::string_view areaTag, AreaTeleport& teleport)
{
	if (areaTag.length() < 17)
		return false;

	teleport.tag = std::string(areaTag);
	teleport.teleportIndex = 0;

	std::string_view zoneId = areaTag.substr(5, 5); // [5-19)
	teleport.zoneId = str_to_int(zoneId, 0);

	std::string_view x = areaTag.substr(10, 6); // [10-16)
	if (areaTag.length() < 31 || teleport.zoneId == 255)
	{
		teleport.teleportIndex = str_to_int(x, 0);
		return true;
	}
	teleport.position.x = str_to_float(x, 0);

	std::string_view y = areaTag.substr(16, 6); // [16-22)
	teleport.position.y = str_to_float(y, 0);

	std::string_view z = areaTag.substr(22, 6); // [22-28)
	teleport.position.z = str_to_float(z, 0);

	std::string_view heading = areaTag.substr(28, 3); // [28-31)
	teleport.heading = str_to_float(heading, 0)/* * EQ_TO_RAD*/;

	return true;
}

//=================================================================================================

Terrain::Terrain()
	: m_bbMin(std::numeric_limits<float>::max())
	, m_bbMax(std::numeric_limits<float>::min())
{
}

Terrain::~Terrain()
{
}

bool Terrain::InitFromWLDData(const STerrainWLDData& wldData)
{
	uint32_t numFaces = 0;
	uint32_t numVertices = 0;
	uint32_t faceOffset = 0;
	uint32_t vertexOffset = 0;

	// Count the total number of vertices and faces.
	for (const SRegionWLDData& region : wldData.regions)
	{
		if (region.regionSprite)
		{
			numFaces += region.regionSprite->numFaces;
			numVertices += region.regionSprite->numVertices;
		}
		else
		{
			//EQG_LOG_WARN("Found region '{}' with no sprite instance!", region.tag);
		}
	}

	m_vertices.resize(numVertices);
	m_uvs.resize(numVertices);
	m_normals.resize(numVertices);
	m_rgbColors.resize(numVertices);
	m_rgbTints.resize(numVertices);
	m_faces.resize(numFaces);

	m_numWLDRegions = (uint32_t)wldData.regions.size();

	//
	uint32_t numWLDAreas = (uint32_t)wldData.areas.size();
	m_wldAreas.resize(numWLDAreas);
	m_wldAreaIndices.resize(m_numWLDRegions);

	for (uint32_t areaNum = 0; areaNum < numWLDAreas; ++areaNum)
	{
		const SAreaWLDData& wldArea = wldData.areas[areaNum];
		if (wldArea.areaNum >= m_wldAreas.size())
		{
			EQG_LOG_WARN("Area num {} is out of range (size: {})",
				wldArea.areaNum, m_wldAreas.size());
			continue;
		}
		SArea& area = m_wldAreas[wldArea.areaNum];

		area.tag = wldArea.tag;
		area.userData = wldArea.userData;
		area.regionNumbers.resize(wldArea.numRegions);
		memcpy(area.regionNumbers.data(), wldArea.regions, sizeof(uint32_t) * wldArea.numRegions);

		for (uint32_t region : area.regionNumbers)
		{
			m_wldAreaIndices[region] = areaNum;
		}
	}

	uint32_t numWLDRegions = (uint32_t)wldData.regions.size();

	for (uint32_t regionNum = 0; regionNum < numWLDRegions; ++regionNum)
	{
		// Read the region number from the tag string and add this region's center to that area.
		const SRegionWLDData& wldRegion = wldData.regions[regionNum];
		if (wldRegion.tag.size() > 1)
		{
			int regionId = str_to_int(wldRegion.tag.substr(1), -1);
			if (regionId == -1)
			{
				EQG_LOG_WARN("Bad region tag {}", wldRegion.tag);
			}

			if (regionId >= 0 && regionId < (int)numWLDRegions && !m_wldAreas.empty())
			{
				uint32_t areaId = m_wldAreaIndices[regionId];
				if (areaId < m_wldAreas.size())
				{
					m_wldAreas[areaId].centers.push_back(wldRegion.sphereCenter);
				}
			}
		}

		if (SDMSpriteDef2WLDData* pSprite = wldRegion.regionSprite.get())
		{
			if (!m_materialPalette && pSprite->materialPalette)
			{
				m_materialPalette = pSprite->materialPalette;
			}

			// Copy over vertices and indices
			for (uint32_t i = 0; i < pSprite->numVertices; ++i)
			{
				glm::vec3& outVertex = m_vertices[vertexOffset + i];
				glm::i16vec3* inVertex = pSprite->vertices + i;

				outVertex = glm::vec3(inVertex->x, inVertex->y, inVertex->z) * pSprite->vertexScaleFactor + pSprite->centerOffset;

				if (pSprite->numUVs)
				{
					if (pSprite->uvsOldForm)
					{
						glm::vec2& outUV = m_uvs[vertexOffset + i];
						glm::u16vec2* inUV = pSprite->uvsOldForm + i;

						outUV = glm::vec2(inUV->x, inUV->y) * S3D_UV_TO_FLOAT;
					}
					else
					{
						glm::vec2& outUV = m_uvs[vertexOffset + i];
						glm::vec2* inUV = pSprite->uvs + i;

						outUV = *inUV;
					}
				}
				else
				{
					m_uvs[vertexOffset + i] = glm::vec2(0.0f, 0.0f);
				}

				if (pSprite->numVertexNormals)
				{
					glm::vec3& outNormal = m_normals[vertexOffset + i];
					glm::u8vec3* inNormal = pSprite->vertexNormals + i;

					outNormal = glm::vec3(inNormal->x, inNormal->y, inNormal->z) * S3D_NORM_TO_FLOAT;
				}
				else
				{
					m_normals[vertexOffset + i] = glm::vec3(0.0f, 0.0f, 0.0f);
				}

				if (pSprite->numRGBs)
				{
					m_rgbColors[vertexOffset + i] = pSprite->rgbData[i];
				}
				else
				{
					m_rgbColors[vertexOffset + i] = 0xff1f1f1f;
				}

				m_rgbTints[vertexOffset + i] = 0xff808080;

				m_bbMin = glm::min(m_bbMin, outVertex);
				m_bbMax = glm::max(m_bbMax, outVertex);
			}

			// Initialize faces and materials
			WLD_MATERIALGROUP* faceGroups = pSprite->faceMaterialGroups;
			uint32_t numMaterials = pSprite->materialPalette->GetNumMaterials();

			uint32_t curGroup = 0;
			uint32_t groupSize = faceGroups[curGroup].group_size;
			int16_t materialIndex = faceGroups[curGroup].material_index;
			if (materialIndex > (int)numMaterials)
				materialIndex = -1;

			uint32_t groupStart = 0;
			Material* pMaterial = materialIndex >= 0 && materialIndex < (int16_t)numMaterials
				? pSprite->materialPalette->GetMaterial(materialIndex) : nullptr;

			for (uint32_t face = 0; face < pSprite->numFaces; ++face)
			{
				if (face > groupStart + groupSize)
				{
					// Increment to next group
					if (curGroup < pSprite->numFaceMaterialGroups - 1)
						++curGroup;

					groupSize = faceGroups[curGroup].group_size;
					materialIndex = faceGroups[curGroup].material_index;
					if (materialIndex > (int)numMaterials)
						materialIndex = -1;

					groupStart = face;

					pMaterial = materialIndex >= 0 && materialIndex < (int16_t)numMaterials
						? pSprite->materialPalette->GetMaterial(materialIndex) : nullptr;
				}

				SFace& outFace = m_faces[faceOffset + face];
				WLD_DMFACE2* inFace = pSprite->faces + face;

				outFace.indices = glm::uvec3{ inFace->indices[0], inFace->indices[1], inFace->indices[2] } + faceOffset;
				outFace.materialIndex = materialIndex;
				outFace.flags = 0
					| (inFace->flags & S3D_FACEFLAG_PASSABLE ? EQG_FACEFLAG_PASSABLE : 0)
					| (!pMaterial || pMaterial->IsTransparent() ? EQG_FACEFLAG_TRANSPARENT : 0);
			}

			vertexOffset += pSprite->numVertices;
			faceOffset += pSprite->numFaces;
		}
	}

	// Assign areas and teleports
	if (!m_wldAreas.empty())
	{
		m_wldAreaEnvironments.resize(m_numWLDRegions);

		for (const SArea& area : m_wldAreas)
		{
			std::string_view areaTag = !area.userData.empty() ? area.userData : area.tag;

			AreaEnvironment env = areaTag;
			if (env.flags & AreaEnvironment::Teleport)
			{
				AreaTeleport teleport;
				if (ParseAreaTeleportTag(areaTag, teleport))
				{
					if (teleport.teleportIndex != 0)
					{
						env.flags |= AreaEnvironment::TeleportIndex;
						env.flags &= (AreaEnvironment::Flags)~AreaEnvironment::Teleport;
					}
					else if (teleport.zoneId == 255)
					{
						env.flags |= AreaEnvironment::TeleportIndex;
						env.flags &= (AreaEnvironment::Flags)~AreaEnvironment::Teleport;
					}
					else
					{
						teleport.teleportIndex = (int)m_teleports.size();
						m_teleports.push_back(teleport);
					}

					env.teleportIndex = (int16_t)teleport.teleportIndex;
				}
				else
				{
					EQG_LOG_WARN("Failed to parse teleport tag: {}", areaTag);
				}
			}

			for (uint32_t regionNum : area.regionNumbers)
			{
				m_wldAreaEnvironments[regionNum] = env;
			}
		}
	}

	m_wldBspTree = wldData.worldTree;
	m_constantAmbientColor = wldData.constantAmbientColor;

	return true;
}

} // namespace eqg
