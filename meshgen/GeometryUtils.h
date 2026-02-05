//
// GeometryUtils.h
//

#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <set>
#include <unordered_map>

// Forward declarations
struct AreaVolumeComponent;

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
		: distance(0.0f)
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
	const std::vector<std::vector<uint16_t>>& faces);

// Build all convex hulls for regions in areas
std::vector<ConvexHullResult> BuildConvexHullsFromRegions(const eqg::Terrain& terrain);

// Result structure for a single area's BSP tree
struct AreaBSPTree
{
	// BSP node for area-specific trees
	struct Node
	{
		glm::vec3 normal;
		float dist = 0.f;
		uint32_t region;      // Non-zero for leaf nodes (1-based region index)
		uint32_t front = 0;       // Front child index (0 = none, 1-based otherwise)
		uint32_t back = 0;        // Back child index (0 = none, 1-based otherwise)
	};

	uint32_t areaNum;
	uint32_t rootNum;
	std::unordered_map<uint32_t, Node> nodes;
};

struct BRepVolume
{
	using Vec3 = glm::vec<3, double>;
	using Vec2 = glm::vec<2, double>;

	struct Face
	{
		std::vector<Vec3> vertexes;
		int planeId;
		int areaId = 0;
		bool valid = true;

		Face() = default;
		Face(std::vector<Vec3> v, int pid, int a = 0)
			: vertexes(std::move(v)), planeId(pid), areaId(a) {}
	};

	struct Segment
	{
		Vec2 start;
		Vec2 end;

		[[nodiscard]] Vec2 midpoint() const { return (start + end) * 0.5; }
		[[nodiscard]] bool isCoincident(const std::vector<Segment>& boundary) const;
		[[nodiscard]] bool isCollinearAndOverlapping(const Segment& other) const;
	};

	struct ClassifiedSegment : Segment
	{
		enum class Classification
		{
			INSIDE,
			OUTSIDE,
			COINCIDENT
		};

		Classification classification;
	};

	struct PlaneBasis
	{
		Vec3 origin; // point on the plane
		Vec3  u;     // first basis vector (tangent)
		Vec3  v;     // second basis vector (bitangent)

		[[nodiscard]] Vec2 project(const Vec3& point) const;
		[[nodiscard]] Vec3 unproject(const Vec2& point) const;
	};

	std::vector<Face> faces;
};

// Result of building a BRep from BSP planes
struct BRepResult
{
	int areaIndex;

	std::vector<glm::vec3> vertexes;
	std::vector<std::vector<uint16_t>> faces;  // Polygon faces (not triangulated)
};

// Extracts BSP trees for individual areas from the full zone BSP tree.
// Each area gets its own subtree containing only the nodes that can reach
// regions belonging to that area.
std::vector<BRepResult> BuildBRepsFromAreas(const eqg::Terrain& terrain);

