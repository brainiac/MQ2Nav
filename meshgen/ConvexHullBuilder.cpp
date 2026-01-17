//
// ConvexHullBuilder.cpp
//

#include "pch.h"
#include "meshgen/ConvexHullBuilder.h"
#include "eqglib/eqg_terrain.h"

#include "glm/gtx/norm.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <unordered_set>

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
using Plane = PolyClipper::Plane<VA>;

#pragma optimize("t", on)

Polyhedron BuildPolyhedron(const glm::vec3& bmin, const glm::vec3& bmax, const std::vector<Plane>& planes)
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
void BuildConvexHull(const aabb& aabb, ConvexHullResult& result, const std::vector<Plane>& planes)
{
	Polyhedron poly = BuildPolyhedron(aabb.min, aabb.max, planes);

	result.vertices.reserve(poly.size());
	for (const auto& v : poly)
	{
		result.vertices.push_back(VA::get_triple(v.position));
	}
	std::vector<std::vector<int>> faces = PolyClipper::extractFaces(poly);

	// Build triangles directly from the polyhedron faces
	for (const auto& faceIndices : faces)
	{
		// Add edges
		for (size_t i = 0; i < faceIndices.size(); ++i)
		{
			uint16_t v0 = faceIndices[i];
			uint16_t v1 = faceIndices[(i + 1) % faceIndices.size()];
			auto edge = (v0 < v1) ? std::make_pair(v0, v1) : std::make_pair(v1, v0);

			result.edges.push_back(edge);
		}

		// Fan triangulate (double-sided)
		for (size_t i = 1; i + 1 < faceIndices.size(); ++i)
		{
			// Front
			result.indices.push_back((uint16_t)faceIndices[0]);
			result.indices.push_back((uint16_t)faceIndices[i]);
			result.indices.push_back((uint16_t)faceIndices[i + 1]);
			// Back
			result.indices.push_back((uint16_t)faceIndices[0]);
			result.indices.push_back((uint16_t)faceIndices[i + 1]);
			result.indices.push_back((uint16_t)faceIndices[i]);
		}
	}
}

#pragma optimize("", on)


void BuildConvexHull(const eqg::Terrain& terrain, std::vector<ConvexHullResult>& results, uint32_t regionIndex, const std::vector<Plane>& planes)
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
			
			if (!result.indices.empty())
			{
				results.push_back(std::move(result));
			}
			else
			{
				SPDLOG_WARN("Region {} has faces but no triangles built", regionIndex);
			}
		}
	}
}

void TraverseBSP(
	const eqg::Terrain& terrain,
	uint32_t nodeIndex,
	std::vector<Plane>& currentPlanes,
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

	Plane plane = { node.plane.dist, VA::Vector(node.plane.normal.x, node.plane.normal.y, node.plane.normal.z) };

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

	std::vector<Plane> currentPlanes;
	TraverseBSP(terrain, 0, currentPlanes, results);

	SPDLOG_INFO("Built {} convex hulls from WLD BSP tree", results.size());
	return results;
}
