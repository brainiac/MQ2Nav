#include "pch.h"
#include "ZoneCollisionMesh.h"

#include "meshgen/Entity.h"
#include "Recast.h"

#include "glm/gtc/type_ptr.hpp"
#include "spdlog/spdlog.h"

//============================================================================================================

ZoneCollisionMesh::ZoneCollisionMesh()
{
}

ZoneCollisionMesh::~ZoneCollisionMesh()
{
	clear();
}

void ZoneCollisionMesh::clear()
{
	delete[] m_verts;
	delete[] m_normals;
	delete[] m_tris;
}

void ZoneCollisionMesh::setMaxExtents(const std::pair<glm::vec3, glm::vec3>& maxExtents)
{
	m_maxExtents.first = maxExtents.first.zxy;
	m_maxExtents.second = maxExtents.second.zxy;

	m_maxExtentsSet = true;
}

void ZoneCollisionMesh::addVertex(float x, float y, float z)
{
	if (x < -10000 || y < -10000 || z < -10000)
		return;
	if (m_vertCount + 1 > vcap)
	{
		vcap = !vcap ? 8 : vcap * 2;
		float* nv = new float[vcap * 3];
		if (m_vertCount)
			memcpy(nv, m_verts, m_vertCount * 3 * sizeof(float));
		delete[] m_verts;
		m_verts = nv;
	}
	float* dst = &m_verts[m_vertCount * 3];
	*dst++ = x * m_scale;
	*dst++ = y * m_scale;
	*dst++ = z * m_scale;
	m_vertCount++;
}

void ZoneCollisionMesh::addTriangle(int a, int b, int c)
{
	if (m_triCount + 1 > tcap)
	{
		tcap = !tcap ? 8 : tcap * 2;
		int* nv = new int[tcap * 3];
		if (m_triCount)
			memcpy(nv, m_tris, m_triCount * 3 * sizeof(int));
		delete[] m_tris;
		m_tris = nv;
	}
	int* dst = &m_tris[m_triCount * 3];
	*dst++ = a;
	*dst++ = b;
	*dst++ = c;
	m_triCount++;
}

void ZoneCollisionMesh::addTriangle(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3)
{
	int index = m_vertCount;

	addVertex(v1.x, v1.y, v1.z);
	addVertex(v2.x, v2.y, v2.z);
	addVertex(v3.x, v3.y, v3.z);

	addTriangle(index, index + 1, index + 2);
}

void ZoneCollisionMesh::addTerrainSystem(const TerrainSystemPtr& terrain)
{
	const auto& tiles = terrain->GetTiles();
	uint32_t quads_per_tile = terrain->GetQuadsPerTile();
	float units_per_vertex = terrain->GetUnitsPerVertex();

	for (uint32_t i = 0; i < tiles.size(); ++i)
	{
		auto& tile = tiles[i];
		bool flat = tile->m_flat;

		float y = tile->m_tilePos.x;
		float x = tile->m_tilePos.y;

		if (flat)
		{
			float z = tile->m_heightField[0];

			// get x,y of corner point for this quad
			float dt = quads_per_tile * units_per_vertex;

			if (ArePointsOutsideExtents(glm::vec3{ y, x, z }))
				continue;

			int index = m_vertCount;

			addVertex(x, z, y);
			addVertex(x + dt, z, y);
			addVertex(x + dt, z, y + dt);
			addVertex(x, z, y + dt);

			addTriangle(index + 0, index + 2, index + 1);
			addTriangle(index + 2, index + 0, index + 3);
		}
		else
		{
			auto& floats = tile->m_heightField;
			int row_number = -1;

			for (uint32_t quad = 0; quad < terrain->m_quadCount; ++quad)
			{
				if (quad % quads_per_tile == 0)
					++row_number;

				if (tile->m_quadFlags[quad] & 0x01)
					continue;

				// get x,y of corner point for this quad
				float _x = x + (row_number * units_per_vertex);
				float _y = y + (quad % quads_per_tile) * units_per_vertex;
				float dt = units_per_vertex;

				float z1 = floats[quad + row_number];
				float z2 = floats[quad + row_number + quads_per_tile + 1];
				float z3 = floats[quad + row_number + quads_per_tile + 2];
				float z4 = floats[quad + row_number + 1];

				if (ArePointsOutsideExtents(glm::vec3{ _y, _x, z1 }))
					continue;

				int index = m_vertCount;

				addVertex(_x, z1, _y);
				addVertex(_x + dt, z2, _y);
				addVertex(_x + dt, z3, _y + dt);
				addVertex(_x, z4, _y + dt);

				addTriangle(index + 0, index + 2, index + 1);
				addTriangle(index + 2, index + 0, index + 3);
			}
		}
	}
}

void ZoneCollisionMesh::addTerrain(const TerrainPtr& terrain)
{
	auto* vertices = terrain->m_vertices.data();
	auto& faces = terrain->m_faces;

	for (const eqg::SFace& face : faces)
	{
		if (face.IsCollidable())
		{
			addTriangle(vertices[face.indices[0]], vertices[face.indices[1]], vertices[face.indices[2]]);
		}
	}
}

void ZoneCollisionMesh::addPolys(const std::vector<glm::vec3>& verts, const std::vector<uint32_t>& indices)
{
	for (uint32_t index = 0; index < indices.size(); index += 3)
	{
		const glm::vec3& vert1 = verts[indices[index]];
		const glm::vec3& vert2 = verts[indices[index + 2]];
		const glm::vec3& vert3 = verts[indices[index + 1]];

		if (ArePointsOutsideExtents(vert1.yxz, vert2.yxz, vert3.yxz))
			continue;

		int current_index = m_vertCount;

		addVertex(vert1.x, vert1.z, vert1.y);
		addVertex(vert2.x, vert2.z, vert2.y);
		addVertex(vert3.x, vert3.z, vert3.y);

		addTriangle(current_index, current_index + 2, current_index + 1);
	}
}

void ZoneCollisionMesh::addActor(entt::handle handle, const eqg::Actor* actor)
{
	TransformComponent& transform = handle.get<TransformComponent>();

	if (handle.any_of<HiddenComponent>())
		return;

	// Get model transform
	glm::mat4 mtx = GetWorldSpaceTransformMatrix(handle);

	if (IsPointOutsideExtents(glm::vec3{ mtx * glm::vec4{ 0., 0., 0., 1. } }))
	{
		SPDLOG_WARN("Ignoring placement of actor '{}' at {{ {:.2f} {:.2f} {:.2f} }} due to being outside of max extents",
			actor->GetActorName(), transform.position.x, transform.position.y, transform.position.z);
		return;
	}

	// If we have a collision model, use it. SimpleModels will set this if they have collision.
	eqg::SimpleModelPtr collisionModel = actor->GetCollisionModel();
	if (collisionModel)
	{
		auto pDefinition = collisionModel->GetDefinition();
		auto& faceIndices = pDefinition->m_collidableIndices;

		SPDLOG_DEBUG("Adding simple model {} to collision mesh ({} faces) at {:.2f} {:.2f} {:.2f}", actor->GetActorName(), faceIndices.size(),
			transform.position.x, transform.position.y, transform.position.z);

		if (addSimpleModel(mtx, collisionModel.get()))
			return;
	}

	// If this is a hierarchical model, we have two options: A special collision model represented by vertices
	// and indices, or we enumerate the bones and produce a mesh from the bones.
	eqg::HierarchicalModelPtr hModel = actor->GetHierarchicalModel();
	if (hModel)
	{
		auto pDefinition = hModel->GetDefinition();

		SPDLOG_DEBUG("Adding hierarchical model {} to collision mesh at {:.2f} {:.2f} {:.2f}", actor->GetActorName(),
			transform.position.x, transform.position.y, transform.position.z);

		if (addHierarchicalModel(mtx, hModel.get()))
			return;
	}

	SPDLOG_WARN("Ignoring placement of actor '{}': It has no collision model!", actor->GetActorName());
}

bool ZoneCollisionMesh::addSimpleModel(const glm::mat4& mtx, const eqg::SimpleModel* model)
{
	auto pDefinition = model->GetDefinition();
	auto& faces = pDefinition->m_faces;
	auto& faceIndices = pDefinition->m_collidableIndices;
	auto& vertices = pDefinition->m_vertices;

	for (uint32_t faceIndex : faceIndices)
	{
		eqg::SFace& face = faces[faceIndex];

		addTriangle(
			glm::vec3(mtx * glm::vec4(vertices[face.indices[0]], 1)),
			glm::vec3(mtx * glm::vec4(vertices[face.indices[1]], 1)),
			glm::vec3(mtx * glm::vec4(vertices[face.indices[2]], 1))
		);
	}

	return !faceIndices.empty();
}

bool ZoneCollisionMesh::addHierarchicalModel(const glm::mat4& mtx, const eqg::HierarchicalModel* model)
{
	auto pDefinition = model->GetDefinition();
	auto& vertices = pDefinition->m_collisionVertices;
	auto& indices = pDefinition->m_collisionIndices;

	if (!vertices.empty() && !indices.empty())
	{
		for (size_t i = 0; i < indices.size(); i += 3)
		{
			addTriangle(
				glm::vec3(mtx * glm::vec4(vertices[indices[i + 0]], 1)),
				glm::vec3(mtx * glm::vec4(vertices[indices[i + 1]], 1)),
				glm::vec3(mtx * glm::vec4(vertices[indices[i + 2]], 1))
			);
		}

		return true;
	}

	return false;
}


bool ZoneCollisionMesh::finalize()
{
	if (m_normals)
	{
		delete[] m_normals;
	}
	if (m_triCount > 0)
	{
		m_normals = new float[m_triCount * 3];

		for (int i = 0; i < m_triCount * 3; i += 3)
		{
			const float* v0 = &m_verts[m_tris[i] * 3];
			const float* v1 = &m_verts[m_tris[i + 1] * 3];
			const float* v2 = &m_verts[m_tris[i + 2] * 3];

			float e0[3], e1[3];
			for (int j = 0; j < 3; ++j)
			{
				e0[j] = v1[j] - v0[j];
				e1[j] = v2[j] - v0[j];
			}

			float* n = &m_normals[i];
			n[0] = e0[1] * e1[2] - e0[2] * e1[1];
			n[1] = e0[2] * e1[0] - e0[0] * e1[2];
			n[2] = e0[0] * e1[1] - e0[1] * e1[0];

			float d = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
			if (d > 0)
			{
				d = 1.0f / d;
				n[0] *= d;
				n[1] *= d;
				n[2] *= d;
			}
		}
	}

	if (m_vertCount > 0)
	{
		rcCalcBounds(m_verts, m_vertCount, glm::value_ptr(m_boundsMin), glm::value_ptr(m_boundsMax));

		SPDLOG_INFO("Building chunky triangle mesh");

		// Construct the partitioned triangle mesh
		m_chunkyMesh = std::make_unique<rcChunkyTriMesh>();

		if (!rcCreateChunkyTriMesh(m_verts, m_tris, m_triCount, 256, m_chunkyMesh.get()))
		{
			SPDLOG_ERROR("ZoneCollisionMesh::finalize: Failed to build chunky triangle mesh");
			return false;
		}

		return true;
	}

	SPDLOG_WARN("No vertices loaded?");
	return false;
}

//========================================================================
// RaycastMesh
//========================================================================

static bool intersectSegmentTriangle(const glm::vec3& sp, const glm::vec3& sq,
	const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& t)
{
	glm::vec3 ab = b - a;
	glm::vec3 ac = c - a;
	glm::vec3 qp = sp - sq;

	// Compute triangle normal. Can be precalculated or cached if
	// intersecting multiple segments against the same triangle
	glm::vec3 norm = glm::cross(ab, ac);

	// Compute denominator d. If d <= 0, segment is parallel to or points
	// away from triangle, so exit early
	float d = glm::dot(qp, norm);
	if (d <= 0.0f) return false;

	// Compute intersection t value of pq with plane of triangle. A ray
	// intersects iff 0 <= t. Segment intersects iff 0 <= t <= 1. Delay
	// dividing by d until intersection has been found to pierce triangle
	glm::vec3 ap = sp - a;
	t = glm::dot(ap, norm);
	if (t < 0.0f) return false;
	if (t > d) return false; // For segment; exclude this code line for a ray test

	// Compute barycentric coordinate components and test if within bounds
	glm::vec3 e = glm::cross(qp, ap);
	float v = glm::dot(ac, e);
	if (v < 0.0f || v > d) return false;
	float w = -glm::dot(ab, e);
	if (w < 0.0f || v + w > d) return false;

	// Segment/ray intersects triangle. Perform delayed division
	t /= d;

	return true;
}

static bool isectSegAABB(const glm::vec3& sp, const glm::vec3& sq,
	const glm::vec3& amin, const glm::vec3& amax, float& tmin, float& tmax)
{
	static constexpr float EPS = 1e-6f;

	glm::vec3 d = sq - sp;
	tmin = 0.0;
	tmax = 1.0f;

	for (int i = 0; i < 3; i++)
	{
		if (fabsf(d[i]) < EPS)
		{
			if (sp[i] < amin[i] || sp[i] > amax[i])
				return false;
		}
		else
		{
			const float ood = 1.0f / d[i];
			float t1 = (amin[i] - sp[i]) * ood;
			float t2 = (amax[i] - sp[i]) * ood;
			if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
			tmin = std::max(t1, tmin);
			if (t2 < tmax) tmax = t2;
			if (tmin > tmax) return false;
		}
	}

	return true;
}

bool ZoneCollisionMesh::RaycastMesh(const glm::vec3& src, const glm::vec3& dest, float& tMin)
{
	// Prune hit ray.
	float btmin, btmax;
	if (!isectSegAABB(src, dest, m_boundsMin, m_boundsMax, btmin, btmax))
		return false;

	glm::vec2 pq = dest.xz() - src.xz();

	glm::vec2 p, q;
	p.x = src.x + pq.x * btmin;
	p.y = src.z + pq.y * btmin;
	q.x = src.x + pq.x * btmax;
	q.y = src.z + pq.y * btmax;

	int cid[512];
	const int ncid = rcGetChunksOverlappingSegment(m_chunkyMesh.get(), glm::value_ptr(p), glm::value_ptr(q), cid, 512);
	if (!ncid)
		return false;

	tMin = 1.0f;
	bool hit = false;
	const glm::vec3* verts = reinterpret_cast<const glm::vec3*>(m_verts);

	for (int i = 0; i < ncid; ++i)
	{
		const rcChunkyTriMeshNode& node = m_chunkyMesh->nodes[cid[i]];
		const int* tris = &m_chunkyMesh->tris[node.i * 3];
		const int ntris = node.n;

		for (int j = 0; j < ntris * 3; j += 3)
		{
			float t = 1;
			if (intersectSegmentTriangle(src, dest, verts[tris[j]], verts[tris[j + 1]], verts[tris[j + 2]], t))
			{
				tMin = std::min(t, tMin);
				hit = true;
			}
		}
	}

	return hit;
}

