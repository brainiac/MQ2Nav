//
// InputGeom.cpp
//

#include "meshgen/InputGeom.h"

#include <DebugDraw.h>
#include <DetourNavMesh.h>
#include <Recast.h>
#include <RecastDebugDraw.h>
#include <glm/gtc/type_ptr.inl>

static bool intersectSegmentTriangle(const glm::vec3& sp, const glm::vec3& sq,
	const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float &t)
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

//----------------------------------------------------------------------------

InputGeom::InputGeom(const std::string& zoneShortName, const std::string& eqPath)
	: m_eqPath(eqPath)
	, m_zoneShortName(zoneShortName)
{
}

InputGeom::~InputGeom()
{
}

bool InputGeom::loadGeometry(std::unique_ptr<MapGeometryLoader> loader, rcContext* ctx)
{
	m_chunkyMesh.reset();
	m_volumes.clear();

	m_loader = std::move(loader);
	if (!m_loader->load())
	{
		ctx->log(RC_LOG_ERROR, "buildTiledNavigation: Could not load '%s'",
			m_zoneShortName.c_str());
		return false;
	}

	if (m_loader->getVerts() == nullptr)
		return false;

	rcCalcBounds(m_loader->getVerts(), m_loader->getVertCount(),
		&m_meshBMin[0], &m_meshBMax[0]);

	// Construct the partitioned triangle mesh
	m_chunkyMesh.reset(new rcChunkyTriMesh);
	if (!rcCreateChunkyTriMesh(
		m_loader->getVerts(),        // verts
		m_loader->getTris(),         // tris
		m_loader->getTriCount(),     // ntris
		256,                         // trisPerChunk
		m_chunkyMesh.get()))         // [out] chunkyMesh
	{
		ctx->log(RC_LOG_ERROR, "buildTiledNavigation: Failed to build chunky mesh.");
		return false;
	}

	return true;
}

#pragma region Utilities
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
			if (t1 > tmin) tmin = t1;
			if (t2 < tmax) tmax = t2;
			if (tmin > tmax) return false;
		}
	}

	return true;
}

bool InputGeom::raycastMesh(const glm::vec3& src, glm::vec3& dst, float& tmin)
{
	glm::vec3 dir = dst - src;

	// Prune hit ray.
	float btmin, btmax;
	if (!isectSegAABB(src, dst, m_meshBMin, m_meshBMax, btmin, btmax))
		return false;

	glm::vec2 pq = dst.xz() - src.xz();

	glm::vec2 p, q;
	p.x = src.x + pq.x * btmin;
	p.y = src.z + pq.y * btmin;
	q.x = src.x + pq.x * btmax;
	q.y = src.z + pq.y * btmax;

	int cid[512];
	const int ncid = rcGetChunksOverlappingSegment(m_chunkyMesh.get(), glm::value_ptr(p), glm::value_ptr(q), cid, 512);
	if (!ncid)
		return false;

	tmin = 1.0f;
	bool hit = false;
	const glm::vec3* verts = reinterpret_cast<const glm::vec3*>(m_loader->getVerts());

	for (int i = 0; i < ncid; ++i)
	{
		const rcChunkyTriMeshNode& node = m_chunkyMesh->nodes[cid[i]];
		const int* tris = &m_chunkyMesh->tris[node.i * 3];
		const int ntris = node.n;

		for (int j = 0; j < ntris * 3; j += 3)
		{
			float t = 1;
			if (intersectSegmentTriangle(src, dst, verts[tris[j]], verts[tris[j + 1]], verts[tris[j + 2]], t))
			{
				if (t < tmin)
					tmin = t;
				hit = true;
			}
		}
	}

	return hit;
}
#pragma endregion
