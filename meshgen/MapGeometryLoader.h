//
// MapGeometryLoader.h
//

#pragma once

#pragma warning(push)
#pragma warning(disable : 4018)

#include <zone-utilities/common/s3d_loader.h>
#include <zone-utilities/common/eqg_loader.h>
#include <zone-utilities/common/eqg_v4_loader.h>

#pragma warning(pop)

#include <cstdint>
#include <string>
#include <map>
#include <tuple>
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

class MapGeometryLoader
{
public:
	MapGeometryLoader(const std::string& zoneShortName, const std::string& everquest_path,
		const std::string& mesh_path);
	~MapGeometryLoader();

	void SetMaxExtents(const std::pair<glm::vec3, glm::vec3>& maxExtents);

	bool load();

	const std::string& getFileName() const { return m_zoneName; }

	const float* getVerts() const { return m_verts; }
	const float* getNormals() const { return m_normals; }
	const int* getTris() const { return m_tris; }
	int getVertCount() const { return m_vertCount; }
	int getTriCount() const { return m_triCount; }

	int GetDynamicObjectsCount() const { return m_dynamicObjects; }
	bool HasDynamicObjects() const { return m_hasDynamicObjects; }

private:
	bool Build();
	void LoadDoors();

	void TraverseBone(std::shared_ptr<EQEmu::S3D::SkeletonTrack::Bone> bone, glm::vec3 parent_trans, glm::vec3 parent_rot, glm::vec3 parent_scale);

	bool CompileS3D(
		EQEmu::S3D::S3DLoader& zone_loader,
		EQEmu::S3D::S3DLoader& zone_object_loader,
		EQEmu::S3D::S3DLoader& object_loader,
		EQEmu::S3D::S3DLoader& object2_loader);

	bool CompileEQG(
		std::vector<std::shared_ptr<EQEmu::EQG::Geometry>>& models,
		std::vector<std::shared_ptr<EQEmu::Placeable>>& placeables,
		std::vector<std::shared_ptr<EQEmu::EQG::Region>>& regions,
		std::vector<std::shared_ptr<EQEmu::Light>>& lights);
	bool CompileEQGv4();

	void AddFace(glm::vec3& v1, glm::vec3& v2, glm::vec3& v3, bool collidable);

	std::vector<glm::vec3> collide_verts;
	std::vector<uint32_t> collide_indices;

	std::vector<glm::vec3> non_collide_verts;
	std::vector<uint32_t> non_collide_indices;

	uint32_t current_collide_index = 0;
	uint32_t current_non_collide_index = 0;

	std::unordered_map<glm::vec3, uint32_t, KeyFuncs, KeyFuncs> collide_vert_to_index;
	std::unordered_map<glm::vec3, uint32_t, KeyFuncs, KeyFuncs> non_collide_vert_to_index;

	std::shared_ptr<EQEmu::EQG::Terrain> terrain;
	std::map<std::string, std::shared_ptr<EQEmu::S3D::Geometry>> map_models;
	std::map<std::string, std::shared_ptr<EQEmu::EQG::Geometry>> map_eqg_models;
	std::vector<std::shared_ptr<EQEmu::Placeable>> map_placeables;
	std::vector<std::shared_ptr<EQEmu::PlaceableGroup>> map_group_placeables;

	int m_dynamicObjects = 0;
	bool m_hasDynamicObjects = false;

	struct ModelEntry
	{
	public:
		struct Poly
		{
			glm::ivec3 indices;
			uint8_t vis;
			uint32_t flags;
		};
		std::vector<glm::vec3> verts;
		std::vector<Poly> polys;
	};
	std::map<std::string, std::shared_ptr<ModelEntry>> m_models;

	std::pair<glm::vec3, glm::vec3> m_maxExtents;
	bool m_maxExtentsSet = false;

	template <typename VecT>
	inline bool IsPointOutsideExtents(const VecT& p)
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

	template<typename... Args>
	inline bool ArePointsOutsideExtents(Args &&... args)
	{
		bool outside = false;
		(void)std::initializer_list<bool>{
			outside = outside || IsPointOutsideExtents(std::forward<Args>(args))...};
		return outside;
	}

	//---------------------------------------------------------------------------

	void addVertex(float x, float y, float z);
	void addTriangle(int a, int b, int c);

	int vcap = 0, tcap = 0;
	float m_scale = 1.0;
	float* m_verts = 0;
	int* m_tris = 0;
	float* m_normals = 0;
	int m_vertCount = 0;
	int m_triCount = 0;

	std::string m_zoneName;
	std::string m_eqPath;
	std::string m_meshPath;

	bool m_doorsLoaded = false;

	std::vector<std::unique_ptr<EQEmu::PFS::Archive>> m_archives;
	std::vector<std::unique_ptr<EQEmu::S3D::S3DLoader>> m_s3dLoaders;
};
