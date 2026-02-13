//
// GeometryUtils.cpp
//

#include "pch.h"
#include "meshgen/GeometryUtils.h"
#include "meshgen/EQComponents.h"
#include "eqglib/eqg_terrain.h"

#include "glm/gtx/norm.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <functional>
#include <random>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

//============================================================================================================
// Polygon Clipping for wld terrain regions (BSP Tree -> Convex Hull)
//============================================================================================================

#pragma region Polygon Clipping

// This is pretty intensive stuff, keep it optimized, even in debug
#pragma optimize("t", on)
#pragma warning(push)
#pragma warning(disable: 4267) // warning C4267: 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable: 4018) // warning C4018: '<': signed/unsigned mismatch
#include "PolyClipper/src/polyclipper3d.hh"
#pragma warning(pop)
#pragma optimize("", on)

struct GlmVectorAdapter
{
	using VECTOR = glm::vec<3, double, glm::defaultp>;
	static VECTOR Vector(double a, double b, double c) { return VECTOR(a, b, c); }
	static bool equal(const VECTOR& a, const VECTOR& b) { return a == b; }
	static double& x(VECTOR& a) { return a.x; }
	static double& y(VECTOR& a) { return a.y; }
	static double& z(VECTOR& a) { return a.z; }
	static double x(const VECTOR& a) { return a.x; }
	static double y(const VECTOR& a) { return a.y; }
	static double z(const VECTOR& a) { return a.z; }
	static double dot(const VECTOR& a, const VECTOR& b) { return glm::dot(a, b); }
	static VECTOR cross(const VECTOR& a, const VECTOR& b) { return glm::cross(a, b); }
	static double magnitude2(const VECTOR& a) { return glm::length2(a); }
	static double magnitude(const VECTOR& a) { return glm::length(a); }
	static VECTOR& imul(VECTOR& a, const double b) { a *= b; return a; }
	static VECTOR& idiv(VECTOR& a, const double b) { a /= b; return a; }
	static VECTOR& iadd(VECTOR& a, const VECTOR& b) { a += b; return a; }
	static VECTOR& isub(VECTOR& a, const VECTOR& b) { a -= b; return a; }
	static VECTOR mul(const VECTOR& a, const double b) { return a * b; }
	static VECTOR div(const VECTOR& a, const double b) { return a / b; }
	static VECTOR add(const VECTOR& a, const VECTOR& b) { return a + b; }
	static VECTOR sub(const VECTOR& a, const VECTOR& b) { return a - b; }
	static VECTOR neg(const VECTOR& a) { return -a; }
	static VECTOR unitVector(const VECTOR& a) { auto mag = magnitude(a); return mag > 1.0e-15 ? div(a, mag) : Vector(1.0f, 0.0f, 0.0f); }
	static std::string str(const VECTOR& a) { return fmt::format("({} {} {})", a.x, a.y, a.z); }
	static glm::vec3 get_triple(const VECTOR& a) { return a; }
	static void set_triple(VECTOR& a, const glm::vec3& vals) { a = vals; }
};

using VA = GlmVectorAdapter;
using Vector = VA::VECTOR;
using Polyhedron = std::vector<PolyClipper::Vertex3d<VA>>;
using PolyPlane = PolyClipper::Plane<VA>;

#pragma optimize("t", on)

Polyhedron BuildPolyhedron(const glm::vec3& bmin, const glm::vec3& bmax, const std::vector<PolyPlane>& planes)
{
	// 8 vertices of AABB
	Vector v5 = { bmax.x, bmin.y, bmax.z }; // 0
	Vector v1 = { bmax.x, bmin.y, bmin.z }; // 1
	Vector v0 = { bmin.x, bmin.y, bmin.z }; // 2
	Vector v3 = { bmin.x, bmax.y, bmin.z }; // 3
	Vector v6 = { bmax.x, bmax.y, bmax.z }; // 4
	Vector v2 = { bmax.x, bmax.y, bmin.z }; // 5
	Vector v4 = { bmin.x, bmin.y, bmax.z }; // 6
	Vector v7 = { bmin.x, bmax.y, bmax.z }; // 7

	std::vector points = {
		v0, v1, v2, v3, v4, v5, v6, v7
	};

	std::vector<std::vector<int>> neighbors = {
		{ 1, 4, 3 }, // 0
		{ 5, 0, 2 }, // 1
		{ 3, 6, 1 }, // 2
		{ 7, 2, 0 }, // 3
		{ 5, 7, 0 }, // 4
		{ 1, 6, 4 }, // 5
		{ 5, 2, 7 }, // 6
		{ 4, 6, 3 }  // 7
	};

	Polyhedron poly;

	PolyClipper::initializePolyhedron<GlmVectorAdapter>(poly, points, neighbors);
	PolyClipper::clipPolyhedron(poly, planes);

	return poly;
}

// Generate convex hull from zone aabox and set of planes
void BuildConvexHull(const aabb& aabb, ConvexHullResult& result, const std::vector<PolyPlane>& planes)
{
	Polyhedron poly = BuildPolyhedron(aabb.min, aabb.max, planes);

	result.vertices.reserve(poly.size());
	for (const auto& v : poly)
	{
		result.vertices.push_back(VA::get_triple(v.position));
	}

	// Extract faces as polygons (not triangulated)
	std::vector<std::vector<int>> faces = PolyClipper::extractFaces(poly);
	result.faces.reserve(faces.size());

	for (const auto& faceIndices : faces)
	{
		std::vector<uint16_t> face;
		face.reserve(faceIndices.size());
		for (int idx : faceIndices)
		{
			face.push_back(static_cast<uint16_t>(idx));
		}
		result.faces.push_back(std::move(face));
	}
}

#pragma optimize("", on)

void BuildConvexHull(const eqg::Terrain& terrain, std::vector<ConvexHullResult>& results, uint32_t regionIndex,
	const std::vector<PolyPlane>& planes)
{
	if (regionIndex < terrain.m_wldAreaEnvironments.size())
	{
		eqg::AreaEnvironment env = terrain.m_wldAreaEnvironments[regionIndex];

		// Skip regions with no special environment
		if (env.type != eqg::AreaEnvironment::Type_None || env.flags != eqg::AreaEnvironment::Flags_None)
		{
			auto& areaIndices = terrain.m_wldAreaIndices;
			auto& areaEnvs = terrain.m_wldAreaEnvironments;

			// Use region index to look up area index and environment
			if (regionIndex >= areaIndices.size() || regionIndex >= areaEnvs.size())
			{
				return;
			}

			// Skip regions with no special environment
			if (env.type == eqg::AreaEnvironment::Type_None
				&& env.flags == eqg::AreaEnvironment::Flags_None)
			{
				return;
			}

			ConvexHullResult result;
			result.regionIndex = regionIndex;

			BuildConvexHull(terrain.m_aabb, result, planes);

			if (!result.faces.empty())
			{
				results.push_back(std::move(result));
			}
		}
	}
}

void TraverseBSP(
	const eqg::Terrain& terrain,
	uint32_t nodeIndex,
	std::vector<PolyPlane>& currentPlanes,
	std::vector<ConvexHullResult>& outResult)
{
	auto& bspTree = *terrain.m_wldBspTree;

	if (nodeIndex >= bspTree.nodes.size())
		return;

	const auto& node = bspTree.nodes[nodeIndex];

	// Check if this is a leaf node (has a region reference)
	if (node.region != 0)
	{
		uint32_t regionIndex = node.region - 1;

		BuildConvexHull(terrain, outResult, regionIndex, currentPlanes);
		return;
	}

	PolyPlane plane = { node.plane.dist, VA::Vector(node.plane.normal.x, node.plane.normal.y, node.plane.normal.z) };

	if (node.front != 0)
	{
		currentPlanes.push_back(plane);
		TraverseBSP(terrain, node.front - 1, currentPlanes, outResult);
		currentPlanes.pop_back();
	}

	if (node.back != 0)
	{
		plane.normal = -plane.normal;
		plane.dist = -plane.dist;

		currentPlanes.push_back(plane);
		TraverseBSP(terrain, node.back - 1, currentPlanes, outResult);
		currentPlanes.pop_back();
	}
}

std::vector<ConvexHullResult> BuildConvexHullsFromRegions(const eqg::Terrain& terrain)
{
	std::vector<ConvexHullResult> results;

	if (!terrain.m_wldBspTree || terrain.m_wldBspTree->nodes.empty())
	{
		return results;
	}

	std::vector<PolyPlane> currentPlanes;
	TraverseBSP(terrain, 0, currentPlanes, results);

	SPDLOG_INFO("Built {} convex hulls from WLD BSP tree", results.size());
	return results;
}


#pragma endregion

//============================================================================================================
//============================================================================================================

template <>
struct std::hash<glm::vec3>
{
	size_t operator()(const glm::vec3& v) const noexcept
	{
		size_t h = 0;
		hash_combine(h, v.x);
		hash_combine(h, v.y);
		hash_combine(h, v.z);
		return h;
	}
};

Plane QuantizePlane(const Plane& p)
{
	return { QuantizeVec3(p.normal, 0.01f), QuantizeFloat(static_cast<float>(p.distance), 0.01f) };
}

// Create a Plane suitable for use as a key in an unordered map, both quantized and
// normalized towards origin.
Plane MakePlaneKey(const Plane& p)
{
	// If distance is positive (or zero), the plane faces away from origin - flip it
	if (Plane qp = QuantizePlane(p); qp.distance >= 0.0f)
	{
		return Plane{ -qp.normal, -qp.distance };
	}
	else
	{
		return Plane{ qp.normal, qp.distance };
	}
}

struct PlaneHasher
{
	size_t operator()(const Plane& k) const
	{
		size_t h = 0;
		hash_combine(h, k.normal.x);
		hash_combine(h, k.normal.y);
		hash_combine(h, k.normal.z);
		hash_combine(h, k.distance);
		return h;
	}
};

// Compute plane from a polygon face using Newell's method
Plane ComputeFacePlane(const std::vector<glm::vec3>& vertices, const std::vector<uint16_t>& face)
{
	Plane plane{ glm::vec3(0.0f), 0.0f };

	if (face.size() < 3)
		return plane;

	// Use Newell's method to compute robust normal for arbitrary polygon
	glm::vec3 normal(0.0f);
	glm::vec3 centroid(0.0f);

	for (size_t i = 0; i < face.size(); ++i)
	{
		const glm::vec3& current = vertices[face[i]];
		const glm::vec3& next = vertices[face[(i + 1) % face.size()]];

		normal.x += (current.y - next.y) * (current.z + next.z);
		normal.y += (current.z - next.z) * (current.x + next.x);
		normal.z += (current.x - next.x) * (current.y + next.y);

		centroid += current;
	}

	float len = glm::length(normal);
	if (len > 1e-6f)
	{
		plane.normal = normal / len;
		centroid /= static_cast<float>(face.size());
		plane.distance = glm::dot(VA::get_triple(plane.normal), centroid);
	}

	return plane;
}

uint32_t GetRandomColor(size_t hashVal)
{
	std::mt19937 rng(static_cast<uint32_t>(hashVal));

	// Define a distribution for an 8-bit color channel (0-255)
	std::uniform_int_distribution<int> dist(0, 255);

	// Generate random values for Red, Green, and Blue channels
	uint8_t r = static_cast<uint8_t>(dist(rng));
	uint8_t g = static_cast<uint8_t>(dist(rng));
	uint8_t b = static_cast<uint8_t>(dist(rng));

	// Combine into a single 32-bit integer (format: 0x00RRGGBB)
	return (static_cast<uint32_t>(r) << 0) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16)
		| (static_cast<uint32_t>(178) << 24);
}

std::vector<uint32_t> DebugColorFacesByPlane(const std::vector<glm::vec3>& vertices,
	const std::vector<std::vector<uint16_t>>& faces)
{
	std::vector<uint32_t> faceColors(faces.size(), 0xFFFFFFFF);  // Default white

	if (faces.empty() || vertices.empty())
		return faceColors;

	const size_t numFaces = faces.size();

	// Compute plane for each face
	std::vector<Plane> facePlanes;
	facePlanes.reserve(numFaces);

	for (size_t i = 0; i < numFaces; ++i)
	{
		facePlanes.emplace_back(ComputeFacePlane(vertices, faces[i]));
	}

	// Group faces by coplanar plane (normalized to face towards origin)
	std::unordered_map<Plane, std::vector<size_t>, PlaneHasher> planeGroups;
	for (size_t i = 0; i < numFaces; ++i)
	{
		planeGroups[MakePlaneKey(facePlanes[i])].push_back(i);
	}

	for (const auto& [plane, faceIndices] : planeGroups)
	{
		if (faceIndices.empty())
			continue;

		uint32_t color = GetRandomColor(PlaneHasher()(plane));

		for (size_t idx : faceIndices)
		{
			faceColors[idx] = color;
		}
	}

	return faceColors;
}