//
// MapGeometryLoader.h
//

#pragma once

#include "ZoneCollisionMesh.h"
#include "zone-utilities/common/wld_loader.h"
#include "zone-utilities/common/eqg_loader.h"

#include <cstdint>
#include <string>
#include <map>
#include <tuple>
#include <unordered_map>

#include <glm/glm.hpp>

class MapGeometryLoader
{
public:
	MapGeometryLoader(const std::string& zoneShortName, const std::string& everquest_path,
		const std::string& mesh_path);
	~MapGeometryLoader();

	void SetMaxExtents(const std::pair<glm::vec3, glm::vec3>& maxExtents)
	{
		m_collisionMesh.setMaxExtents(maxExtents);
	}

	bool Load();

	void BuildCollisionMesh();
	ZoneCollisionMesh& GetCollisionMesh() { return m_collisionMesh; }

	const std::string& getFileName() const { return m_zoneName; }

	int GetDynamicObjectsCount() const { return m_dynamicObjects; }
	bool HasDynamicObjects() const { return m_hasDynamicObjects; }

	bool IsLoaded() const { return m_loaded; }

private:
	void Clear();

	bool LoadZone();
	bool LoadGlobalData();

	EQEmu::PFS::Archive* LoadArchive(const std::string& path);
	EQEmu::EQGLoader* LoadEQG(std::string_view fileName);
	EQEmu::EQGLoader* LoadEQG(EQEmu::PFS::Archive* archive);
	EQEmu::S3D::WLDLoader* LoadS3D(std::string_view fileName, std::string_view wldFile = "");
	EQEmu::S3D::WLDLoader* LoadWLD(EQEmu::PFS::Archive* archive, const std::string& fileName);

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

	void TraverseBone(std::shared_ptr<EQEmu::S3D::SkeletonTrack::Bone> bone, glm::vec3 parent_trans, glm::vec3 parent_rot, glm::vec3 parent_scale);
	bool LoadModelInst(const PlaceablePtr& model_inst);

	void AddFace(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, bool collidable);

	//---------------------------------------------------------------------------

	std::string m_zoneName;
	std::string m_eqPath;
	std::string m_meshPath;

	ZoneCollisionMesh m_collisionMesh;
	bool m_doorsLoaded = false;
	bool m_loaded = false;

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
	std::map<std::string_view, S3DGeometryPtr> map_models;
	std::map<std::string_view, SkeletonTrackPtr> map_anim_models;
	std::map<std::string, EQGGeometryPtr> map_eqg_models;
	EQGGeometryPtr terrain_model;

	std::vector<PlaceablePtr> map_placeables;
	std::vector<S3DGeometryPtr> map_s3d_geometry;
	std::vector<TerrainAreaPtr> map_areas;
	std::vector<std::shared_ptr<EQEmu::Light>> map_lights;

	std::vector<GlobalLoadAsset> global_load_assets;

	int m_dynamicObjects = 0;
	bool m_hasDynamicObjects = false;

	std::vector<std::unique_ptr<EQEmu::PFS::Archive>> m_archives;
	std::vector<std::unique_ptr<EQEmu::S3D::WLDLoader>> m_wldLoaders;
	std::vector<std::unique_ptr<EQEmu::EQGLoader>> m_eqgLoaders;
};
