#include "pch.h"
#include "ZoneCollisionMesh.h"

#include "Recast.h"

#include <spdlog/spdlog.h>
#include <glm/gtc/type_ptr.hpp>

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

void ZoneCollisionMesh::addTriangle(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3)
{
	int index = m_vertCount;

	addVertex(v1.y, v1.z, v1.x);
	addVertex(v2.y, v2.z, v2.x);
	addVertex(v3.y, v3.z, v3.x);

	addTriangle(index, index + 1, index + 2);
}

void ZoneCollisionMesh::addTerrain(const TerrainPtr& terrain)
{
	const auto& tiles = terrain->GetTiles();
	uint32_t quads_per_tile = terrain->GetQuadsPerTile();
	float units_per_vertex = terrain->GetUnitsPerVertex();

	for (uint32_t i = 0; i < tiles.size(); ++i)
	{
		auto& tile = tiles[i];
		bool flat = tile->flat;

		float y = tile->tile_pos.x;
		float x = tile->tile_pos.y;

		if (flat)
		{
			float z = tile->height_field[0];

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
			auto& floats = tile->height_field;
			int row_number = -1;

			for (uint32_t quad = 0; quad < terrain->quad_count; ++quad)
			{
				if (quad % quads_per_tile == 0)
					++row_number;

				if (tile->quad_flags[quad] & 0x01)
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

// 0x10 = invisible
// 0x01 = no collision
auto isVisible = [](int flags)
	{
		bool invisible = false; // (flags & 0x01) || (flags & 0x10);

		return !invisible;
	};

// or?
// bool visible = (iter->flags & 0x11) == 0;

void ZoneCollisionMesh::addModel(std::string_view name, const S3DGeometryPtr& model)
{
	std::shared_ptr<ModelEntry> entry = std::make_shared<ModelEntry>();

	for (const auto& vert : model->GetVertices())
	{
		entry->verts.push_back(vert.pos);
	}

	for (const auto& poly : model->GetPolygons())
	{
		bool visible = isVisible(poly.flags);

		entry->polys.emplace_back(
			ModelEntry::Poly{
				.indices = glm::ivec3{poly.verts[0], poly.verts[1], poly.verts[2]},
				.vis = visible,
				.flags = poly.flags
			});
	}

	m_models.emplace(name, std::move(entry));
}

void ZoneCollisionMesh::addModel(std::string_view name, const EQGGeometryPtr& model)
{
	std::shared_ptr<ModelEntry> entry = std::make_shared<ModelEntry>();

	for (const auto& vert : model->GetVertices())
	{
		entry->verts.push_back(vert.pos);
	}

	for (const auto& poly : model->GetPolygons())
	{
		bool visible = isVisible(poly.flags);

		entry->polys.emplace_back(
			ModelEntry::Poly{
				.indices = glm::ivec3{poly.verts[0], poly.verts[1], poly.verts[2]},
				.vis = visible,
				.flags = poly.flags
			});
	}

	m_models.emplace(name, std::move(entry));
}

void ZoneCollisionMesh::addModelInstance(const PlaceablePtr& obj)
{
	const std::string& name = obj->tag;

	auto modelIter = m_models.find(name);
	if (modelIter == m_models.end())
	{
		SPDLOG_WARN("ZoneCollisionMesh::addModelInstance: No model definition found for tag '{}'", name);
		return;
	}

	// some objects have a really wild position, just ignore them.
	if (obj->GetZ() < -30000 || obj->GetX() > 15000 || obj->GetY() > 15000 || obj->GetZ() > 15000)
		return;

	// Get model transform
	auto mtx = obj->GetTransform();

	if (IsPointOutsideExtents(glm::vec3{ mtx * glm::vec4{ 0., 0., 0., 1. } }))
	{
		SPDLOG_WARN("Ignoring placement of '{}' at {{ {:.2f} {:.2f} {:.2f} }} due to being outside of max extents",
			obj->GetFileName(), obj->pos.x, obj->pos.y, obj->pos.z);
		return;
	}

	const auto& model = modelIter->second;
	for (const auto& poly : model->polys)
	{
		if (!poly.vis)
			continue;

		addTriangle(
			glm::vec3(mtx * glm::vec4(model->verts[poly.indices[0]], 1)),
			glm::vec3(mtx * glm::vec4(model->verts[poly.indices[1]], 1)),
			glm::vec3(mtx * glm::vec4(model->verts[poly.indices[2]], 1))
		);
	}
}

void ZoneCollisionMesh::addZoneGeometry(const S3DGeometryPtr& model)
{
	auto& mod_polys = model->GetPolygons();
	auto& mod_verts = model->GetVertices();

	for (uint32_t j = 0; j < mod_polys.size(); ++j)
	{
		auto& current_poly = mod_polys[j];
		auto v1 = mod_verts[current_poly.verts[0]];
		auto v2 = mod_verts[current_poly.verts[1]];
		auto v3 = mod_verts[current_poly.verts[2]];

		if ((current_poly.flags & eqg::S3D_FACEFLAG_PASSABLE) == 0)
		{
			addTriangle(v1.pos, v2.pos, v3.pos);
		}
	}
}

void ZoneCollisionMesh::addZoneGeometry(const EQGGeometryPtr& model)
{
	auto& mod_polys = model->GetPolygons();
	auto& mod_verts = model->GetVertices();

	for (uint32_t j = 0; j < mod_polys.size(); ++j)
	{
		auto& current_poly = mod_polys[j];
		auto v1 = mod_verts[current_poly.verts[0]];
		auto v2 = mod_verts[current_poly.verts[1]];
		auto v3 = mod_verts[current_poly.verts[2]];

		if ((current_poly.flags & 0x01) == 0)
		{
			addTriangle(v1.pos, v2.pos, v3.pos);
		}
	}
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

