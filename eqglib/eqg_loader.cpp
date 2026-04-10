
#include "pch.h"
#include "eqg_loader.h"

#include "archive.h"
#include "buffer_reader.h"
#include "eqg_material.h"
#include "eqg_resource_manager.h"
#include "eqg_structs.h"
#include "eqg_terrain_loader.h"
#include "log_internal.h"

#include "mq/base/Format.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace eqg {

EQGLoader::EQGLoader(ResourceManager* resourceMgr)
	: m_resourceMgr(resourceMgr)
{
}

EQGLoader::~EQGLoader()
{
}

bool EQGLoader::Load(Archive* archive, int loadFlags)
{
	if (archive == nullptr)
		return false;

	eqg::ScopedUseResourceManager useResourceManager(m_resourceMgr);

	m_archive = archive;
	m_loadFlags = loadFlags;

	fs::path zonFilePath = fs::path(archive->GetFilePath()).replace_extension(".zon");
	bool loaded_anything = false;

	// Check for a .zon file that exists next to this .eqg
	std::vector<char> zonBuffer;
	FILE* pZonFile = _fsopen(zonFilePath.string().c_str(), "rb", _SH_DENYNO);
	if (pZonFile != nullptr)
	{
		fseek(pZonFile, 0, SEEK_END);
		size_t sz = ftell(pZonFile);

		fseek(pZonFile, 0, SEEK_SET);

		zonBuffer.resize(sz);
		fread(&zonBuffer[0], sz, 1, pZonFile);

		fclose(pZonFile);
		pZonFile = nullptr;

		m_zonData = { reinterpret_cast<uint8_t*>(zonBuffer.data()), sz };
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

			if (!ParseZone(zonBuffer, fileName))
				return false;
		}
	}
	else if (zon_index != -1)
	{
		if (!ParseFile(files[zon_index]))
			return false;

		loaded_anything = true;
	}

	m_zonData = {};

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
	MaterialPalettePtr material_palette;
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

		std::string mat_tag = fmt::format("{}{}", tag, string_pool + material->name_index);
		std::shared_ptr<Material> material_instance = m_resourceMgr->Get<eqg::Material>(mat_tag);
		
		if (!material_instance)
		{
			material_instance = std::make_shared<Material>();
			material_instance->InitFromEQMData(material, params, m_archive, string_pool);

			m_resourceMgr->Add(mat_tag, material_instance);
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

	if (!m_resourceMgr->Add(tag, palette))
	{
		EQG_LOG_WARN("Material palette {} already exists.", tag);
		return false;
	}

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

	// copy into vector
	std::vector<SEQMFace> faces;
	faces.resize(header->num_faces);
	memcpy(faces.data(), eqm_faces, faces.size() * sizeof(SEQMFace));

	// TODO: Load .pts / EQPT
	std::vector<SParticlePoint> points;

	// TODO: Load .prt / PTCL
	std::vector<SActorParticle> particles;

	HierarchicalModelDefinitionPtr hModelPtr = nullptr;
	SimpleModelDefinitionPtr sModelPtr = nullptr;

	if (header->num_bones > 0)
	{
		// This is an animated (hierarchical) model
		SEQMBoneData* bone_data = reader.read_array<SEQMBoneData>(header->num_bones);
		std::vector<SEQMBone> bones;
		bones.reserve(header->num_bones);

		for (uint32_t i = 0; i < header->num_bones; ++i)
		{
			bones.emplace_back(string_pool + bone_data[i].name_index, bone_data[i]);
		}

		SEQMSkinData* skin_data = reader.read_array<SEQMSkinData>(header->num_vertices);

		std::string modelTag = fmt::format("{}_HMD", tag);

		if (m_resourceMgr->Contains(modelTag, ResourceType::HierarchicalModelDefinition))
		{
			return true;
		}

		hModelPtr = m_resourceMgr->CreateHierarchicalModelDefinition();

		if (!hModelPtr->InitFromEQMData(modelTag, vertices, faces, bones, points, particles,
			skin_data, vertex_data.material_palette))
		{
			EQG_LOG_ERROR("Failed to create hierarchical model definition: {}", modelTag);
			return false;
		}

		if (!m_resourceMgr->Add(hModelPtr))
		{
			EQG_LOG_ERROR("Failed to add new hierarchical model definition to resource manager: {}", modelTag);
			return false;
		}
	}
	else
	{
		std::string modelTag = fmt::format("{}_SMD", tag);

		if (m_resourceMgr->Contains(modelTag, ResourceType::SimpleModelDefinition))
		{
			return true;
		}

		sModelPtr = m_resourceMgr->CreateSimpleModelDefinition();

		if (!sModelPtr->InitFromEQMData(modelTag, std::move(vertices), std::move(faces),
			points, particles, vertex_data.material_palette))
		{
			EQG_LOG_ERROR("Failed to create simple model definition: {}", modelTag);
			return false;
		}

		if (!m_resourceMgr->Add(sModelPtr))
		{
			EQG_LOG_ERROR("Failed to add new simple model definition to resource manager: {}", modelTag);
			return false;
		}

		// FIXME
		sModelPtr->InitStaticData();
	}

	std::string definitionTag = fmt::format("{}_ACTORDEF", tag);
	ECollisionVolumeType colType = eCollisionVolumeNone;


	ActorDefinitionPtr pActorDef;

	if (hModelPtr)
	{
		pActorDef = std::make_shared<ActorDefinition>(definitionTag, hModelPtr);
	}
	else if (sModelPtr)
	{
		pActorDef = std::make_shared<ActorDefinition>(definitionTag, sModelPtr);
	}

	if (!m_resourceMgr->Add(pActorDef))
	{
		EQG_LOG_ERROR("Failed to add actor definition to resource manager: {}", definitionTag);
		return false;
	}

	pActorDef->SetCallbackTag("SPRITECALLBACK");
	pActorDef->SetCollisionVolumeType(colType);

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

	SEQMUVSet* uv_set = nullptr;

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

	std::vector<SEQMVertex> vertexStorage(header->num_vertices);
	std::span<SEQMVertex> eqmVertices;

	// Convert old vertices to new vertices
	if (vertex_data.verts == nullptr)
	{
		SEQMVertexOld* old_verts = vertex_data.verts_old;

		for (uint32_t index = 0; index < header->num_vertices; ++index)
		{
			SEQMVertex& new_vertex = vertexStorage[index];
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

		eqmVertices = vertexStorage;
	}
	else
	{
		eqmVertices = { vertex_data.verts, header->num_vertices };
	}

	std::span<SEQMFace> eqmFaces = { vertex_data.faces, header->num_faces };

	std::string litFileName = fmt::format("{}.LIT", tag);

	// We need this to exist out here so that the buffer doesn't go out of scope. We're using it
	// to own the data we pass in with the std::span...
	// ReSharper disable once CppJoinDeclarationAndAssignment
	// ReSharper disable once CppTooWideScope
	std::unique_ptr<uint8_t[]> litBuffer;
	std::span<uint32_t> litVerts;

	// Check to see if we have a LIT data from a matching v2 EQGZ file
	if (!m_zonData.empty())
	{
		BufferReader litReader(m_zonData);

		SZONHeader* header = litReader.read_ptr<SZONHeader>();
		if (strncmp(header->magic, "EQGZ", 4) == 0 && header->version == 2)
		{
			litReader.skip<char>(header->string_pool_size);
			litReader.skip<int>(header->num_meshes);
			litReader.skip<SZONInstance>();
			uint32_t numLitVerts = litReader.read<uint32_t>();

			uint32_t* litData = litReader.read_array<uint32_t>(numLitVerts);
			litVerts = { litData, numLitVerts };
		}
	}

	if (!litVerts.data())
	{
		uint32_t litSize;
		litBuffer = m_archive->Get(litFileName, litSize);
		if (!litBuffer)
			litBuffer = m_resourceMgr->ReadFile(litFileName, litSize);
		if (litBuffer)
		{
			BufferReader litReader(litBuffer, litSize);

			char* temp = litReader.read_array<char>(4);
			if (temp && strncmp(temp, "EQGP", 4) == 0)
			{
				uint32_t numLitVerts = litReader.read<uint32_t>();
				uint32_t* litData = litReader.read_array<uint32_t>(numLitVerts);

				litVerts = { litData, numLitVerts };
			}
		}
	}

	if (!litVerts.empty() && litVerts.size() != eqmVertices.size())
	{
		EQG_LOG_WARN("LIT data vertex count mismatch: {} != {}", litVerts.size(), eqmVertices.size());
		litVerts = {};
	}

	std::shared_ptr<Terrain> newTerrain = m_resourceMgr->CreateTerrain();

	newTerrain->InitFromEQGData(eqmVertices, eqmFaces, litVerts, vertex_data.material_palette);

	m_resourceMgr->SetTerrain(std::move(newTerrain));
	return true;
}

bool EQGLoader::ParseTerrainProject(const std::vector<char>& buffer)
{
	// Load terrain data
	auto loading_terrain = std::make_shared<TerrainSystem>(m_archive);

	if (!loading_terrain->Load(buffer.data(), buffer.size()))
	{
		EQG_LOG_ERROR("Failed to parse zone data.");
		return false;
	}

	m_resourceMgr->SetTerrainSystem(loading_terrain);
	return true;
}

bool EQGLoader::ParseLOD(const std::vector<char>& buffer, const std::string& tag)
{
	LODListPtr lodList = std::make_shared<LODList>(tag);

	if (!lodList->Init(buffer))
	{
		EQG_LOG_ERROR("Failed to parse LOD list for \"{}\"", tag);
		return false;
	}

	// Check if we already have this LOD list
	if (m_resourceMgr->Contains(lodList->GetTag(), ResourceType::LODList))
		return true;

	if (!m_resourceMgr->Add(lodList))
	{
		EQG_LOG_ERROR("Failed to add LOD list to resource manager: {}", lodList->GetTag());
		return false;
	}

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
	std::vector<std::string_view> mesh_names;

	// translate string pool
	for (uint32_t i = 0; i < header->num_meshes; ++i)
	{
		int offset = reader.read<uint32_t>();

		if (offset != -1)
		{
			mesh_names.push_back(string_pool + offset);
		}
		else
		{
			mesh_names.emplace_back();
		}
	}

	//m_resourceMgr->RemoveAllActors();

	EQG_LOG_TRACE("Parsing model instances.");

	std::string_view instanceName = string_pool;
	std::string tempStr;

	for (uint32_t instanceId = 0; instanceId < header->num_instances; ++instanceId)
	{
		SZONInstance* instance = reader.read_ptr<SZONInstance>();

		std::span<uint32_t> litVerts;

		if (header->version > 1)
		{
			// V2 has a uint32 after each SZONInstance for the baked lighting
			uint32_t numLitVerts = reader.read<uint32_t>();
			uint32_t* pLitData = reader.read_array<uint32_t>(numLitVerts);

			litVerts = { pLitData, numLitVerts };
		}

		if (header->version > 1)
		{
			std::string_view instance_name_sv{ instanceName };

			while (ci_ends_with(instance_name_sv, ".TER") || ci_ends_with(instance_name_sv, ".MOD"))
			{
				instanceName = instanceName.data() + instance_name_sv.length() + 1;
				instance_name_sv = instanceName;
			}
		}

		std::unique_ptr<uint8_t[]> litBuffer;
		uint32_t numLitVerts = 0;

		if (instance->mesh_id != -1
			&& (header->version == 1 || instanceId > 0))
		{
			std::string_view tag_name = mesh_names[instance->mesh_id];
			tag_name = tag_name.substr(0, tag_name.size() - 4);
			std::string definitionTag = fmt::format("{}_ACTORDEF", mq::fmt_uppercase(tag_name));

			if (header->version == 1)
			{
				tempStr = fmt::format("{}_{}", mq::fmt_uppercase(tag_name), instanceId);
				instanceName = tempStr;

				std::string litFileName = fmt::format("{}.LIT", string_pool + instance->name);

				uint32_t litSize;
				litBuffer = m_archive->Get(litFileName, litSize);
				if (!litBuffer)
					litBuffer = m_resourceMgr->ReadFile(litFileName, litSize);
				if (litBuffer)
				{
					BufferReader litReader(litBuffer, litSize);

					char* temp = litReader.read_array<char>(4);
					if (temp && strncmp(temp, "EQGP", 4) == 0)
					{
						numLitVerts = litReader.read<uint32_t>();
						uint32_t* litData = litReader.read_array<uint32_t>(numLitVerts);

						if (litData)
						{
							litVerts = { litData, numLitVerts };
						}
						else
						{
							EQG_LOG_WARN("Baked lighting file \"{}\" is corrupt!", litFileName);
						}
					}
				}
			}

			ActorDefinitionPtr pActorDef = m_resourceMgr->Get<ActorDefinition>(definitionTag);
			if (!pActorDef)
			{
				EQG_LOG_WARN("Missing actor definition '{}' while loading model instance '{}'",
					definitionTag, instanceName);
				continue;
			}

			ECollisionVolumeType volumetype = eCollisionVolumeNone;
			float boundingRadius = pActorDef->CalculateBoundingRadius();
			glm::vec3 pos = instance->translation.yzx;
			glm::vec3 orientation = glm::vec3(instance->rotation.z, instance->rotation.y, instance->rotation.x).yzx;
			float scale = instance->scale;

			std::string actorTag = fmt::format("ZoneActor_{:05}", instanceId);

			ActorPtr actor;
			
			if (pActorDef->GetHierarchicalModelDefinition())
			{
				actor = m_resourceMgr->CreateHierarchicalActor(actorTag, pActorDef, pos, orientation, scale,
					volumetype, boundingRadius, instanceId, nullptr, litVerts, instanceName);
			}
			else if (pActorDef->GetSimpleModelDefinition())
			{
				actor = m_resourceMgr->CreateSimpleActor(actorTag, pActorDef, pos, orientation, scale,
					volumetype, boundingRadius, instanceId, nullptr, litVerts, instanceName);
			}

			if (actor == nullptr)
			{
				EQG_LOG_ERROR("Failed to create actor '{}'", instanceName);
			}
			else
			{
				m_resourceMgr->AddActor(std::move(actor));
			}
		}

		if (instanceId + 1 < header->num_instances)
		{
			instanceName = instanceName.data() + instanceName.size() + 1;
		}
	}

	EQG_LOG_TRACE("Parsing zone areas.");

	std::span<SZONArea> areas;
	if (!reader.read_array(areas, header->num_areas))
	{
		EQG_LOG_ERROR("Failed to parse zone areas.");
	}
	else
	{
		if (const auto& terrain = m_resourceMgr->GetTerrain())
		{
			terrain->InitAreasFromEQGData(areas, string_pool);
		}
		else
		{
			EQG_LOG_ERROR("Missing terrain when trying to initialize areas!");
		}
	}

	EQG_LOG_TRACE("Parsing zone lights.");

	for (uint32_t i = 0; i < header->num_lights; ++i)
	{
		SZONLight* light = reader.read_ptr<SZONLight>();

		LightDefinitionPtr lightDef = m_resourceMgr->CreateLightDefinition();

		std::string lightTag = fmt::format("{}_LIGHTDEF", string_pool + light->name);

		lightDef->Init(lightTag, 1, nullptr, &light->color, 0, 1, false);

		std::shared_ptr<PointLight> pLight = m_resourceMgr->CreatePointLight(
			string_pool + light->name, lightDef,
			glm::vec3(light->pos.y, -light->pos.x, light->pos.z), light->radius);
		m_resourceMgr->AddLight(std::move(pLight));
	}

	return true;
}

//============================================================================

LODList::LODList(std::string_view name)
	: Resource(ResourceType::LODList)
	, m_tag(fmt::format("{}_LODLIST", name))
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
			m_elements.push_back(element);
		}
		else if (element->type == LODListElement::Collision)
		{
			m_collision = element;
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
		to_upper(definition);

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
		to_upper(definition);
	}
	else
	{
		EQG_LOG_WARN("Unknown LOD element type: {}", data);
	}
}

} // namespace eqg
