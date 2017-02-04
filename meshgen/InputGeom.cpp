//
// InputGeom.cpp
//

#include "InputGeom.h"

#include <DebugDraw.h>
#include <DetourNavMesh.h>
#include <Recast.h>
#include <RecastDebugDraw.h>

static bool intersectSegmentTriangle(const float* sp, const float* sq,
	const float* a, const float* b, const float* c,
	float &t)
{
	float v, w;
	float ab[3], ac[3], qp[3], ap[3], norm[3], e[3];
	rcVsub(ab, b, a);
	rcVsub(ac, c, a);
	rcVsub(qp, sp, sq);

	// Compute triangle normal. Can be precalculated or cached if
	// intersecting multiple segments against the same triangle
	rcVcross(norm, ab, ac);

	// Compute denominator d. If d <= 0, segment is parallel to or points
	// away from triangle, so exit early
	float d = rcVdot(qp, norm);
	if (d <= 0.0f) return false;

	// Compute intersection t value of pq with plane of triangle. A ray
	// intersects iff 0 <= t. Segment intersects iff 0 <= t <= 1. Delay
	// dividing by d until intersection has been found to pierce triangle
	rcVsub(ap, sp, a);
	t = rcVdot(ap, norm);
	if (t < 0.0f) return false;
	if (t > d) return false; // For segment; exclude this code line for a ray test

	// Compute barycentric coordinate components and test if within bounds
	rcVcross(e, qp, ap);
	v = rcVdot(ac, e);
	if (v < 0.0f || v > d) return false;
	w = -rcVdot(ab, e);
	if (w < 0.0f || v + w > d) return false;

	// Segment/ray intersects triangle. Perform delayed division
	t /= d;

	return true;
}

//----------------------------------------------------------------------------

InputGeom::InputGeom(const std::string& zoneShortName, const std::string& eqPath, const std::string& meshPath)
	: m_zoneShortName(zoneShortName)
	, m_eqPath(eqPath)
	, m_meshPath(meshPath)
{
}

InputGeom::~InputGeom()
{
}

bool InputGeom::loadGeometry(rcContext* ctx)
{
	m_chunkyMesh.reset();
	m_offMeshConCount = 0;
	m_volumes.clear();

	m_loader.reset(new MapGeometryLoader(m_zoneShortName, m_eqPath, m_meshPath));
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
static bool isectSegAABB(const float* sp, const float* sq,
	const float* amin, const float* amax, float& tmin, float& tmax)
{
	static const float EPS = 1e-6f;

	float d[3];
	d[0] = sq[0] - sp[0];
	d[1] = sq[1] - sp[1];
	d[2] = sq[2] - sp[2];
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

bool InputGeom::raycastMesh(float* src, float* dst, float& tmin)
{
	float dir[3];
	rcVsub(dir, dst, src);

	// Prune hit ray.
	float btmin, btmax;
	if (!isectSegAABB(src, dst, &m_meshBMin[0], &m_meshBMax[0], btmin, btmax))
		return false;
	float p[2], q[2];
	p[0] = src[0] + (dst[0]-src[0])*btmin;
	p[1] = src[2] + (dst[2]-src[2])*btmin;
	q[0] = src[0] + (dst[0]-src[0])*btmax;
	q[1] = src[2] + (dst[2]-src[2])*btmax;

	int cid[512];
	const int ncid = rcGetChunksOverlappingSegment(m_chunkyMesh.get(), p, q, cid, 512);
	if (!ncid)
		return false;

	tmin = 1.0f;
	bool hit = false;
	const float* verts = m_loader->getVerts();

	for (int i = 0; i < ncid; ++i)
	{
		const rcChunkyTriMeshNode& node = m_chunkyMesh->nodes[cid[i]];
		const int* tris = &m_chunkyMesh->tris[node.i*3];
		const int ntris = node.n;

		for (int j = 0; j < ntris*3; j += 3)
		{
			float t = 1;
			if (intersectSegmentTriangle(
				src, dst,
				&verts[tris[j]*3],
				&verts[tris[j+1]*3],
				&verts[tris[j+2]*3], t))
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

#pragma region Off-Mesh connections
void InputGeom::addOffMeshConnection(const float* spos, const float* epos, const float rad,
	unsigned char bidir, unsigned char area, unsigned short flags)
{
	if (m_offMeshConCount >= MAX_OFFMESH_CONNECTIONS) return;
	float* v = &m_offMeshConVerts[m_offMeshConCount*3*2];
	m_offMeshConRads[m_offMeshConCount] = rad;
	m_offMeshConDirs[m_offMeshConCount] = bidir;
	m_offMeshConAreas[m_offMeshConCount] = area;
	m_offMeshConFlags[m_offMeshConCount] = flags;
	m_offMeshConId[m_offMeshConCount] = 1000 + m_offMeshConCount;
	rcVcopy(&v[0], spos);
	rcVcopy(&v[3], epos);
	m_offMeshConCount++;
}

void InputGeom::deleteOffMeshConnection(int i)
{
	m_offMeshConCount--;
	float* src = &m_offMeshConVerts[m_offMeshConCount*3*2];
	float* dst = &m_offMeshConVerts[i*3*2];
	rcVcopy(&dst[0], &src[0]);
	rcVcopy(&dst[3], &src[3]);
	m_offMeshConRads[i] = m_offMeshConRads[m_offMeshConCount];
	m_offMeshConDirs[i] = m_offMeshConDirs[m_offMeshConCount];
	m_offMeshConAreas[i] = m_offMeshConAreas[m_offMeshConCount];
	m_offMeshConFlags[i] = m_offMeshConFlags[m_offMeshConCount];
}

void InputGeom::drawOffMeshConnections(duDebugDraw* dd, bool hilight)
{
	unsigned int conColor = duRGBA(192,0,128,192);
	unsigned int baseColor = duRGBA(0,0,0,64);
	dd->depthMask(false);

	dd->begin(DU_DRAW_LINES, 2.0f);
	for (int i = 0; i < m_offMeshConCount; ++i)
	{
		float* v = &m_offMeshConVerts[i*3*2];

		dd->vertex(v[0],v[1],v[2], baseColor);
		dd->vertex(v[0],v[1]+0.2f,v[2], baseColor);

		dd->vertex(v[3],v[4],v[5], baseColor);
		dd->vertex(v[3],v[4]+0.2f,v[5], baseColor);

		duAppendCircle(dd, v[0],v[1]+0.1f,v[2], m_offMeshConRads[i], baseColor);
		duAppendCircle(dd, v[3],v[4]+0.1f,v[5], m_offMeshConRads[i], baseColor);

		if (hilight)
		{
			duAppendArc(dd, v[0],v[1],v[2], v[3],v[4],v[5], 0.25f,
				(m_offMeshConDirs[i] & 1) ? 0.6f : 0.0f, 0.6f, conColor);
		}
	}
	dd->end();

	dd->depthMask(true);
}
#pragma endregion

#pragma region Convex Volumes
void InputGeom::addConvexVolume(const std::vector<glm::vec3>& verts,
	const float minh, const float maxh, PolyArea area)
{
	auto vol = std::make_unique<ConvexVolume>();
	vol->areaType = area;
	vol->hmax = maxh;
	vol->hmin = minh;
	vol->verts = verts;

	m_volumes.push_back(std::move(vol));
}

void InputGeom::deleteConvexVolume(size_t i)
{
	if (i < m_volumes.size())
	{
		m_volumes.erase(m_volumes.begin() + i);
	}
}

void InputGeom::drawConvexVolumes(struct duDebugDraw* dd, bool /*hilight*/)
{
	dd->depthMask(false);

	dd->begin(DU_DRAW_TRIS);
	for (const auto& vol : m_volumes)
	{
		uint32_t col = duIntToCol(static_cast<int>(vol->areaType), 32);
		size_t nverts = vol->verts.size();

		for (size_t j = 0, k = nverts - 1; j < nverts; k = j++)
		{
			const glm::vec3& va = vol->verts[k];
			const glm::vec3& vb = vol->verts[j];

			dd->vertex(vol->verts[0][0], vol->hmax, vol->verts[2][0], col);
			dd->vertex(vb[0], vol->hmax, vb[2], col);
			dd->vertex(va[0], vol->hmax, va[2], col);

			dd->vertex(va[0], vol->hmin, va[2], duDarkenCol(col));
			dd->vertex(va[0], vol->hmax, va[2], col);
			dd->vertex(vb[0], vol->hmax, vb[2], col);

			dd->vertex(va[0], vol->hmin, va[2], duDarkenCol(col));
			dd->vertex(vb[0], vol->hmax, vb[2], col);
			dd->vertex(vb[0], vol->hmin, vb[2], duDarkenCol(col));
		}
	}
	dd->end();

	dd->begin(DU_DRAW_LINES, 2.0f);
	for (const auto& vol : m_volumes)
	{
		uint32_t col = duIntToCol(static_cast<int>(vol->areaType), 220);
		size_t nverts = vol->verts.size();

		for (size_t j = 0, k = nverts - 1; j < nverts; k = j++)
		{
			const glm::vec3& va = vol->verts[k];
			const glm::vec3& vb = vol->verts[j];

			dd->vertex(va[0], vol->hmin, va[2], duDarkenCol(col));
			dd->vertex(vb[0], vol->hmin, vb[2], duDarkenCol(col));
			dd->vertex(va[0], vol->hmax, va[2], col);
			dd->vertex(vb[0], vol->hmax, vb[2], col);
			dd->vertex(va[0], vol->hmin, va[2], duDarkenCol(col));
			dd->vertex(va[0], vol->hmax, va[2], col);
		}
	}
	dd->end();

	dd->begin(DU_DRAW_POINTS, 3.0f);
	for (const auto& vol : m_volumes)
	{
		uint32_t col = duDarkenCol(duIntToCol(static_cast<int>(vol->areaType), 255));
		size_t nverts = vol->verts.size();

		for (size_t j = 0; j < nverts; ++j)
		{
			dd->vertex(vol->verts[j].x, vol->verts[j].y + 0.1f, vol->verts[j].z, col);
			dd->vertex(vol->verts[j].x, vol->hmin, vol->verts[j].z, col);
			dd->vertex(vol->verts[j].x, vol->hmax, vol->verts[j].z, col);
		}
	}
	dd->end();

	dd->depthMask(true);
}
#pragma endregion
