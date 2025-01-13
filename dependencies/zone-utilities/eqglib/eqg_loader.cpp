
#include "pch.h"
#include "eqg_loader.h"

#include "archive.h"
#include "buffer_reader.h"
#include "eqg_structs.h"
#include "log_internal.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace eqg {

EQGLoader::EQGLoader()
{
}

EQGLoader::~EQGLoader()
{
}
	
bool EQGLoader::Load(Archive* archive)
{
	if (archive == nullptr)
		return false;

	m_archive = archive;

	fs::path zonFilePath = fs::path(archive->GetFilePath()).replace_extension(".zon");
	bool loaded_anything = false;

	// Check for a .zon file that exists next to this .eqg
	std::vector<char> zonBuffer;
	FILE* pZonFile = _fsopen(zonFilePath.string().c_str(), "rb", _SH_DENYNO);
	if (pZonFile != nullptr)
	{
		fseek(pZonFile, 0, SEEK_END);
		size_t sz = ftell(pZonFile);

		rewind(pZonFile);

		zonBuffer.resize(sz);
		fread(&zonBuffer[0], sz, 1, pZonFile);

		fclose(pZonFile);
		pZonFile = nullptr;
	}

	const std::vector<std::string>& files = m_archive->GetFileNames();
	int zon_index = -1;

	// Load everything else first. Load the zone last.

	for (uint32_t idx = 0; idx < (uint32_t)files.size(); ++idx)
	{
		if (!ci_ends_with(files[idx], ".zon"))
		{
			if (ParseFile(files[idx]))
				loaded_anything = true;
		}
		else
		{
			zon_index = idx;
		}
	}

	if (!zonBuffer.empty())
	{
		if (strncmp(zonBuffer.data(), "EQTZP", 5) == 0)
		{
			if (!ParseTerrainProject(zonBuffer))
				return false;
		}
		else
		{
			std::string fileName = fs::path(zonFilePath).filename().replace_extension().string();

			if (!ParseZoneV2(zonBuffer, fileName))
				return false;
		}
	}
	else if (zon_index != -1)
	{
		if (!ParseFile(files[zon_index]))
			return false;

		loaded_anything = true;
	}

	return loaded_anything;
}

bool EQGLoader::ParseFile(const std::string& fileName)
{
	bool success = false;

	std::string_view sv = fileName;

	if (ci_ends_with(sv, ".ani"))
	{
		// Parse ANI file
		EQG_LOG_TRACE("Animation file: {}", fileName);
		success = true;
	}
	else if (!ci_ends_with(sv, ".dds")
		&& !ci_ends_with(sv, ".tga")
		&& !ci_ends_with(sv, ".lit")
		&& !ci_ends_with(sv, ".txt"))
	{
		std::vector<char> fileBuffer;

		if (!m_archive->Get(sv, fileBuffer))
			return false;

		std::string tag = to_upper_copy(sv.substr(0, sv.length() - 4));

		BufferReader reader(fileBuffer);
		char* pMagic = reader.peek<char>();

		if (strncmp(pMagic, "EQGM", 4) == 0)
		{
			// Model
			EQG_LOG_TRACE("EQGM: Model: {}", fileName);
			success = ParseModel(fileBuffer, fileName, tag);

			actor_tags.push_back(tag);
		}
		else if (strncmp(pMagic, "EQGS", 4) == 0)
		{
			// Skin
			EQG_LOG_TRACE("EQGS: Model skin: {}", fileName);
			success = true;

			//actor_tags.push_back(tag);
		}
		else if (strncmp(pMagic, "EQAL", 4) == 0)
		{
			// Animation list
			EQG_LOG_TRACE("EQAL: Animation list: {}", fileName);
			success = true;
		}
		else if (strncmp(pMagic, "EQGL", 4) == 0)
		{
			// Material layer
			EQG_LOG_TRACE("EQGL: Material layer: {}", fileName);
			success = true;
		}
		else if (strncmp(pMagic, "EQGT", 4) == 0)
		{
			// Terrain
			EQG_LOG_TRACE("EQGT: Terrain data: {}", fileName);
			success = ParseTerrain(fileBuffer, fileName, tag);
		}
		else if (strncmp(pMagic, "EQGZ", 4) == 0)
		{
			// Zone
			EQG_LOG_TRACE("EQGZ: Zone data: {}", fileName);
			success = ParseZone(fileBuffer, tag);
		}
		else if (strncmp(pMagic, "EQLOD", 5) == 0)
		{
			// LOD Data
			EQG_LOG_TRACE("EQLOD: LOD data: {}", fileName);
			success = ParseLOD(fileBuffer, tag);
		}
		else if (strncmp(pMagic, "EQTZP", 5) == 0)
		{
			// Terrain project file
			EQG_LOG_TRACE("EQTZP: Terrain project file: {}", fileName);
			success = ParseTerrainProject(fileBuffer);
		}
		else if (strncmp(pMagic, "EQOBG", 5) == 0)
		{
			// Actor group
			EQG_LOG_TRACE("EQOBG: Actor group: {}", fileName);
			success = true;
		}
	}

	return success;
}

bool EQGLoader::ParseModel(const std::vector<char>& buffer, const std::string& fileName, const std::string& tag)
{
	std::shared_ptr<eqg::Geometry> model;

	if (!LoadEQGModel(buffer, fileName, model))
	{
		return false;
	}

	model->SetName(tag + "_ACTORDEF");

	models.push_back(model);
	return true;
}

bool EQGLoader::ParseTerrain(const std::vector<char>& buffer, const std::string& fileName, const std::string& tag)
{
	std::shared_ptr<eqg::Geometry> model;

	if (!LoadEQGModel(buffer, tag, model))
	{
		return false;
	}

	terrain_model = model;
	terrain_model->name = fileName;
	return true;
}

bool EQGLoader::ParseTerrainProject(const std::vector<char>& buffer)
{
	if (buffer.size() < 5)
		return false;

	SEQZoneParameters params;

	LoadZoneParameters(buffer.data(), buffer.size(), params);

	// Load terrain data
	auto loading_terrain = std::make_shared<Terrain>(params);

	EQG_LOG_DEBUG("Parsing zone data file.");
	if (!loading_terrain->Load(m_archive))
	{
		EQG_LOG_ERROR("Failed to parse zone data.");
		return false;
	}

	EQG_LOG_DEBUG("Parsing water data file.");
	LoadWaterSheets(loading_terrain);

	EQG_LOG_DEBUG("Parsing invisible walls file.");
	LoadInvisibleWalls(loading_terrain);

	terrain = loading_terrain;

	return true;
}

void EQGLoader::LoadZoneParameters(const char* buffer, size_t size, SEQZoneParameters& params)
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

bool EQGLoader::ParseLOD(const std::vector<char>& buffer, const std::string& tag)
{
	std::shared_ptr<LODList> lodList = std::make_shared<LODList>(tag);

	if (!lodList->Init(buffer))
		return false;

	lod_lists.push_back(lodList);
	return true;
}

bool EQGLoader::LoadWaterSheets(std::shared_ptr<Terrain>& terrain)
{
	std::vector<char> wat;
	if (!m_archive->Get("water.dat", wat))
	{
		return false;
	}

	std::vector<std::string> tokens = ParseConfigFile(wat.data(), wat.size());

	std::shared_ptr<WaterSheet> ws;

	for (size_t i = 1; i < tokens.size();)
	{
		std::string_view token = tokens[i];

		if (token.compare("*WATERSHEET") == 0)
		{
			ws = std::make_shared<WaterSheet>();
			ws->SetTile(false);

			++i;
		}
		else if (token.compare("*END_SHEET") == 0)
		{
			if (ws)
			{
				EQG_LOG_TRACE("Adding finite water sheet.");
				terrain->AddWaterSheet(ws);
			}

			++i;
		}
		else if (token.compare("*WATERSHEETDATA") == 0)
		{
			ws = std::make_shared<WaterSheet>();
			ws->SetTile(true);

			++i;
		}
		else if (token.compare("*ENDWATERSHEETDATA") == 0)
		{
			if (ws)
			{
				EQG_LOG_TRACE("Adding infinite water sheet.");
				terrain->AddWaterSheet(ws);
			}

			++i;
		}
		else if (token.compare("*INDEX") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			int32_t index = std::stoi(tokens[i + 1]);
			ws->SetIndex(index);
			i += 2;
		}
		else if (token.compare("*MINX") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			float min_x = std::stof(tokens[i + 1]);
			ws->SetMinX(min_x);

			i += 2;
		}
		else if (token.compare("*MINY") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			float min_y = std::stof(tokens[i + 1]);
			ws->SetMinY(min_y);

			i += 2;
		}
		else if (token.compare("*MAXX") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			float max_x = std::stof(tokens[i + 1]);
			ws->SetMaxX(max_x);

			i += 2;
		}
		else if (token.compare("*MAXY") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			float max_y = std::stof(tokens[i + 1]);
			ws->SetMaxY(max_y);

			i += 2;
		}
		else if (token.compare("*ZHEIGHT") == 0)
		{
			if (i + 1 >= tokens.size())
			{
				break;
			}

			float z = std::stof(tokens[i + 1]);
			ws->SetZHeight(z);

			i += 2;
		}
		else
		{
			++i;
		}
	}

	return true;
}

bool EQGLoader::LoadInvisibleWalls(std::shared_ptr<Terrain>& terrain)
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
		std::shared_ptr<InvisWall> invisWall = std::make_shared<InvisWall>(name);

		reader.read(invisWall->wall_top_height);

		uint32_t vert_count;
		reader.read(vert_count);

		invisWall->verts.resize(vert_count);

		for (uint32_t j = 0; j < vert_count; ++j)
		{
			reader.read(invisWall->verts[j]);
		}

		terrain->AddInvisWall(invisWall);
	}

	return true;
}

std::vector<std::string> EQGLoader::ParseConfigFile(const char* buffer, size_t size)
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

bool EQGLoader::ParseZone(const std::vector<char>& buffer, const std::string& tag)
{
	BufferReader reader(buffer);

	SZONHeader* header = reader.read_ptr<SZONHeader>();
	if (strncmp(header->magic, "EQGZ", 4) != 0)
	{
		return false;
	}

	// Note that this function tries to load .zon file for both version 1 and 2.

	const char* string_pool = reader.read_array<const char>(header->string_pool_size);
	mesh_names.resize(header->string_pool_size);

	// translate string pool
	for (uint32_t i = 0; i < header->num_meshes; ++i)
	{
		int offset = reader.read<uint32_t>();

		if (offset != -1)
		{
			mesh_names[i] = string_pool + offset;
		}
	}

	auto& model_names = mesh_names;

	// load placables
	EQG_LOG_TRACE("Parsing model instances.");

	size_t name_index = 0;

	for (uint32_t i = 0; i < header->num_instances; ++i)
	{
		SZONInstance* instance = reader.read_ptr<SZONInstance>();

		uint32_t num_lit_verts = 0;
		uint32_t* pLitData = nullptr;

		if (header->version > 1)
		{
			num_lit_verts = reader.read<uint32_t>();
			pLitData = reader.read_array<uint32_t>(num_lit_verts);
		}
		else
		{
			// TODO: Search for .LIT file that matches this model.
			std::string litFileName = mesh_names[instance->name] + ".LIT";
			(void)litFileName;
		}

		if (header->version > 1)
		{
			while (name_index < mesh_names.size())
			{
				std::string_view mesh_name = mesh_names[name_index++];
				if (!ci_ends_with(mesh_name, ".TER") && !ci_ends_with(mesh_name, ".MOD"))
					break;
			}
		}

		if (i > 0 && instance->mesh_id != -1)
		{
			std::string_view tag_name= mesh_names[instance->mesh_id];
			tag_name = tag_name.substr(0, tag_name.size() - 4); // strip extension
			std::string instance_tag = to_upper_copy(tag_name) + "_ACTORDEF";
			std::string instance_name = header->version > 1 && name_index < model_names.size() ? model_names[name_index] : "";

			std::shared_ptr<Placeable> obj = std::make_shared<Placeable>();
			obj->tag = std::move(instance_tag);
			//obj->model_file = std::move(instance_name);
			obj->pos = instance->translation;
			obj->rotate = instance->rotation.zyx;
			obj->scale = glm::vec3(instance->scale);

			// This would be called ZoneActor_0%5d if we had a place to keep it.

			placeables.push_back(obj);
		}
	}

	EQG_LOG_TRACE("Parsing zone regions.");

	for (uint32_t i = 0; i < header->num_areas; ++i)
	{
		SZONArea* area = reader.read_ptr<SZONArea>();

		auto newArea = std::make_shared<TerrainArea>();
		newArea->name = string_pool + area->name;
		newArea->position = area->center;
		newArea->orientation = area->orientation;
		newArea->scale = glm::vec3(1.0f);
		newArea->extents = area->extents;

		newArea->transform = glm::translate(glm::identity<glm::mat4x4>(), newArea->position);
		newArea->transform *= glm::mat4_cast(glm::quat{ area->orientation });
		
		areas.push_back(newArea);
	}

	EQG_LOG_TRACE("Parsing zone lights.");

	for (uint32_t i = 0; i < header->num_lights; ++i)
	{
		SZONLight* light = reader.read_ptr<SZONLight>();

		auto newLight = std::make_shared<Light>();
		newLight->tag = std::string(string_pool + light->name) + "_LIGHTDEF";
		newLight->pos = light->pos;
		newLight->color.push_back(light->color);
		newLight->radius = light->radius;

		lights.push_back(newLight);
	}

	return true;
}

bool EQGLoader::ParseZoneV2(const std::vector<char>& buffer, const std::string& tag)
{
	return ParseZone(buffer, tag);
}

//============================================================================

LODList::LODList(const std::string& name)
	: tag(fmt::format("{}_LODLIST", name))
{
}

bool LODList::Init(const std::vector<char>& buffer)
{
	std::string_view data(buffer.data() + 7, buffer.size() - 7);

	auto lines = split_view(data, '\n');
	for (auto&& line : lines)
	{
		line = trim(line);
		if (line.empty())
			continue;

		auto element = std::make_shared<LODListElement>(line);
		if (element->type == LODListElement::LOD)
		{
			elements.push_back(element);
		}
		else if (element->type == LODListElement::Collision)
		{
			collision = element;
		}
	}

	return true;
}

LODListElement::LODListElement(std::string_view data)
{
	auto parts = split_view(data, ',');

	if (parts.size() < 2)
	{
		EQG_LOG_WARN("Invalid LOD element: {}", data);
		return;
	}

	// Level-of-detail definition
	if (parts[0] == "LOD")
	{
		type = LOD;
		definition = parts[1];

		if (parts.size() < 3)
		{
			EQG_LOG_WARN("LOD element has missing distance: {}", data);
			return;
		}
		max_distance = str_to_float(parts[2], -1);

		if (max_distance < 0)
		{
			EQG_LOG_WARN("LOD element has invalid distance: {}", data);
			max_distance = 10000.0f;
		}
	}
	// Collision definition
	else if (parts[0] == "COL")
	{
		type = Collision;
		definition = parts[1];
	}
	else
	{
		EQG_LOG_WARN("Unknown LOD element type: {}", data);
	}
}

} // namespace eqg
