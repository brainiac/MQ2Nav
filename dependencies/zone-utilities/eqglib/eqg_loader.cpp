
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

struct SMaterialFaceVertexData
{
	SEQMVertexOld* verts_old;
	SEQMVertex* verts;
	SEQMFace* faces;
	std::shared_ptr<MaterialPalette> material_palette;
};

bool EQGLoader::ParseMaterialsFacesAndVertices(BufferReader& reader, const char* string_pool, const std::string& tag,
	uint32_t num_faces, uint32_t num_vertices, uint32_t num_materials, bool new_format, SMaterialFaceVertexData* out_data)
{
	// Read material data
	struct StoredMaterial
	{
		SEQMMaterial* material;
		SEQMFXParameter* parameters;
	};
	std::vector<StoredMaterial> stored_materials;
	stored_materials.resize(num_materials);

	for (uint32_t material = 0; material < num_materials; ++material)
	{
		StoredMaterial& stored_material = stored_materials[material];
		stored_material.material = reader.read_ptr<SEQMMaterial>();
		stored_material.parameters = reader.read_array<SEQMFXParameter>(stored_material.material->num_params);
	}

	// Read vertex data
	if (new_format)
	{
		out_data->verts = reader.read_array<SEQMVertex>(num_vertices);
		out_data->verts_old = nullptr;
	}
	else
	{
		out_data->verts_old = reader.read_array<SEQMVertexOld>(num_vertices);
		out_data->verts = nullptr;
	}

	// Read faces
	out_data->faces = reader.read_array<SEQMFace>(num_faces);

	// Figure out which materials are actually used, and count them up.
	std::vector<uint32_t> material_counts(num_materials, 0);
	uint32_t num_material_counts = 0;

	for (uint32_t face = 0; face < num_faces; ++face)
	{
		uint32_t material = out_data->faces[face].material;

		if (material < num_materials)
		{
			if (material_counts[material]++ == 0)
				++num_material_counts;
		}
	}

	struct UsedMaterial
	{
		uint32_t material_index;
		SEQMMaterial* material;
		int name_index;
		uint32_t first_usage;
		SEQMMaterial* first_usage_material;
		SEQMFXParameter* first_usage_material_params;
	};
	std::vector<UsedMaterial> used_materials;

	for (uint32_t material = 0; material < num_materials; ++material)
	{
		// Skip materials that are not in use.
		if (material_counts[material] == 0)
			continue;

		StoredMaterial& stored_material = stored_materials[material];

		UsedMaterial used_mat;
		used_mat.material_index = material;
		used_mat.material = stored_material.material;
		used_mat.name_index = stored_material.material->name_index;

		// find first material matching this material name.
		for (uint32_t i = 0; i < num_materials; ++i)
		{
			StoredMaterial& s = stored_materials[material];

			if (std::string_view{ string_pool + used_mat.name_index } == std::string_view{ string_pool + s.material->name_index })
			{
				used_mat.first_usage = i;
				used_mat.first_usage_material = s.material;
				used_mat.first_usage_material_params = s.parameters;
				break;
			}
		}

		used_materials.push_back(used_mat);
	}

	std::string material_tag = fmt::format("{}_MP", tag);

	// Create new material palette
	std::shared_ptr<MaterialPalette> palette = std::make_shared<MaterialPalette>(tag, num_material_counts);

	for (uint32_t cur_material = 0; cur_material < num_material_counts; ++cur_material)
	{
		// Get/Create material instance
		SEQMMaterial* material = used_materials[cur_material].first_usage_material;
		SEQMFXParameter* params = used_materials[cur_material].first_usage_material_params;

		std::string mat_tag = string_pool + material->name_index;
		std::shared_ptr<Material> material_instance;

		auto iter = this->materials.find(mat_tag);
		if (iter != this->materials.end())
		{
			material_instance = iter->second;
		}
		else
		{
			material_instance = std::make_shared<Material>();
			material_instance->InitFromEQMData(material, params, m_archive, string_pool);

			this->materials[mat_tag] = material_instance;
		}

		// This material index duplicates another material with the same index. Use this one and replace all instances of
		// the other index with this one.
		if (used_materials[cur_material].material_index != cur_material)
		{
			for (uint32_t face = 0; face < num_faces; ++face)
			{
				if (out_data->faces[face].material == used_materials[cur_material].material_index)
				{
					out_data->faces[face].material = cur_material;
				}
			}
		}

		palette->SetMaterial(cur_material, material_instance);
	}

	//if (num_materials != num_material_counts)
	//{
	//	EQG_LOG_WARN("Material palette {} has {} materials, but only {} are used.", tag, num_materials, num_material_counts);
	//}

	if (material_palettes.contains(tag))
	{
		EQG_LOG_WARN("Material palette {} already exists.", tag);
		return false;
	}

	material_palettes[tag] = palette;
	out_data->material_palette = palette;
	return true;
}

bool EQGLoader::ParseModel(const std::vector<char>& buffer, const std::string& fileName, const std::string& tag)
{
	BufferReader reader(buffer);

	SEQMHeader* header = reader.read_ptr<SEQMHeader>();

	if (strncmp(header->magic, "EQGM", 4) != 0 || header->version == 0)
	{
		EQG_LOG_ERROR("Unable to load \"{}\"; unexpected header", tag);
		return false;
	}
	
	const char* string_pool = reader.read_array<const char>(header->string_pool_size);
	bool new_vertices = header->version > 2;

	SMaterialFaceVertexData vertex_data;

	if (!ParseMaterialsFacesAndVertices(reader, string_pool, tag, header->num_faces, header->num_vertices,
		header->num_materials, new_vertices, &vertex_data))
	{
		EQG_LOG_ERROR("Failed to read materials and vertices from model data for \"{}\"", tag);
		return false;
	}

	SEQMFace* eqm_faces = vertex_data.faces;
	SEQMVertex* eqm_vertices = vertex_data.verts;
	SEQMUVSet* uv_set = nullptr;

	std::vector<SEQMVertex> vertices(header->num_vertices);

	// Load secondary UV
	if (header->version == 2)
	{
		int num_uv_sets = reader.read<int>();

		if (num_uv_sets == 1)
		{
			uv_set = reader.read_array<SEQMUVSet>(header->num_vertices);
		}
	}

	// Convert old vertices to new vertices
	if (eqm_vertices == nullptr)
	{
		SEQMVertexOld* old_verts = vertex_data.verts_old;

		for (uint32_t index = 0; index < header->num_vertices; ++index)
		{
			SEQMVertex& new_vertex = vertices[index];
			SEQMVertexOld& old_vertex = old_verts[index];

			new_vertex.pos = old_vertex.pos;
			new_vertex.normal = old_vertex.normal;
			new_vertex.color = 0xFF808080;
			new_vertex.uv = old_vertex.uv;

			if (uv_set != nullptr)
			{
				new_vertex.uv2 = uv_set[index].uv;
			}
			else
			{
				new_vertex.uv2 = { 0.0f, 0.0f };
			}
		}
	}
	else
	{
		memcpy(vertices.data(), eqm_vertices, vertices.size() * sizeof(SEQMVertex));
	}

	// TODO: Load .pts / EQPT
	// TODO: Load .prt / PTCL

	SEQMBone* bones = nullptr;
	SEQMSkinData* skin_data = nullptr;

	if (header->num_bones > 0)
	{
		// This is an animated (hierarchical) model
		bones = reader.read_array<SEQMBone>(header->num_bones);
		std::vector<std::string_view> bone_names(header->num_bones);

		for (uint32_t i = 0; i < header->num_bones; ++i)
		{
			bone_names[i] = string_pool + bones[i].name_index;
		}

		skin_data = reader.read_array<SEQMSkinData>(header->num_vertices);

		// TODO: Create hierarchical model
	}
	else
	{
		// TODO: Create simple model
	}

	std::vector<SFace> faces(header->num_faces);
	for (size_t i = 0; i < header->num_faces; ++i)
	{
		faces[i].indices[0] = eqm_faces[i].vertices[0];
		faces[i].indices[1] = eqm_faces[i].vertices[1];
		faces[i].indices[2] = eqm_faces[i].vertices[2];
		faces[i].flags = static_cast<EQG_FACEFLAGS>(eqm_faces[i].flags);
		faces[i].materialIndex = eqm_faces[i].material;
	}

	std::shared_ptr<eqg::Geometry> model = std::make_shared<eqg::Geometry>();
	model->tag = fmt::format("{}_ACTORDEF", tag);
	model->vertices = std::move(vertices);
	model->faces = std::move(faces);
	model->material_palette = vertex_data.material_palette;

	models.push_back(model);
	return true;
}

bool EQGLoader::ParseTerrain(const std::vector<char>& buffer, const std::string& fileName, const std::string& tag)
{
	BufferReader reader(buffer);

	STERHeader* header = reader.read_ptr<STERHeader>();

	if (strncmp(header->magic, "EQGT", 4) != 0 || header->version == 0)
	{
		EQG_LOG_ERROR("Unable to load TER data for \"{}\"; unexpected header", tag);
		return false;
	}

	const char* string_pool = reader.read_array<const char>(header->string_pool_size);
	bool new_vertices = header->version > 2;

	SMaterialFaceVertexData vertex_data;

	if (!ParseMaterialsFacesAndVertices(reader, string_pool, tag, header->num_faces, header->num_vertices,
		header->num_materials, new_vertices, &vertex_data))
	{
		EQG_LOG_ERROR("Failed to read materials and vertices from TER data for \"{}\"", tag);
		return false;
	}

	SEQMFace* eqm_faces = vertex_data.faces;
	SEQMVertex* eqm_vertices = vertex_data.verts;
	SEQMUVSet* uv_set = nullptr;

	std::vector<SEQMVertex> vertices(header->num_vertices);

	// Load secondary UV
	if (header->version == 2)
	{
		int num_uv_sets = reader.read<int>();

		if (num_uv_sets > 0)
		{
			uv_set = reader.read_array<SEQMUVSet>(header->num_vertices);
		}
		
		if (num_uv_sets > 1)
		{
			reader.read_array<SEQMUVSet>(header->num_vertices);
		}
	}

	// Convert old vertices to new vertices
	if (eqm_vertices == nullptr)
	{
		SEQMVertexOld* old_verts = vertex_data.verts_old;

		for (uint32_t index = 0; index < header->num_vertices; ++index)
		{
			SEQMVertex& new_vertex = vertices[index];
			SEQMVertexOld& old_vertex = old_verts[index];

			new_vertex.pos = old_vertex.pos;
			new_vertex.normal = old_vertex.normal;
			new_vertex.color = 0xFF808080;
			new_vertex.uv = old_vertex.uv;

			if (uv_set != nullptr)
			{
				new_vertex.uv2 = uv_set[index].uv;
			}
			else
			{
				new_vertex.uv2 = { 0.0f, 0.0f };
			}
		}
	}
	else
	{
		memcpy(vertices.data(), eqm_vertices, vertices.size() * sizeof(SEQMVertex));
	}

	// TODO: Load .lit data

	std::vector<SFace> faces(header->num_faces);
	for (size_t i = 0; i < header->num_faces; ++i)
	{
		faces[i].indices[0] = eqm_faces[i].vertices[0];
		faces[i].indices[1] = eqm_faces[i].vertices[1];
		faces[i].indices[2] = eqm_faces[i].vertices[2];
		faces[i].flags = static_cast<EQG_FACEFLAGS>(eqm_faces[i].flags);
		faces[i].materialIndex = eqm_faces[i].material;
	}

	// TODO: Load this data and WLD regions directly into a terrain geometry object
	std::shared_ptr<eqg::Geometry> model = std::make_shared<eqg::Geometry>();
	model->tag = fmt::format("{}_ACTORDEF", tag);
	model->vertices = std::move(vertices);
	model->faces = std::move(faces);
	model->material_palette = vertex_data.material_palette;

	terrain_model = model;
	return true;
}

bool EQGLoader::ParseTerrainProject(const std::vector<char>& buffer)
{
	// Load terrain data
	auto loading_terrain = std::make_shared<Terrain>(m_archive);

	if (!loading_terrain->Load(buffer.data(), buffer.size()))
	{
		EQG_LOG_ERROR("Failed to parse zone data.");
		return false;
	}

	terrain = loading_terrain;
	return true;
}

bool EQGLoader::ParseLOD(const std::vector<char>& buffer, const std::string& tag)
{
	std::shared_ptr<LODList> lodList = std::make_shared<LODList>(tag);

	if (!lodList->Init(buffer))
		return false;

	lod_lists.push_back(lodList);
	return true;
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
