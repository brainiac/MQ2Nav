//
// MapGeometryLoader.h
//

#pragma once

#include "eqglib/wld_loader.h"
#include "eqglib/eqg_loader.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <map>
#include <unordered_map>

#include <glm/glm.hpp>

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
		const std::string& mesh_path);
	~ZoneResourceManager();

	bool Load();

	bool BuildCollisionMesh(ZoneCollisionMesh& collisionMesh);

	const std::string& getFileName() const { return m_zoneName; }

	int GetDynamicObjectsCount() const { return m_dynamicObjects; }
	bool HasDynamicObjects() const { return m_hasDynamicObjects; }

	bool IsLoaded() const { return m_loaded; }

	int GetNumArchives() const;
	eqg::Archive* GetArchive(int index) const;

private:
	void Clear();

	bool LoadZone();
	bool LoadGlobalData();

	eqg::Archive* LoadArchive(const std::string& path);
	eqg::EQGLoader* LoadEQG(std::string_view fileName);
	eqg::EQGLoader* LoadEQG(eqg::Archive* archive);
	eqg::WLDLoader* LoadS3D(std::string_view fileName, std::string_view wldFile = "");
	eqg::WLDLoader* LoadWLD(eqg::Archive* archive, const std::string& fileName);

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

	//void TraverseBone(std::shared_ptr<eqg::s3d::SkeletonTrack::Bone> bone, glm::vec3 parent_trans, glm::vec3 parent_rot, glm::vec3 parent_scale);
	bool LoadModelInst(const PlaceablePtr& model_inst);

	void AddFace(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, bool collidable);

	//---------------------------------------------------------------------------

	std::string m_zoneName;
	std::string m_eqPath;
	std::string m_meshPath;

	bool m_doorsLoaded = false;
	bool m_loaded = false;

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

	TerrainPtr terrain;
	//std::map<std::string_view, S3DGeometryPtr> map_models;
	//std::map<std::string_view, SkeletonTrackPtr> map_anim_models;
	std::map<std::string, EQGGeometryPtr> map_eqg_models;
	EQGGeometryPtr terrain_model;
	std::vector<std::shared_ptr<eqg::LODList>> lod_lists;

	std::vector<PlaceablePtr> map_placeables;
	//std::vector<S3DGeometryPtr> map_s3d_geometry;
	std::vector<TerrainAreaPtr> map_areas;
	std::vector<std::shared_ptr<eqg::Light>> map_lights;

	std::vector<GlobalLoadAsset> global_load_assets;

	int m_dynamicObjects = 0;
	bool m_hasDynamicObjects = false;

	mutable std::mutex m_archiveMutex;
	std::vector<std::unique_ptr<eqg::Archive>> m_archives;
	std::vector<std::unique_ptr<eqg::WLDLoader>> m_wldLoaders;
	std::vector<std::unique_ptr<eqg::EQGLoader>> m_eqgLoaders;
};
