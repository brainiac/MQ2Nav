#pragma once

#include "meshgen/ChunkyTriMesh.h"
#include "eqglib/wld_loader.h"
#include "eqglib/eqg_loader.h"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

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

	//void addModel(std::string_view name, const S3DGeometryPtr& model);
	void addModel(std::string_view name, const EQGGeometryPtr& model);
	void addModelInstance(const PlaceablePtr& inst);
	//void addZoneGeometry(const S3DGeometryPtr& model); // For s3d zone geometry
	void addZoneGeometry(const EQGGeometryPtr& model); // For TER zone geometry

	bool finalize();

	void setMaxExtents(const std::pair<glm::vec3, glm::vec3>& maxExtents);

	// TOOD: Collapse this impl into the collision mesh itself
	const rcChunkyTriMesh* GetChunkyMesh() { return m_chunkyMesh.get(); }
	bool RaycastMesh(const glm::vec3& src, const glm::vec3& dest, float& tMin);

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
		std::vector<glm::vec3> verts;
		std::vector<eqg::SFace> polys;
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

	std::unique_ptr<rcChunkyTriMesh> m_chunkyMesh;
};

