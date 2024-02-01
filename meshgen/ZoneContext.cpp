
#include "pch.h"
#include "ZoneContext.h"

#include "meshgen/ApplicationConfig.h"
#include "meshgen/ChunkyTriMesh.h"
#include "meshgen/MapGeometryLoader.h"
#include "meshgen/RecastContext.h"
#include "meshgen/ZoneResourceManager.h"
#include "common/NavMeshData.h"

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

ZoneContext::ZoneContext(std::string_view zoneShortName)
	: m_zoneShortName(zoneShortName)
{
	// Get the long name from the application config
	m_zoneLongName = g_config.GetLongNameForShortName(m_zoneShortName);
	m_displayName = fmt::format("{} ({})", m_zoneLongName, m_zoneShortName);

	m_resourceMgr = std::make_unique<ZoneResourceManager>(m_zoneShortName);
	m_rcContext = std::make_unique<RecastContext>();
}

ZoneContext::~ZoneContext()
{
}

bool ZoneContext::LoadZone()
{
	std::string eqPath = g_config.GetEverquestPath();
	std::string outputPath = g_config.GetOutputPath();

	m_loader = std::make_unique<MapGeometryLoader>(m_zoneShortName, eqPath, outputPath);

	if (g_config.GetUseMaxExtents())
	{
		auto iter = MaxZoneExtents.find(m_zoneShortName);
		if (iter != MaxZoneExtents.end())
		{
			m_loader->SetMaxExtents(iter->second);
		}
	}

	if (!m_loader->load())
	{
		SPDLOG_ERROR("LoadZone: Failed to load '{}'", m_zoneShortName);
		return false;
	}

	if (m_loader->getVerts() == nullptr)
	{
		SPDLOG_ERROR("LoadZone: Zone loaded but had no geometry: '{}'", m_zoneShortName);
		return false;
	}

	rcCalcBounds(m_loader->getVerts(), m_loader->getVertCount(),
		&m_meshBMin[0], &m_meshBMax[0]);

	m_zoneLoaded = true;
	return true;
}

bool ZoneContext::BuildTriangleMesh()
{
	assert(m_loader != nullptr);
	assert(m_zoneLoaded);

	// Construct the partitioned triangle mesh
	m_chunkyMesh = std::make_unique<rcChunkyTriMesh>();

	if (!rcCreateChunkyTriMesh(
		m_loader->getVerts(),
		m_loader->getTris(),
		m_loader->getTriCount(),
		256,
		m_chunkyMesh.get()))
	{
		SPDLOG_ERROR("LoadGeometry: Failed to build chunky triangle mesh");
		return false;
	}

	return true;
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
			if (t1 > tmin) tmin = t1;
			if (t2 < tmax) tmax = t2;
			if (tmin > tmax) return false;
		}
	}

	return true;
}

bool ZoneContext::RaycastMesh(const glm::vec3& src, const glm::vec3& dest, float& tMin)
{
	glm::vec3 dir = dest - src;

	// Prune hit ray.
	float btmin, btmax;
	if (!isectSegAABB(src, dest, m_meshBMin, m_meshBMax, btmin, btmax))
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
	const glm::vec3* verts = reinterpret_cast<const glm::vec3*>(m_loader->getVerts());

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
				if (t < tMin)
					tMin = t;
				hit = true;
			}
		}
	}

	return hit;
}