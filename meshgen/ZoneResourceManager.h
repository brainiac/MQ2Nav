//
// MapGeometryLoader.h
//

#pragma once

#include "eqglib/eqglib.h"
#include "meshgen/Entity.h"

#include "glm/glm.hpp"
#include <cstdint>
#include <mutex>
#include <string>
#include <map>
#include <unordered_map>

class Scene;

struct KeyFuncs
{
	size_t operator()(const glm::vec3& k)const
	{
		return std::hash<float>()(k.x) ^ std::hash<float>()(k.y) ^ std::hash<float>()(k.z);
	}

	bool operator()(const glm::vec3& a, const glm::vec3& b)const
	{
		return a == b;
	}
};

class ZoneCollisionMesh;

class ZoneResourceManager
{
public:
	ZoneResourceManager(const std::string& zoneShortName, const std::string& everquest_path,
		const std::string& mesh_path, const std::shared_ptr<Scene>& scene);
	~ZoneResourceManager();

	bool Load();

	bool BuildCollisionMesh(ZoneCollisionMesh& collisionMesh);

	const std::string& getFileName() const { return m_zoneName; }

	void SetLoadSocials(bool socials) { m_skipSocials = !socials; }

	int GetDynamicObjectsCount() const { return m_dynamicObjects; }
	bool HasDynamicObjects() const { return m_hasDynamicObjects; }

	bool IsLoaded() const { return m_loaded; }

	int GetNumArchives() const;
	eqg::Archive* GetArchive(int index) const;

	bool BuildScene(Scene& scene);

private:
	void Clear();

	bool LoadZone();
	bool LoadGlobalData();

	eqg::Archive* LoadArchive(const std::string& path);
	eqg::EQGLoader* LoadEQG(std::string_view fileName, int loadFlags = 0);
	eqg::EQGLoader* LoadEQG(eqg::Archive* archive, int loadFlags = 0);
	eqg::WLDLoader* LoadS3D(std::string_view fileName, int loadFlags = 0, std::string_view wldFile = "");
	eqg::WLDLoader* LoadWLD(eqg::Archive* archive, const std::string& fileName, int loadFlags = 0);

	void LoadAssetsFile();
	void LoadDoors();

	struct GlobalLoadAsset
	{
		int phase = -1;
		bool itemAnims = false; // 3
		bool luclinAnims = false; // 4
		std::string fileName;
		std::string message;
	};
	void LoadGlobalLoadFile();
	void PerformGlobalLoad(int phase);
	void LoadGlobalChr();

	void AddActor(const eqg::ActorPtr& actor);
	void RemoveActor(const eqg::ActorPtr& actor);

	void AddPointLight(const eqg::PointLightPtr& light);
	void RemovePointLight(const eqg::PointLightPtr& light);

	void AddArea(const eqg::TerrainAreaPtr& areaPtr);

	void AddFace(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, bool collidable);

	//---------------------------------------------------------------------------

	std::string m_zoneName;
	std::string m_eqPath;
	std::string m_meshPath;

	bool m_doorsLoaded = false;
	bool m_loaded = false;

	bool m_skipSocials = false;
	bool m_loadLuclinModels = false;
	bool m_loadLuclinElementals = false; // luclin mounts/elementals
	int m_defaultLoadFlags = 0;
	std::unique_ptr<eqg::ResourceManager> m_globalResourceMgr;
	std::unique_ptr<eqg::ResourceManager> m_resourceMgr;

	std::vector<glm::vec3> collide_verts;
	std::vector<uint32_t> collide_indices;

	std::vector<glm::vec3> non_collide_verts;
	std::vector<uint32_t> non_collide_indices;

	uint32_t current_collide_index = 0;
	uint32_t current_non_collide_index = 0;

	// Used to optimize collision meshes
	std::unordered_map<glm::vec3, uint32_t, KeyFuncs, KeyFuncs> collide_vert_to_index;
	std::unordered_map<glm::vec3, uint32_t, KeyFuncs, KeyFuncs> non_collide_vert_to_index;

	std::vector<GlobalLoadAsset> global_load_assets;

	int m_dynamicObjects = 0;
	bool m_hasDynamicObjects = false;

	mutable std::mutex m_archiveMutex;
	std::vector<std::unique_ptr<eqg::Archive>> m_archives;
	std::vector<std::unique_ptr<eqg::WLDLoader>> m_wldLoaders;
	std::vector<std::unique_ptr<eqg::EQGLoader>> m_eqgLoaders;

	// Our scene object
	std::shared_ptr<Scene> m_scene;

	// List of all currently known actors
	std::unordered_map<eqg::Actor*, entt::entity> m_actors;

	// List of all currently known point light instances
	std::unordered_map<eqg::PointLight*, entt::entity> m_lights;

	// List of all currently known terrain areas
	std::unordered_map<eqg::TerrainArea*, entt::entity> m_areas;
};
