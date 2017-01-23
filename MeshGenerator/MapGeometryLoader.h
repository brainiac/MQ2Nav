
#pragma once

#pragma warning(push)
#pragma warning(disable : 4018)

#include "zone-utilities/common/s3d_loader.h"
#include "zone-utilities/common/eqg_loader.h"
#include "zone-utilities/common/eqg_v4_loader.h"

#pragma warning(pop)

#include <cstdint>
#include <string>
#include <map>
#include <tuple>

#include <glm/glm.hpp>

class MapGeometryLoader
{
public:
	MapGeometryLoader(const std::string& zoneShortName, const std::string& everquest_path,
		const std::string& mesh_path);
	~MapGeometryLoader();

	bool load();

	inline const std::string& getFileName() const { return m_zoneName; }

	inline const float* getVerts() const { return m_verts; }
	inline const float* getNormals() const { return m_normals; }
	inline const int* getTris() const { return m_tris; }
	inline int getVertCount() const { return m_vertCount; }
	inline int getTriCount() const { return m_triCount; }

	inline int GetDynamicObjectsCount() const { return m_dynamicObjects; }
	inline bool HasDynamicObjects() const { return m_hasDynamicObjects; }

private:
	bool Build();
	void LoadDoors();

	void TraverseBone(std::shared_ptr<EQEmu::S3D::SkeletonTrack::Bone> bone, glm::vec3 parent_trans, glm::vec3 parent_rot, glm::vec3 parent_scale);

	bool CompileS3D(
		std::vector<EQEmu::S3D::WLDFragment>& zone_frags,
		std::vector<EQEmu::S3D::WLDFragment>& zone_object_frags,
		std::vector<EQEmu::S3D::WLDFragment>& object_frags);
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

	std::map<std::tuple<float, float, float>, uint32_t> collide_vert_to_index;
	std::map<std::tuple<float, float, float>, uint32_t> non_collide_vert_to_index;

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
			union {
				struct {
					uint32_t v1, v2, v3;
				};
				uint32_t v[3];
			};
			uint8_t vis;
		};
		std::vector<glm::vec3> verts;
		std::vector<Poly> polys;
	};
	std::map<std::string, std::shared_ptr<ModelEntry>> m_models;



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
};
