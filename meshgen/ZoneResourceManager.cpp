//
// MapGeometryLoader.cpp
//

#include "meshgen/ZoneResourceManager.h"
#include "meshgen/ApplicationConfig.h"
#include "meshgen/EQComponents.h"
#include "meshgen/GeometryUtils.h"
#include "meshgen/MGBitmap.h"
#include "meshgen/MGSimpleModel.h"
#include "meshgen/MGTerrain.h"
#include "meshgen/MGTerrainTile.h"
#include "meshgen/Scene.h"
#include "meshgen/ZoneCollisionMesh.h"
#include "common/NavMeshData.h"
#include "mq/base/String.h"

#include "eqglib/eqg_global_data.h"
#include "eqglib/eqg_terrain_loader.h"
#include "entt/entity/handle.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "rapidjson/document.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <ranges>
#include <sstream>

#define SKIP_GLOBAL_LOAD 1

namespace fs = std::filesystem;

//============================================================================================================

// This interface receives the objects created when loading a zone and turns them into entities that
// we will place into our scene.

class MGResourceManager : public eqg::ResourceManager
{
public:
	MGResourceManager(const std::string& data_path, eqg::ResourceManager* parent, ZoneResourceManager* zoneResourceMgr, bool isGlobal)
		: eqg::ResourceManager(data_path, parent)
		, m_zoneResourceMgr(zoneResourceMgr)
		, m_isGlobal(isGlobal)
	{
	}

	// Create our custom bitmap type that supports bgfx textures
	virtual std::shared_ptr<eqg::Bitmap> CreateBitmap() const override
	{
		return std::make_shared<MGBitmap>();
	}

	// Create our custom SimpleModel type that supports bgfx buffers
	virtual std::shared_ptr<eqg::SimpleModel> CreateSimpleModel() const override
	{
		return std::make_shared<MGSimpleModel>();
	}

	// Create our custom Terrain type that supports bgfx buffers
	virtual eqg::TerrainPtr CreateTerrain() const override
	{
		return std::make_shared<MGTerrain>();
	}

private:
	ZoneResourceManager* m_zoneResourceMgr = nullptr;
	bool m_isGlobal = false;
};


//============================================================================================================

ZoneResourceManager::ZoneResourceManager(const std::string& zoneShortName,
	const std::string& everquest_path, const std::string& mesh_path, const std::shared_ptr<Scene>& scene)
	: m_zoneName(zoneShortName)
	, m_eqPath(everquest_path)
	, m_meshPath(mesh_path)
	, m_scene(scene)
{
	m_globalResourceMgr = std::make_unique<MGResourceManager>(m_eqPath, nullptr, this, true);
	m_resourceMgr = std::make_unique<MGResourceManager>(m_eqPath, m_globalResourceMgr.get(), this, false);

	eqg::g_dataManager.Init(m_eqPath);
}

ZoneResourceManager::~ZoneResourceManager()
{
	m_actors.clear();
	m_lights.clear();
	m_areas.clear();
	m_scene->Clear();

	global_load_assets.clear();

	collide_verts.clear();
	collide_indices.clear();
	non_collide_verts.clear();
	non_collide_indices.clear();
	collide_vert_to_index.clear();
	non_collide_vert_to_index.clear();

	m_eqgLoaders.clear();
	m_wldLoaders.clear();
	m_archives.clear();

	m_resourceMgr.reset();
	m_globalResourceMgr.reset();
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

bool ZoneResourceManager::BuildScene(Scene& scene)
{
	auto& registry = m_scene->GetRegistry();

#if 0 // cobalt scar druid ring - just testing. should work with switches being loaded.
	auto pActorDef = m_resourceMgr->Get<eqg::ActorDefinition>("DRAGCIRCLE200_ACTORDEF");
	if (pActorDef)
	{
		glm::mat4 mat{
			0.19509030878543854, 0.9807851910591125, 0.0, 0.0,
			-0.9807851910591125, 0.19509030878543854, 0.0, 0.0,
			0.0, 0.0, 0.9999999403953552, 0.0,
			-1062.3533935546875, -1621.2421875, 293.53326416015625,1.0
		};

		glm::vec3 scale, translation;
		glm::quat orientation;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(mat, scale, orientation, translation, skew, perspective);

		auto actor = m_resourceMgr->CreateSimpleActor("DRAGCIRCLE200", pActorDef, translation.yzx, glm::eulerAngles(orientation).yzx, scale.x, eqg::eCollisionVolumeNone);
		AddActor(actor);
	}
#endif

	for (const eqg::ActorPtr& actor : m_resourceMgr->GetActors())
	{
		AddActor(actor);
	}

	for (const eqg::PointLightPtr& pointLight : m_resourceMgr->GetLights())
	{
		AddPointLight(pointLight);
	}

	const auto& terrain = m_resourceMgr->GetTerrain();

	if (terrain)
	{
		// Create terrain render entity (WLD zones)
		MGTerrain* mgTerrain = static_cast<MGTerrain*>(terrain.get());

		entt::handle terrainEntity = m_scene->CreateEntity("Terrain");
		terrainEntity.emplace<TerrainRenderComponent>(mgTerrain);

		// EQG zones use TerrainArea with bounding boxes
		for (const eqg::TerrainAreaPtr& terrainArea : terrain->m_areas)
		{
			AddArea(terrainArea);
		}

		// WLD zones use BSP trees for area bounds
		if (terrain->m_wldBspTree && !terrain->m_wldAreas.empty())
		{
			CreateWldAreaEntities(*terrain);
		}
	}

	// Create terrain tile entities (EQG zones with TerrainSystem)
	if (auto terrainSystem = m_resourceMgr->GetTerrainSystem())
	{
		m_terrainTiles.clear();
		for (const auto& tile : terrainSystem->GetTiles())
		{
			auto mgTile = std::make_shared<MGTerrainTile>(terrainSystem.get(), tile.get());
			m_terrainTiles.push_back(mgTile);

			std::string tileName = fmt::format("Tile ({}, {})", tile->m_tileLoc.x, tile->m_tileLoc.y);
			entt::handle tileEntity = m_scene->CreateEntity(tileName);
			tileEntity.emplace<TerrainTileRenderComponent>(mgTile.get());
		}
		SPDLOG_DEBUG("Created {} terrain tile entities", m_terrainTiles.size());

		// Create invisible wall entities
		for (const auto& wall : terrainSystem->GetInvisWalls())
		{
			entt::handle wallEntity = m_scene->CreateEntity(wall->GetName());

			auto& wallComp = wallEntity.emplace<InvisibleWallComponent>();
			wallComp.vertices = wall->GetVertices();
			wallComp.wallHeight = wall->GetWallHeight();

			wallEntity.emplace<InvisibleWallRenderComponent>();
		}
	}

	registry.sort<IdentityComponent>([](const IdentityComponent& lhs, const IdentityComponent& rhs) {
		return mq::ci_string_compare(lhs.name, rhs.name) < 0;
	});
	return true;
}

bool ZoneResourceManager::BuildCollisionMesh(ZoneCollisionMesh& collisionMesh)
{
	SPDLOG_INFO("Building collision mesh");

	collisionMesh.clear();

	// load terrain geometry
	if (auto terrainSystem = m_resourceMgr->GetTerrainSystem())
	{
		// Invisible walls
		auto& invis_walls = terrainSystem->GetInvisWalls();
		for (auto& wall : invis_walls)
		{
			auto& verts = wall->GetVertices();
			float wallHeight = wall->GetWallHeight();

			for (int j = 0; j < (int)verts.size() - 1; ++j)
			{
				glm::vec3 v1 = verts[j];
				glm::vec3 v2 = verts[j + 1];

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

		collisionMesh.addTerrainSystem(terrainSystem);
	}

	if (auto terrain = m_resourceMgr->GetTerrain())
	{
		collisionMesh.addTerrain(terrain);
	}

	collisionMesh.addPolys(collide_verts, collide_indices);

	// Load models

	auto view = m_scene->GetAllEntitiesWith<const ActorComponent, const CollisionComponent>();
	for (auto [entity, actor, collision] : view.each())
	{
		entt::handle handle{ m_scene->GetRegistry(), entity };

		collisionMesh.addActor(handle, actor.actor.get());
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

#if 0
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
#endif
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
#if !SKIP_GLOBAL_LOAD
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
#endif
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
	// TODO: Load Luclin Elementals
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

std::vector<std::pair<std::string, std::string>> ZoneResourceManager::LoadChrFile(const std::string& inputFile)
{
	std::vector<std::pair<std::string, std::string>> assets;

	fs::path assets_file = fs::path(m_eqPath) / inputFile;

	std::error_code ec;
	if (fs::exists(assets_file, ec))
	{
		SPDLOG_DEBUG("Loading Chr file: {}", inputFile);

		std::ifstream assets_stream(assets_file);
		std::vector<std::string> lines;

		if (assets_stream.is_open())
		{
			std::copy(std::istream_iterator<std::string>(assets_stream),
				std::istream_iterator<std::string>(),
				std::back_inserter(lines));
		}

		int numEntries = 0;
		int entryCount = 0;

		bool error = false;

		for (size_t lineNum = 0; lineNum < lines.size(); ++lineNum) // Skip first line
		{
			const std::string& line = lines[lineNum];

			try
			{
				if (lineNum == 0)
				{
					numEntries = atoi(line.c_str());

					if (numEntries < 0 || numEntries >= 1000)
					{
						SPDLOG_ERROR("Bad file: {} - {} entries", inputFile, numEntries);
						break;
					}

					continue;
				}

				if (line.empty())
				{
					break;
				}

				std::vector<std::string> tokens = mq::split(line, ',');

				if (tokens.size() == 2)
				{
					if (tokens[0].empty())
					{
						numEntries--;
						SPDLOG_WARN("Bad file: {} - invalid line: {}", inputFile, line);
						continue;
					}

					if (entryCount < numEntries)
					{
						assets.emplace_back(tokens[0], tokens[1]);
						entryCount++;
					}
					else
					{
						SPDLOG_WARN("Bad file: {} - too many entries: {}", inputFile, entryCount);
						break;
					}
				}
				else
				{
					SPDLOG_WARN("Bad file: {} - invalid line: {}", inputFile, line);
				}
			}
			catch (const std::exception& exc)
			{
				SPDLOG_ERROR("Failed to parse line in {}: {} - {}", inputFile, line, exc.what());
			}
		}
	}

	return assets;
}

bool ZoneResourceManager::LoadZone(bool loadNPCModels)
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
		// If this is poknowledge, load extra poknowledge_obj3.eqg
		if (m_zoneName == "poknowledge")
		{
			LoadEQG("poknowledge_obj3");
		}


		LoadS3D(m_zoneName + "_obj2", eqg::LoadFlag_ItemAnims);
		LoadS3D(m_zoneName + "_obj", eqg::LoadFlag_ItemAnims);
		LoadS3D(m_zoneName + "_2_obj", eqg::LoadFlag_ItemAnims);

#if 0
		// Load extra s3d obj files
		std::string file = fmt::format("{}_obj2.s3d", m_zoneName);
		if (fs::is_regular_file(file, ec))
			LoadS3D(file, eqg::LoadFlag_ItemAnims);

		file = fmt::format("{}_obj.s3d", m_zoneName);
		if (fs::is_regular_file(file, ec))
			LoadS3D(file, eqg::LoadFlag_ItemAnims);

		file = fmt::format("{}_2_obj.s3d", m_zoneName);
		if (fs::is_regular_file(file, ec))
			LoadS3D(file, eqg::LoadFlag_ItemAnims);
#endif
	}

#if 0
	if (loadNPCModels)
	{
		// Load {zone}_chr.txt
		auto zoneChrData = LoadChrFile(fmt::format("{}_chr.txt", m_zoneName));
		for (auto& [npcCode, fileName] : zoneChrData)
		{
			if (!npcCode.empty() && !fileName.empty())
			{
				// TODO: These are normally loaded to the persistent memory pool

				if (std::string_view{ npcCode }.substr(0, 3) == std::string_view{ fileName }.substr(0, 3))
				{
					if (!LoadEQG(fmt::format("{}.EQG", fileName)))
					{
						LoadS3D(fileName);
					}
				}
				else
				{
					LoadS3D(fileName);
				}
			}
		}
	}
#endif

	// Load <zonename>_assets.txt
	//LoadAssetsFile();


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

eqg::EQGLoader* ZoneResourceManager::LoadEQG(std::string_view fileName, int loadFlags)
{
	// Load EQG archive and then load all the data within it.
	fs::path archivePath = (fs::path(m_eqPath) / fileName).replace_extension(".eqg");
	std::string archiveFileName = archivePath.filename().string();

	eqg::Archive* archive = LoadArchive(archivePath.string());
	if (!archive)
	{
		return nullptr;
	}

	eqg::EQGLoader* loader = LoadEQG(archive, loadFlags);

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

eqg::EQGLoader* ZoneResourceManager::LoadEQG(eqg::Archive* archive, int loadFlags)
{
	for (const auto& loader : m_eqgLoaders)
	{
		if (archive == loader->GetArchive())
			return loader.get();
	}

	bool globalLoad = (loadFlags & eqg::LoadFlag_GlobalLoad) != 0;
	auto loader = std::make_unique<eqg::EQGLoader>(globalLoad ? m_globalResourceMgr.get() : m_resourceMgr.get());
	if (!loader->Load(archive, loadFlags))
		return nullptr;

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

void ZoneResourceManager::AddActor(const eqg::ActorPtr& actor)
{
	entt::handle entity = m_scene->CreateEntity(actor->GetActorName());

	auto& transform = entity.get<TransformComponent>();
	transform.position = actor->GetPosition();
	transform.rotationEuler = actor->GetOrientation();
	transform.scale = glm::vec3{ actor->GetScale() };

	[[maybe_unused]]
	auto& actorComponent = entity.emplace<ActorComponent>(actor);

	if (!actor->GetTag().empty())
	{
		[[maybe_unused]]
		auto& tagComponent = entity.emplace<TagComponent>(actor->GetTag());
	}

	eqg::SimpleModelPtr collisionModel = actor->GetCollisionModel();

	// Note: This is simply the model used for collision. it may not actually have any collision facets
	if (actor->IsCollidable())
	{
		auto& collisionComponent = entity.emplace<CollisionComponent>();

		collisionComponent.collisionModel = actor->GetCollisionModel();
		collisionComponent.boundingRadius = actor->GetBoundingRadius();
	}

	// Add render component if actor has a renderable SimpleModel
	if (eqg::SimpleModelPtr simpleModel = actor->GetSimpleModel())
	{
		MGSimpleModel* mgModel = static_cast<MGSimpleModel*>(simpleModel.get());
		
		entity.emplace<StaticMeshRenderComponent>(mgModel);
	}

	// some objects have a really wild position, just disable them.
	if (IsPositionOutOfBounds(transform.position))
	{
		entity.emplace<HiddenComponent>();
	}

	m_actors.emplace(actor.get(), entity);
}

void ZoneResourceManager::RemoveActor(const eqg::ActorPtr& actor)
{
	auto iter = m_actors.find(actor.get());

	if (iter == m_actors.end())
		return;

	m_scene->DestroyEntity(iter->second);

	m_actors.erase(iter);
}

void ZoneResourceManager::AddPointLight(const eqg::PointLightPtr& light)
{
	entt::handle entity = m_scene->CreateEntity(light->GetDefinition()->GetTag());

	TransformComponent& transform = entity.get<TransformComponent>();
	transform.position = light->GetPosition().yzx;

	PointLightComponent& lightComponent = entity.emplace<PointLightComponent>();
	lightComponent.definition = light->GetDefinition();
	lightComponent.dynamic = light->IsDynamic();
	lightComponent.radius = light->GetRadius();

	m_lights.emplace(light.get(), entity);
}

void ZoneResourceManager::RemovePointLight(const eqg::PointLightPtr& light)
{
	auto iter = m_lights.find(light.get());

	if (iter == m_lights.end())
		return;

	m_scene->DestroyEntity(iter->second);

	m_lights.erase(iter);
}

mq::MQColor AreaEnvironmentToColor(const eqg::AreaEnvironment& env)
{
	mq::MQColor color;

	// Set color based on environment type
	switch (env.type)
	{
	case eqg::AreaEnvironment::UnderWater:
		color = mq::MQColor(0, 0, 255); // blue
		break;
	case eqg::AreaEnvironment::UnderWater2:
		color = mq::MQColor(64, 64, 255); // blue
		break;
	case eqg::AreaEnvironment::UnderWater3:
		color = mq::MQColor(128, 128, 255); // blue
		break;
	case eqg::AreaEnvironment::UnderLava:
		color = mq::MQColor(255, 0, 0); // red
		break;
	case eqg::AreaEnvironment::UnderIceWater:
		color = mq::MQColor(128, 0, 128); // purple
		break;
	case eqg::AreaEnvironment::UnderSlime:
		color = mq::MQColor(75, 255, 75); // green
		break;
	case eqg::AreaEnvironment::Fog:
		color = mq::MQColor(198, 147, 10); // brownish
		break;
	case eqg::AreaEnvironment::Portal:
		color = mq::MQColor(255, 0, 255); // magenta
		break;
	default:
		color = mq::MQColor(255, 255, 255); // white
	}

	// Override color for special flags
	if (env.flags & eqg::AreaEnvironment::Kill)
	{
		color = mq::MQColor(255, 165, 0); // orange
	}
	else if (env.flags & eqg::AreaEnvironment::Slippery)
	{
		color = mq::MQColor(0, 240, 240); // cyan
	}
	else if (env.flags & eqg::AreaEnvironment::Teleport
		|| env.flags & eqg::AreaEnvironment::TeleportIndex)
	{
		color = mq::MQColor(255, 255, 0); // yellow
	}

	return color;
}

void ZoneResourceManager::AddArea(const eqg::TerrainAreaPtr& areaPtr)
{
	entt::handle entity = m_scene->CreateEntity(areaPtr->name);

	TransformComponent& transform = entity.get<TransformComponent>();
	transform.position = areaPtr->position;
	transform.rotation = areaPtr->orientation;
	transform.scale = areaPtr->scale;

	AreaComponent& areaComponent = entity.emplace<AreaComponent>();
	areaComponent.area = areaPtr;
	areaComponent.color = AreaEnvironmentToColor(areaPtr->environment);

	RenderBoundingBoxComponent& renderComponent = entity.emplace<RenderBoundingBoxComponent>();
	renderComponent.color = AreaEnvironmentToColor(areaPtr->environment);
}

void ZoneResourceManager::CreateWldAreaEntities(const eqg::Terrain& terrain) const
{
	if (!terrain.m_wldBspTree)
	{
		return;
	}

	SPDLOG_INFO("Generating area volumes from BSP Tree");

	auto hulls = BuildConvexHullsFromRegions(terrain);
	auto breps = BuildBRepsFromConvexHulls(hulls, terrain);

	std::unordered_map<uint32_t, AreaVolumeComponent*> areaVolumeComponents;
	std::unordered_map<uint32_t, entt::handle> handles;

	auto& areas = terrain.m_wldAreas;
	auto& environments = terrain.m_wldAreaEnvironmentsPerArea;

	for (auto& brep : breps)
	{
		if (!brep.vertexes.empty() && !brep.faces.empty()) // TODO: are we okay with no edges?
		{
			uint32_t areaIndex = brep.areaIndex;
			AreaVolumeComponent* areaVolumeComponent;

			auto iter = areaVolumeComponents.find(areaIndex);
			if (iter == areaVolumeComponents.end())
			{
				const eqg::SArea* area = &areas[areaIndex];
				entt::handle entity = m_scene->CreateEntity(area->tag);
				handles.emplace(areaIndex, entity);

				WldAreaComponent* wldComponent = &entity.emplace<WldAreaComponent>();
				wldComponent->environment = environments[areaIndex];
				wldComponent->area = area;
				if (wldComponent->environment.hasTeleportEntry)
					wldComponent->teleport = terrain.m_teleports[wldComponent->environment.teleportIndex];
				wldComponent->color = AreaEnvironmentToColor(wldComponent->environment);
				wldComponent->areaIndex = areaIndex;

				areaVolumeComponent = &entity.emplace<AreaVolumeComponent>();

				areaVolumeComponents.emplace(areaIndex, areaVolumeComponent);
			}
			else
			{
				areaVolumeComponent = iter->second;
			}

			auto baseIndex = static_cast<uint16_t>(areaVolumeComponent->vertices.size());

			// Combine vertex, face, and (potentially) edge indexes into AreaVolumeComponent
			areaVolumeComponent->vertices.reserve(areaVolumeComponent->vertices.size() + brep.vertexes.size());
			areaVolumeComponent->vertices.insert(areaVolumeComponent->vertices.end(), brep.vertexes.begin(), brep.vertexes.end());

			areaVolumeComponent->faces.reserve(areaVolumeComponent->faces.size() + brep.faces.size());
			for (const auto& face : brep.faces)
			{
				std::array<uint16_t, 3> adjustedFace{};
				adjustedFace[0] = face[0] +  baseIndex;
				adjustedFace[1] = face[1] + baseIndex;
				adjustedFace[2] = face[2] + baseIndex;
				areaVolumeComponent->faces.push_back(adjustedFace);
			}

			areaVolumeComponent->outerEdges.reserve(areaVolumeComponent->outerEdges.size() + brep.outerEdges.size());
			for (const auto& outerEdge : brep.outerEdges)
			{
				std::array<uint16_t, 2> adjustedEdge{};
				adjustedEdge[0] = outerEdge[0] + baseIndex;
				adjustedEdge[1] = outerEdge[1] + baseIndex;
				areaVolumeComponent->outerEdges.push_back(adjustedEdge);
			}
		}
	}

	// Now that we have all the convex hulls consolidated, process each area
	for (const auto& [areaIndex, areaVolume] : areaVolumeComponents)
	{
		// Create a transform to give this object local space coordinates.
		glm::vec3 center{ 0.0f, 0.0f, 0.0f };

		// Calculate the center
		for (const glm::vec3& vertex : areaVolume->vertices)
		{
			center += vertex;
		}
		center /= static_cast<float>(areaVolume->vertices.size());

		// Center the volume on the new center point
		for (glm::vec3& vertex : areaVolume->vertices)
		{
			vertex -= center;
		}

		TransformComponent& transform = handles[areaIndex].get<TransformComponent>();
		transform.position = center;

		// Add render component with fill color from WldAreaComponent
		WldAreaComponent& wldArea = handles[areaIndex].get<WldAreaComponent>();
		AreaVolumeRenderComponent& renderComp = handles[areaIndex].emplace<AreaVolumeRenderComponent>();

		mq::MQColor fillColor = wldArea.color;
		fillColor.Alpha = 51; // 20% opacity
		renderComp.color = fillColor.ToABGR();
	}

	SPDLOG_INFO("Created {} WLD area entities from BSP tree", areaVolumeComponents.size());
}
