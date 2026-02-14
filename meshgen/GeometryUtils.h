//
// GeometryUtils.h
//

#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <set>
#include <unordered_map>

#include "eqglib/eqg_terrain.h"

// Forward declarations
namespace eqg
{
	class Terrain;
}

// Result of building a convex hull from BSP planes
struct ConvexHullResult
{
	int regionIndex;

	std::vector<glm::vec3> vertices;
	std::vector<std::vector<uint16_t>> faces;  // Polygon faces (not triangulated)
};

// Simple plane representation
struct Plane
{
	glm::vec3 normal;
	float distance;

	Plane()
		: distance(0.0)
	{
	}

	Plane(const glm::vec3& normal, float distance)
		: normal(normal), distance(distance)
	{
	}

	bool operator==(const Plane& other) const
	{
		return normal == other.normal
			&& distance == other.distance;
	}
};


// Hash combine helper for building composite hash values
template <class T>
void hash_combine(std::size_t& seed, const T& v)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// Quantize a float to a grid for consistent hashing.
// Values within the grid size will snap to the same quantized value,
// ensuring that hash(a) == hash(b) when a and b are "close enough".
inline float QuantizeFloat(float f, float gridSize = 1e-4f)
{
	return std::round(f / gridSize) * gridSize;
}

inline glm::vec3 QuantizeVec3(const glm::vec3& v, float gridSize = 1e-4f)
{
	return { QuantizeFloat(v.x, gridSize), QuantizeFloat(v.y, gridSize), QuantizeFloat(v.z, gridSize) };
}

// Assign a unique color to each group of coincident faces.
// Faces on the same plane (regardless of normal direction) get the same color.
// This helps visualize which faces would be candidates for internal face removal.
std::vector<uint32_t> DebugColorFacesByPlane(const std::vector<glm::vec3>& vertices,
	const std::vector<std::array<uint16_t, 3>>& faces);

// Build all convex hulls for regions in areas
std::vector<ConvexHullResult> BuildConvexHullsFromRegions(const eqg::Terrain& terrain);

// Result of building a BRep from BSP planes
struct BRepResult
{
	int areaIndex = -1;

	std::vector<glm::vec3> vertexes;
	std::vector<std::array<uint16_t, 3>> faces;  // Polygon faces (triangulated)
	std::vector<std::array<uint16_t, 2>> outerEdges;
};

std::vector<BRepResult> convertBSPToBRepPolyhedraUnion(const eqg::Terrain& terrain);
