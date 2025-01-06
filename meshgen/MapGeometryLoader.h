//
// MapGeometryLoader.h
//

#pragma once

#include "zone-utilities/common/wld_loader.h"
#include "zone-utilities/common/eqg_loader.h"

#include <cstdint>
#include <string>
#include <map>
#include <tuple>
#include <unordered_map>

#include <glm/glm.hpp>

using PlaceablePtr = std::shared_ptr<EQEmu::Placeable>;
using PlaceableGroupPtr = std::shared_ptr<EQEmu::PlaceableGroup>;
using S3DGeometryPtr = std::shared_ptr<EQEmu::S3D::Geometry>;
using SkeletonTrackPtr = std::shared_ptr<EQEmu::S3D::SkeletonTrack>;
using EQGGeometryPtr = std::shared_ptr<EQEmu::EQG::Geometry>;
using TerrainPtr = std::shared_ptr<EQEmu::EQG::Terrain>;

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

class ZoneCollisionMesh
{
public:
	ZoneCollisionMesh();
	~ZoneCollisionMesh();

	void clear();

	const float* getVerts() const { return m_verts; }
	const float* getNormals() const { return m_normals; }
	const int* getTris() const { return m_tris; }
	int getVertCount() const { return m_vertCount; }
	int getTriCount() const { return m_triCount; }

	void addVertex(float x, float y, float z);
	void addTriangle(int a, int b, int c);
	void addTriangle(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3);

	void addTerrain(const TerrainPtr& terrain);
	void addPolys(const std::vector<glm::vec3>& verts, const std::vector<uint32_t>& indices);

	void addModel(std::string_view name, const S3DGeometryPtr& model);
	void addModel(std::string_view name, const EQGGeometryPtr& model);
	void addModelInstance(const PlaceablePtr& inst, const glm::mat4x4& transform);
	void addZoneGeometry(const S3DGeometryPtr& model); // For s3d zone geometry

	void finalize();

	void setMaxExtents(const std::pair<glm::vec3, glm::vec3>& maxExtents);

	template<typename... Args>
	bool ArePointsOutsideExtents(Args &&... args)
	{
		bool outside = false;
		(void)std::initializer_list<bool>{
			outside = outside || IsPointOutsideExtents(std::forward<Args>(args))...};
		return outside;
	}

	template <typename VecT>
	bool IsPointOutsideExtents(const VecT& p)
	{
		if (!m_maxExtentsSet)
			return false;

		for (int i = 0; i < 3; i++)
		{
			if ((m_maxExtents.first[i] != 0. && p[i] < m_maxExtents.first[i])
				|| (m_maxExtents.second[i] != 0. && p[i] > m_maxExtents.second[i]))
			{
				return true;
			}
		}

		return false;
	}

	// model collision mesh
	struct ModelEntry
	{
		struct Poly
		{
			glm::ivec3 indices;
			uint8_t vis;
			uint32_t flags;
		};
		std::vector<glm::vec3> verts;
		std::vector<Poly> polys;
	};
	std::unordered_map<std::string_view, std::shared_ptr<ModelEntry>> m_models;

	std::pair<glm::vec3, glm::vec3> m_maxExtents;
	bool m_maxExtentsSet = false;
	glm::vec3 m_boundsMin, m_boundsMax;

	int vcap = 0, tcap = 0;
	float m_scale = 1.0;
	float* m_verts = nullptr;
	int* m_tris = nullptr;
	float* m_normals = nullptr;
	int m_vertCount = 0;
	int m_triCount = 0;
};

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
	void LoadDoors();

	bool TryLoadEQG();
	bool TryLoadS3D();

	void TraverseBone(std::shared_ptr<EQEmu::S3D::SkeletonTrack::Bone> bone, glm::vec3 parent_trans, glm::vec3 parent_rot, glm::vec3 parent_scale);

	bool CompileS3D(EQEmu::S3D::WLDLoader* zone_loader);
	bool CompileEQG(EQEmu::EQGLoader& eqgLoader);
	bool CompileEQGv4();

	bool LoadModelInst(const PlaceablePtr& model_inst);

	EQEmu::PFS::Archive* LoadArchive(const std::string& path);

	EQEmu::S3D::WLDLoader* LoadWLD(EQEmu::PFS::Archive* archive, const std::string& fileName);
	void LoadAssetsFile();

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

	std::unordered_map<glm::vec3, uint32_t, KeyFuncs, KeyFuncs> collide_vert_to_index;
	std::unordered_map<glm::vec3, uint32_t, KeyFuncs, KeyFuncs> non_collide_vert_to_index;

	TerrainPtr terrain;
	std::map<std::string_view, S3DGeometryPtr> map_models;
	std::map<std::string_view, SkeletonTrackPtr> map_anim_models;
	std::map<std::string, EQGGeometryPtr> map_eqg_models;

	std::vector<PlaceablePtr> map_placeables;
	std::vector<PlaceableGroupPtr> map_group_placeables;

	std::vector<S3DGeometryPtr> map_s3d_geometry;
	std::vector<PlaceablePtr> map_s3d_model_instances;
	std::vector<PlaceablePtr> dynamic_map_objects;

	int m_dynamicObjects = 0;
	bool m_hasDynamicObjects = false;

	std::vector<std::unique_ptr<EQEmu::PFS::Archive>> m_archives;
	std::vector<std::unique_ptr<EQEmu::S3D::WLDLoader>> m_wldLoaders;
};
