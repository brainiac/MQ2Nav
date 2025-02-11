//
// MapGeometryLoader.cpp
//

#include "meshgen/ZoneResourceManager.h"
#include "meshgen/ApplicationConfig.h"
#include "meshgen/ZoneCollisionMesh.h"
#include "common/NavMeshData.h"
#include "mq/base/String.h"

#include <rapidjson/document.h>

#include <filesystem>
#include <sstream>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

static void RotateVertex(glm::vec3& v, float rx, float ry, float rz)
{
	glm::vec3 nv = v;

	nv.y = (cos(rx) * v.y) - (sin(rx) * v.z);
	nv.z = (sin(rx) * v.y) + (cos(rx) * v.z);

	v = nv;

	nv.x = (cos(ry) * v.x) + (sin(ry) * v.z);
	nv.z = -(sin(ry) * v.x) + (cos(ry) * v.z);

	v = nv;

	nv.x = (cos(rz) * v.x) - (sin(rz) * v.y);
	nv.y = (sin(rz) * v.x) + (cos(rz) * v.y);

	v = nv;
}

//============================================================================================================

ZoneResourceManager::ZoneResourceManager(const std::string& zoneShortName,
	const std::string& everquest_path, const std::string& mesh_path)
	: m_zoneName(zoneShortName)
	, m_eqPath(everquest_path)
	, m_meshPath(mesh_path)
{
	m_globalResourceMgr = std::make_unique<eqg::ResourceManager>();
	m_resourceMgr = std::make_unique<eqg::ResourceManager>(m_globalResourceMgr.get());
}

ZoneResourceManager::~ZoneResourceManager()
{
}

bool ZoneResourceManager::Load()
{
	Clear();

	// Do a quick check to ensure the zone exists before we start loading everything
	fs::path zone_path = fs::path(m_eqPath) / m_zoneName;
	std::error_code ec;
	if (!fs::exists(zone_path.replace_extension(".eqg"), ec)
		&& !fs::exists(zone_path.replace_extension(".s3d"), ec))
	{
		SPDLOG_WARN("Zone files for '{}' not found", m_zoneName);
		return false;
	}

	LoadGlobalData();

	if (!LoadZone())
	{
		Clear();

		return false;
	}

	m_loaded = true;
	return true;
}

bool ZoneResourceManager::BuildCollisionMesh(ZoneCollisionMesh& collisionMesh)
{
	SPDLOG_INFO("Building collision mesh");

	collisionMesh.clear();

	// load terrain geometry
	if (terrain)
	{
		collisionMesh.addTerrain(terrain);
	}

	if (terrain_model)
	{
		collisionMesh.addZoneGeometry(terrain_model);
	}

	//for (auto& model : map_s3d_geometry)
	//{
	//	collisionMesh.addZoneGeometry(model);
	//}

	collisionMesh.addPolys(collide_verts, collide_indices);

	// Load models
	//for (auto& [name, model] : map_models)
	//{
	//	collisionMesh.addModel(name, model);
	//}

	for (auto& [name, model] : map_eqg_models)
	{
		collisionMesh.addModel(name, model);
	}

	// Add model instances
	for (const auto& obj : map_placeables)
	{
		if (!obj->ignore_for_collision)
			collisionMesh.addModelInstance(obj);
	}

	bool result = collisionMesh.finalize();

	SPDLOG_INFO("Finished building collision mesh");

	return result;
}

struct DoorParams
{
	uint32_t id;
	std::string name;
	uint16_t type;
	float scale;
	glm::mat4x4 transform;
};

bool IsSwitchStationary(DWORD type)
{
	return (type != 53 && type >= 50 && type < 59)
		|| (type >= 153 && type <= 155);
}

void ZoneResourceManager::LoadDoors()
{
	//
	// Load the door data
	//
	std::string filename = (fs::path(m_meshPath) / fmt::format("{}_doors.json", m_zoneName)).string();

	std::error_code ec;
	if (!fs::is_regular_file(filename, ec))
		return;

	std::stringstream ss;
	std::ifstream ifs;

	ifs.open(filename.c_str());
	ss << ifs.rdbuf();
	ifs.close();

	rapidjson::Document document;

	if (document.Parse<0>(ss.str().c_str()).HasParseError())
		return;
	if (!document.IsArray())
		return;

	std::vector<DoorParams> doors;
	m_hasDynamicObjects = true;

	for (auto iter = document.Begin(); iter != document.End(); ++iter)
	{
		if (!iter->IsObject())
			return;

		DoorParams params;
		params.id = (*iter)["ID"].GetInt();
		params.name = (*iter)["Name"].GetString();
		params.type = (uint16_t)(*iter)["Type"].GetInt();
		params.scale = (float)(*iter)["Scale"].GetDouble();

		// only add stationary objects
		if (!IsSwitchStationary(params.type))
			continue;

		const auto& t = (*iter)["Transform"];
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				params.transform[i][j] = (float)t[i][j].GetDouble();
			}
		}

		doors.push_back(params);
	}

	if (doors.empty())
	{
		m_doorsLoaded = true;
		return;
	}

	for (DoorParams& params : doors)
	{
		std::shared_ptr<eqg::Placeable> gen_plac = std::make_shared<eqg::Placeable>();
		glm::vec3 scale;
		glm::quat rotation;
		glm::vec3 pos;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(params.transform, scale, rotation, pos, skew, perspective);

		gen_plac->SetPosition(pos);
		gen_plac->SetRotation(glm::eulerAngles(rotation));
		gen_plac->SetScale(params.scale * scale);
		gen_plac->SetName(params.name + "_ACTORDEF");

		SPDLOG_TRACE("Adding placeable from dynamic objects {} at ({}, {}, {})", params.name, pos.x, pos.y, pos.z);

		LoadModelInst(gen_plac);
		++m_dynamicObjects;
	}
}

void ZoneResourceManager::LoadGlobalLoadFile()
{
	global_load_assets.clear();

	fs::path assets_file = fs::path(m_eqPath) / "Resources" / "GlobalLoad.txt";

	std::error_code ec;
	if (fs::exists(assets_file, ec))
	{
		SPDLOG_INFO("Loading GlobalLoad.txt");

		std::ifstream assets_stream(assets_file);
		std::vector<std::string> lines;

		if (assets_stream.is_open())
		{
			std::copy(std::istream_iterator<std::string>(assets_stream),
				std::istream_iterator<std::string>(),
				std::back_inserter(lines));
		}

		for (const std::string& line : lines)
		{
			try
			{
				std::vector<std::string> tokens = mq::split(line, ',');
				if (tokens.size() == 5)
				{
					GlobalLoadAsset asset;
					asset.phase = std::atoi(tokens[0].c_str());

					if (tokens[2].size() > 4)
					{
						asset.itemAnims = tokens[2][3] == 'T';
						asset.luclinAnims = tokens[2][4] == 'T';
					}

					asset.fileName = tokens[3];
					asset.message = tokens[4];

					global_load_assets.push_back(std::move(asset));
				}
			}
			catch (const std::exception& exc)
			{
				SPDLOG_ERROR("Failed to parse line in GlobalLoad.txt: {} - {}", line, exc.what());
			}
		}
	}
}

void ZoneResourceManager::PerformGlobalLoad(int phase)
{
	SPDLOG_INFO("Loading GlobalLoad.txt phase {}", phase);

	for (const GlobalLoadAsset& asset : global_load_assets)
	{
		if (asset.phase == phase)
		{
			if (mq::ci_ends_with(asset.fileName, ".eqg"))
			{
				LoadEQG(asset.fileName);
			}
			else
			{
				int flags = 0;

				if (asset.luclinAnims)
					flags |= eqg::LoadFlag_LuclinAnims;
				if (asset.itemAnims)
					flags |= eqg::LoadFlag_ItemAnims;

				LoadS3D(asset.fileName, flags);
			}
		}
	}
}

void ZoneResourceManager::LoadGlobalChr()
{
	// TODO: Load GlobalLoad_chr.txt
}

//============================================================================

void ZoneResourceManager::Clear()
{
	m_doorsLoaded = false;
	m_loaded = false;

	global_load_assets.clear();
	collide_verts.clear();
	collide_indices.clear();
	non_collide_verts.clear();
	non_collide_indices.clear();
	current_collide_index = 0;
	current_non_collide_index = 0;
	collide_vert_to_index.clear();
	non_collide_vert_to_index.clear();
	terrain.reset();
	//map_models.clear();
	//map_anim_models.clear();
	map_eqg_models.clear();

	map_placeables.clear();
	//map_s3d_geometry.clear();
	m_dynamicObjects = 0;
	m_hasDynamicObjects = false;

	m_wldLoaders.clear();
	m_archives.clear();
}

bool ZoneResourceManager::LoadGlobalData()
{
	// Create dummy actor definition for PLAYER_1
	auto simpleDef = m_globalResourceMgr->CreateSimpleModelDefinition();
	eqg::ActorDefinitionPtr actorDef = std::make_shared<eqg::ActorDefinition>("PLAYER_1", simpleDef);
	m_globalResourceMgr->Add(actorDef);

	m_defaultLoadFlags = eqg::LoadFlag_GlobalLoad;

	LoadGlobalLoadFile();

	PerformGlobalLoad(1);
	PerformGlobalLoad(2);

	m_defaultLoadFlags |= eqg::LoadFlag_SkipOldAnims | eqg::LoadFlag_OptimizeAnims | (m_skipSocials ? eqg::LoadFlag_SkipSocials : 0);

	// TODO: Load Global Models etc
#if 0
	bool loadedLuclinModels = false;

	if (m_loadLuclinModels)
	{
		// For each Race/Gender
		if (ShouldLoadLuclinModel(race, gender))
		{
			// %s -> Tag for the race+gender of the model

			LoadS3D("global%s_chr2", LoadFlag_LuclinAnims);
			LoadS3D("global%s_chr", LoadFlag_LuclinAnims);

			LoadS3D("VEquip"); // VahShir equipment models

			// If we loaded any luclin models, set this to true.
			loadedLuclinModels = true;
		}
	}
#endif
	m_defaultLoadFlags &= ~eqg::LoadFlag_SkipOldAnims;
#if 0
	// TOOD: Load Luclin Elementals
	if (m_loadLuclinElementals)
	{
		// Luclin elementals and horses
		LoadS3D("Global5_chr2");
		LoadS3D("Global5_chr");

		// Froglog mount animations
		LoadS3D("frog_mount_chr");
	}

	if (loadedLuclinModels)
	{
		// Equipment for luclin models
		LoadS3D("LGEquip_amr2");
		LoadS3D("LGEquip_amr");

		LoadS3D("LGEquip2");
		LoadS3D("LGEquip");
	}

	if (false) // vah shir low poly models, if the good versions are off
	{
		LoadS3D("GEquip6");
		LoadS3D("GEquip7_chr");
	}
#endif
	m_defaultLoadFlags = eqg::LoadFlag_GlobalLoad;

	PerformGlobalLoad(3);

#if 0
	if (oldModels || LoadVeliousArmorsWithLuclin)
	{
		// Velious armors
		for (int i = 17; i < 24; ++i)
		{
			LoadS3D(fmt::format("global{}_amr", i));
		}
	}
#endif

	PerformGlobalLoad(4);

	LoadGlobalChr();

	// TODO: Stick figure bones

	return true;
}

bool ZoneResourceManager::LoadZone()
{
	m_defaultLoadFlags = eqg::LoadFlag_None;

	// Load <zonename>_EnvironmentEmitters.txt

	bool loadedEQG = false;
	fs::path zonePath = fs::path(m_eqPath) / m_zoneName;

	// Check if .eqg file exists

	std::error_code ec;
	if (fs::exists(zonePath.replace_extension(".eqg"), ec))
	{
		if (LoadEQG(m_zoneName))
		{
			loadedEQG = true;
		}
	}
	else if (!fs::exists(zonePath.replace_extension(".s3d"), ec))
	{
		return false;
	}

	// If this is LDON zone, load obequip_lexit.s3d
	if (g_config.GetExpansionForZone(m_zoneName) == "Lost Dungeons of Norrath")
	{
		LoadS3D("obequip_lexit", eqg::LoadFlag_ItemAnims);
	}

	if (!loadedEQG)
	{
		// Load zone data: <zonename>.s3d
		eqg::WLDLoader* zone_archive = LoadS3D(m_zoneName);
		if (!zone_archive)
		{
			return false; // Zone doesn't exist
		}
	}

	// TODO: Load <zonename>_chr.txt

	// Load <zonename>_assets.txt
	LoadAssetsFile();


	if (!loadedEQG)
	{
		// If this is poknowledge, load poknowledge_obj3.eqg
		if (m_zoneName == "poknowledge")
		{
			LoadEQG("poknowledge_obj3");
		}

		LoadS3D(m_zoneName + "_obj2", eqg::LoadFlag_ItemAnims);
		LoadS3D(m_zoneName + "_obj", eqg::LoadFlag_ItemAnims);
		LoadS3D(m_zoneName + "_2_obj", eqg::LoadFlag_ItemAnims);
	}

	if (!loadedEQG)
	{
		if (!LoadS3D(m_zoneName))
			return false;

		// Load object placements
		LoadS3D(m_zoneName, 0, "objects.wld");
		LoadS3D(m_zoneName, 0, "lights.wld");
	}

	// Load our dynamic objects that we saved off from the plugin.
	LoadDoors();

	return true;
}

eqg::Archive* ZoneResourceManager::GetArchive(int index) const
{
	std::unique_lock lock(m_archiveMutex);

	if (index < (int)m_archives.size())
	{
		return m_archives[index].get();
	}

	return nullptr;
}

int ZoneResourceManager::GetNumArchives() const
{
	std::unique_lock lock(m_archiveMutex);

	return (int)m_archives.size();
}

eqg::Archive* ZoneResourceManager::LoadArchive(const std::string& path)
{
	std::unique_lock lock(m_archiveMutex);

	for (const auto& archive : m_archives)
	{
		if (path == archive->GetFilePath())
			return archive.get();
	}

	std::string filename = fs::path(path).filename().string();
	SPDLOG_INFO("Loading archive: {}", filename);

	auto archive = std::make_unique<eqg::Archive>();
	if (archive->Open(path))
	{
		m_archives.push_back(std::move(archive));

		return m_archives.back().get();
	}

	return nullptr;
}

eqg::EQGLoader* ZoneResourceManager::LoadEQG(std::string_view fileName)
{
	// Load EQG archive and then load all the data within it.
	fs::path archivePath = (fs::path(m_eqPath) / fileName).replace_extension(".eqg");
	std::string archiveFileName = archivePath.filename().string();

	eqg::Archive* archive = LoadArchive(archivePath.string());
	if (!archive)
	{
		return nullptr;
	}

	eqg::EQGLoader* loader = LoadEQG(archive);

	if (loader)
	{
		SPDLOG_INFO("Loaded {}", archiveFileName);
	}
	else
	{
		SPDLOG_ERROR("Failed to load {}", archiveFileName);
	}

	return loader;
}

eqg::EQGLoader* ZoneResourceManager::LoadEQG(eqg::Archive* archive)
{
	for (const auto& loader : m_eqgLoaders)
	{
		if (archive == loader->GetArchive())
			return loader.get();
	}

	auto loader = std::make_unique<eqg::EQGLoader>();
	if (!loader->Load(archive))
		return nullptr;

	//============================================================================================================
	// Load all the data that we build from the archive.

	// Import models
	for (const auto& model : loader->models)
	{
		map_eqg_models[model->tag] = model;
	}

	// Import the terrain (only once)
	if (terrain)
	{
		if (loader->terrain)
		{
			SPDLOG_WARN("Already have a terrain loaded when loading {}", archive->GetFileName());
		}
	}
	else
	{
		terrain = loader->terrain;

		if (terrain)
		{
			// Water sheets
			auto& water_sheets = terrain->GetWaterSheets();
			for (size_t i = 0; i < water_sheets.size(); ++i)
			{
				auto& sheet = water_sheets[i];

				//if (sheet->GetTile())
				//{
				//	for (const auto& tile : terrain->tiles)
				//	{
				//		float x = tile->tile_pos.x;
				//		float y = tile->tile_pos.y;
				//		float z = tile->base_water_level;

				//		float QuadVertex1X = x;
				//		float QuadVertex1Y = y;
				//		float QuadVertex1Z = z;

				//		float QuadVertex2X = QuadVertex1X + (terrain->GetParams().quads_per_tile * terrain->GetParams().units_per_vert);
				//		float QuadVertex2Y = QuadVertex1Y;
				//		float QuadVertex2Z = QuadVertex1Z;

				//		float QuadVertex3X = QuadVertex2X;
				//		float QuadVertex3Y = QuadVertex1Y + (terrain->GetParams().quads_per_tile * terrain->GetParams().units_per_vert);
				//		float QuadVertex3Z = QuadVertex1Z;

				//		float QuadVertex4X = QuadVertex1X;
				//		float QuadVertex4Y = QuadVertex3Y;
				//		float QuadVertex4Z = QuadVertex1Z;

				//		uint32_t current_vert = (uint32_t)non_collide_verts.size() + 3;
				//		non_collide_verts.push_back(glm::vec3(QuadVertex1X, QuadVertex1Y, QuadVertex1Z));
				//		non_collide_verts.push_back(glm::vec3(QuadVertex2X, QuadVertex2Y, QuadVertex2Z));
				//		non_collide_verts.push_back(glm::vec3(QuadVertex3X, QuadVertex3Y, QuadVertex3Z));
				//		non_collide_verts.push_back(glm::vec3(QuadVertex4X, QuadVertex4Y, QuadVertex4Z));

				//		non_collide_indices.push_back(current_vert);
				//		non_collide_indices.push_back(current_vert - 2);
				//		non_collide_indices.push_back(current_vert - 1);

				//		non_collide_indices.push_back(current_vert);
				//		non_collide_indices.push_back(current_vert - 3);
				//		non_collide_indices.push_back(current_vert - 2);
				//	}
				//}
				//else
				//{
				//	uint32_t id = (uint32_t)non_collide_verts.size();

				//	non_collide_verts.push_back(glm::vec3(sheet->GetMinY(), sheet->GetMinX(), sheet->GetZHeight()));
				//	non_collide_verts.push_back(glm::vec3(sheet->GetMinY(), sheet->GetMaxX(), sheet->GetZHeight()));
				//	non_collide_verts.push_back(glm::vec3(sheet->GetMaxY(), sheet->GetMinX(), sheet->GetZHeight()));
				//	non_collide_verts.push_back(glm::vec3(sheet->GetMaxY(), sheet->GetMaxX(), sheet->GetZHeight()));

				//	non_collide_indices.push_back(id);
				//	non_collide_indices.push_back(id + 1);
				//	non_collide_indices.push_back(id + 2);

				//	non_collide_indices.push_back(id + 1);
				//	non_collide_indices.push_back(id + 3);
				//	non_collide_indices.push_back(id + 2);
				//}
			}

			// Invisible wals
			auto& invis_walls = terrain->GetInvisWalls();
			for (auto& wall : invis_walls)
			{
				auto& verts = wall->GetVertices();
				float wallHeight = wall->GetWallHeight();

				for (int j = 0; j < (int)verts.size() - 1; ++j)
				{
					glm::vec3 v1 = verts[j].yxz;
					glm::vec3 v2 = verts[j + 1].yxz;

					glm::vec3 v3 = v1;
					v3.z += wallHeight;

					glm::vec3 v4 = v2;
					v4.z += wallHeight;

					AddFace(v2, v1, v3, true);
					AddFace(v3, v4, v2, true);

					AddFace(v3, v1, v2, true);
					AddFace(v2, v4, v3, true);
				}
			}

			for (const auto& object : terrain->objects)
			{
				//map_placeables.push_back(object);
				LoadModelInst(object);
			}
		}
	}

	if (terrain_model)
	{
		if (loader->terrain_model)
		{
			SPDLOG_WARN("Already have a TER model loaded when loading {}", archive->GetFileName());
		}
	}
	else
	{
		terrain_model = loader->terrain_model;
	}

	for (const auto& plac : loader->placeables)
	{
		// TODO: come up with external list of models to ignore.
		if (plac->tag == "OBJ_BLOCKERA_ACTORDEF")
			plac->ignore_for_collision = true;

		//eqLogMessage(LogTrace, "Adding placeable %s at (%.2f, %.2f, %.2f)",
		//	plac->tag.c_str(), plac->pos.x, plac->pos.y, plac->pos.z)
		//map_placeables.push_back(plac);

		LoadModelInst(plac);
	}

	for (const auto& area : loader->areas)
	{
		map_areas.push_back(area);
	}

	for (const auto& lights : loader->lights)
	{
		map_lights.push_back(lights);
	}

	for (const auto& lod_list : loader->lod_lists)
	{
		lod_lists.push_back(lod_list);
	}

	m_eqgLoaders.push_back(std::move(loader));
	return m_eqgLoaders.back().get();
}

eqg::WLDLoader* ZoneResourceManager::LoadS3D(std::string_view fileName, int loadFlags, std::string_view wldFile)
{
	// Combo load S3D archive and .WLD file by same name
	std::string archivePath = (fs::path(m_eqPath) / fileName).replace_extension(".s3d").string();
	std::string wldFileName = wldFile.empty() ? fs::path(fileName).filename().replace_extension(".wld").string() : std::string(wldFile);

	eqg::Archive* archive = LoadArchive(archivePath);
	if (!archive)
	{
		return nullptr;
	}

	return LoadWLD(archive, wldFileName, m_defaultLoadFlags | loadFlags);
}

eqg::WLDLoader* ZoneResourceManager::LoadWLD(eqg::Archive* archive, const std::string& fileName, int loadFlags)
{
	for (const auto& loader : m_wldLoaders)
	{
		if (fileName == loader->GetFileName())
			return loader.get();
	}

	bool globalLoad = (loadFlags & eqg::LoadFlag_GlobalLoad) != 0;

	auto loader = std::make_unique<eqg::WLDLoader>(globalLoad ? m_globalResourceMgr.get() : m_resourceMgr.get());
	if (!loader->Init(archive, fileName, loadFlags))
		return nullptr;

	if (!loader->ParseAll())
		return nullptr;

	// Load objects from the .wld
	auto& objectList = loader->GetObjectList();
	for (auto& obj : objectList)
	{
		//if (obj.type == eqg::WLD_OBJ_ZONE_TYPE)
		//{
		//	eqg::WLDFragment29* frag = static_cast<eqg::WLDFragment29*>(obj.parsed_data);
		//	auto region = frag->region;

		//	SPDLOG_TRACE("Processing zone region '{}' '{}' for s3d.", region->tag, region->old_style_tag);
		//}
		//else if (obj.type == eqg::WLD_OBJ_REGION_TYPE)
		//{
		//	// Regions with DMSPRITEDEF2 make up the geometry of the zone
		//	eqg::WLDFragment22* region_frag = static_cast<eqg::WLDFragment22*>(obj.parsed_data);

		//	if (region_frag->region_sprite_index != -1 && region_frag->region_sprite_is_def)
		//	{
		//		// Get the geometry from this sprite index
		//		auto& sprite_obj = loader->GetObject(region_frag->region_sprite_index);
		//		if (sprite_obj.type == eqg::WLD_OBJ_DMSPRITEDEFINITION2_TYPE)
		//		{
		//			//auto model = static_cast<eqg::WLDFragment36*>(sprite_obj.parsed_data)->geometry;

		//			//map_s3d_geometry.push_back(model);
		//		}
		//	}
		//}
		//else if (obj.type == eqg::WLD_OBJ_ACTORINSTANCE_TYPE)
		//{
		//	eqg::WLDFragment15* frag = static_cast<eqg::WLDFragment15*>(obj.parsed_data);
		//	if (!frag->placeable)
		//		continue;

		//	//map_s3d_model_instances.push_back(frag->placeable);
		//	LoadModelInst(frag->placeable);
		//}
		//else if (obj.type == eqg::WLD_OBJ_ACTORDEFINITION_TYPE)
		//{
		//	std::string_view tag = obj.tag;
		//	if (tag.empty())
		//		continue;

		//	eqg::WLDFragment14* obj_frag = static_cast<eqg::WLDFragment14*>(obj.parsed_data);
		//	int sprite_id = obj_frag->sprite_id;

		//	auto& sprite_obj = loader->GetObjectFromID(sprite_id);

		//	if (sprite_obj.type == eqg::WLD_OBJ_DMSPRITEINSTANCE_TYPE)
		//	{
		//		eqg::WLDFragment2D* r_frag = static_cast<eqg::WLDFragment2D*>(sprite_obj.parsed_data);
		//		auto m_ref = r_frag->sprite_id;

		//		//eqg::WLDFragment36* mod_frag = static_cast<eqg::WLDFragment36*>(loader->GetObjectFromID(m_ref).parsed_data);
		//		//auto mod = mod_frag->geometry;

		//		//map_models.emplace(tag, mod);
		//	}
		//	else if (sprite_obj.type == eqg::WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE)
		//	{
		//		eqg::WLDFragment11* r_frag = static_cast<eqg::WLDFragment11*>(sprite_obj.parsed_data);
		//		auto s_ref = r_frag->def_id;

		//		eqg::WLDFragment10* skeleton_frag = static_cast<eqg::WLDFragment10*>(loader->GetObjectFromID(s_ref).parsed_data);
		//		auto skele = skeleton_frag->track;

		//		map_anim_models.emplace(tag, skele);
		//	}
		//}
	}
	
	m_wldLoaders.push_back(std::move(loader));
	SPDLOG_TRACE("Loaded {} from {}", fileName, archive->GetFileName());

	return m_wldLoaders.back().get();
}

void ZoneResourceManager::LoadAssetsFile()
{
	// to try to read an _assets file and load more data for the zone.
	std::vector<std::string> filenames;

	std::error_code ec;
	fs::path assets_file = fs::path(m_eqPath) / fmt::format("{}_assets.txt", m_zoneName);
	if (fs::exists(assets_file, ec))
	{
		SPDLOG_INFO("Loading {}_assets.txt", m_zoneName);

		std::ifstream assets(assets_file);

		if (assets.is_open())
		{
			std::copy(std::istream_iterator<std::string>(assets),
				std::istream_iterator<std::string>(),
				std::back_inserter(filenames));
		}
	}

	for (const std::string& filename : filenames)
	{
		if (mq::ci_ends_with(filename, ".eqg"))
		{
			LoadEQG(filename);
		}
		else
		{
			LoadS3D(filename);
		}
	}
}

//void ZoneResourceManager::TraverseBone(
//	std::shared_ptr<eqg::s3d::SkeletonTrack::Bone> bone,
//	glm::vec3 parent_trans,
//	glm::vec3 parent_rot,
//	glm::vec3 parent_scale)
//{
//	glm::vec3 pos = glm::vec3(0.0f, 0.0f, 0.0f);
//	glm::vec3 rot = glm::vec3(0.0f, 0.0f, 0.0f);
//	glm::vec3 scale = parent_scale;
//
//	if (!bone->transforms.empty())
//	{
//		auto& transform = bone->transforms[0];
//
//		if (transform.scale != 0)
//		{
//			pos = transform.pivot * transform.scale;
//		}
//
//		if (transform.rotation.w != 0)
//		{
//			rot.x = transform.rotation.x / transform.rotation.w;
//			rot.y = transform.rotation.y / transform.rotation.w;
//			rot.z = transform.rotation.z / transform.rotation.w;
//
//			rot = glm::radians(rot);
//		}
//	}
//
//	RotateVertex(pos, parent_rot.x, parent_rot.y, parent_rot.z);
//	pos += parent_trans;
//	rot += parent_rot;
//
//	if (bone->model)
//	{
//		if (!map_models.contains(bone->model->GetName()))
//		{
//			map_models[bone->model->GetName()] = bone->model;
//		}
//		
//		std::shared_ptr<eqg::Placeable> gen_plac = std::make_shared<eqg::Placeable>();
//		gen_plac->SetName(bone->model->GetName());
//		gen_plac->SetPosition(pos);
//		gen_plac->SetRotation(rot);
//		gen_plac->SetScale(scale);
//		map_placeables.push_back(gen_plac);
//
//		SPDLOG_TRACE("Adding placeable from bones {} at ({}, {}, {})", bone->model->GetName(), pos.x, pos.y, pos.z);
//	}
//
//	for (size_t i = 0; i < bone->children.size(); ++i)
//	{
//		TraverseBone(bone->children[i], pos, rot, parent_scale);
//	}
//}

bool ZoneResourceManager::LoadModelInst(const PlaceablePtr& inst)
{
	if (auto map_eqg_models_iter = map_eqg_models.find(inst->tag); map_eqg_models_iter != map_eqg_models.end())
	{
		map_placeables.push_back(inst);

		SPDLOG_TRACE("Adding model instance '{}' at ({}, {}, {})", inst->tag, inst->pos.x, inst->pos.y, inst->pos.z);
		return true;
	}

	//if (auto map_models_iter = map_models.find(inst->tag); map_models_iter != map_models.end())
	//{
	//	map_placeables.push_back(inst);

	//	SPDLOG_TRACE("Adding model instance '{}' at ({}, {}, {})", inst->tag, inst->pos.x, inst->pos.y, inst->pos.z);
	//	return true;
	//}

	//if (auto map_anim_models_iter = map_anim_models.find(inst->tag); map_anim_models_iter != map_anim_models.end())
	//{
	//	const auto& skel = map_anim_models_iter->second;

	//	auto& bones = skel->GetBones();

	//	if (!bones.empty())
	//	{
	//		TraverseBone(bones[0], inst->GetPosition(), inst->GetRotation(), inst->GetScale());
	//	}

	//	SPDLOG_TRACE("Adding hierarchical model instance '{}' at ({}, {}, {})", inst->tag, inst->pos.x, inst->pos.y, inst->pos.z);
	//	return true;
	//}
	
	SPDLOG_WARN("Could not find model for '{}'", inst->tag);
	return false;
}

void ZoneResourceManager::AddFace(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, bool collidable)
{
	if (!collidable)
	{
		auto InsertVertex = [this](const glm::vec3& vec)
		{
			auto iter = non_collide_vert_to_index.find(vec);
			if (iter == non_collide_vert_to_index.end())
			{
				non_collide_vert_to_index[vec] = current_non_collide_index;
				non_collide_verts.push_back(vec);
				non_collide_indices.push_back(current_non_collide_index);

				++current_non_collide_index;
			}
			else
			{
				uint32_t t_idx = iter->second;
				non_collide_indices.push_back(t_idx);
			}
		};

		InsertVertex(v1);
		InsertVertex(v2);
		InsertVertex(v3);
	}
	else
	{
		auto InsertVertex = [this](const glm::vec3& vec)
		{
			auto iter = collide_vert_to_index.find(vec);
			if (iter == collide_vert_to_index.end())
			{
				collide_vert_to_index[vec] = current_collide_index;
				collide_verts.push_back(vec);
				collide_indices.push_back(current_collide_index);

				++current_collide_index;
			}
			else
			{
				uint32_t t_idx = iter->second;
				collide_indices.push_back(t_idx);
			}
		};

		InsertVertex(v1);
		InsertVertex(v2);
		InsertVertex(v3);
	}
}
