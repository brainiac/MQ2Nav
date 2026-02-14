//
// GeometryUtils.cpp
//

#include "pch.h"
#include "meshgen/GeometryUtils.h"
#include "meshgen/EQComponents.h"
#include "eqglib/eqg_terrain.h"

#include "CDT.h"
#include "clipper2/clipper.h"
#include "glm/gtx/norm.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <functional>
#include <random>
#include <ranges>
#include <unordered_map>
#include <unordered_set>


constexpr float EPSILON = glm::epsilon<float>();

// Orthonormal basis for projecting 3D points onto a plane
struct PlaneBasis
{
	glm::vec3 origin;    // A point on the plane
	glm::vec3 u;         // First basis vector (tangent)
	glm::vec3 v;         // Second basis vector (bitangent)

	// Create basis from a plane
	static PlaneBasis fromPlane(const Plane& plane)
	{
		PlaneBasis basis;
		// Compute origin: closest point to world origin on the plane
		basis.origin = plane.distance * plane.normal;

		// Compute orthonormal basis vectors on the plane
		glm::vec3 up = glm::abs(plane.normal.y) < 0.9 ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
		basis.u = glm::normalize(glm::cross(plane.normal, up));
		basis.v = glm::cross(plane.normal, basis.u);
		return basis;
	}

	// Project 3D point to 2D coordinates in this basis
	[[nodiscard]] glm::vec2 project(const glm::vec3& point) const
	{
		glm::vec3 relative = point - origin;
		return {glm::dot(relative, u), glm::dot(relative, v)};
	}

	// Unproject 2D coordinates back to 3D point on the plane
	[[nodiscard]] glm::vec3 unproject(const glm::vec2& point) const
	{
		return origin + point.x * u + point.y * v;
	}
};

// BSP node for area-specific trees
struct Node
{
	glm::vec3 normal;
	float dist = 0.;
	uint32_t region = 0;      // Non-zero for leaf nodes (1-based region index)
	uint32_t front = 0;       // Front child index (0 = none, 1-based otherwise)
	uint32_t back = 0;        // Back child index (0 = none, 1-based otherwise)

	[[nodiscard]] Plane plane() const { return {normal, dist}; }
};

// Result structure for a single area's BSP tree
struct AreaBSPTree
{
	eqg::AreaEnvironment::Type      type;
	eqg::AreaEnvironment::Flags     flags;
	uint32_t areaNum;
	uint32_t rootNum;
	std::unordered_map<uint32_t, Node> nodes;
};

struct Vertex
{
	glm::vec3 position;
	int id = -1;

	Vertex() = default;
	explicit Vertex(const glm::vec3& position, int id)
		: position(position), id(id) {}
};

struct Edge
{
	int start = -1;
	int end = -1;

	Edge() = default;
	explicit Edge(int start, int end)
		: start(start), end(end) {}
};

struct Face
{
	std::vector<int> vertexIds;
	Plane plane;
	int id = -1;

	Face() = default;
	explicit Face(const Plane& plane, int id)
		: plane(plane), id(id) {}
};

struct BRep
{

	std::vector<Vertex> vertexes;
	std::vector<Face> faces;
	std::vector<Edge> outerEdges;

    int addVertex(const glm::vec3& position)
	{
		const int id = static_cast<int>(vertexes.size());
		vertexes.emplace_back(position, id);
		return id;
	}

	int addFace(const Plane& plane)
	{
		const int id = static_cast<int>(faces.size());
		faces.emplace_back(plane, id);
		return id;
	}
};


#pragma region Polygon Clipping

//============================================================================================================
// Polygon Clipping for wld terrain regions (BSP Tree -> Convex Hull)
//============================================================================================================

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

#pragma region BSP Debugging Functions

//============================================================================================================
// Writes out BSP trees and OBJ files for debugging
//============================================================================================================

// Write a single node and recurse (pre-order: node, front, back)
static void writeNode(std::ostream& out, const Node& node, const AreaBSPTree& tree)
{
	if (node.region != 0)
	{
		out << "IN " << tree.areaNum << "\n";
		return;
	}

	// Internal node
	out << "PLANE "
		<< std::setprecision(9) << node.normal.x << " "
		<< std::setprecision(9) << node.normal.y << " "
		<< std::setprecision(9) << node.normal.z << " "
		<< std::setprecision(9) << node.dist << " "
		<< tree.areaNum << "\n";

	if (node.front != 0 && tree.nodes.find(node.front) != tree.nodes.end())
		writeNode(out, tree.nodes.at(node.front), tree);
	else
		out << "NULL\n";

	if (node.back != 0 && tree.nodes.find(node.back) != tree.nodes.end())
		writeNode(out, tree.nodes.at(node.back), tree);
	else
		out << "NULL\n";
}

bool saveBSP(const AreaBSPTree& tree, const std::string& filename)
{
	std::ofstream out(filename);
	if (!out)
		return false;

	out << "# BSP tree file\n";
	out << "BSP 1\n";

	if (tree.nodes.find(tree.rootNum + 1) == tree.nodes.end())
	{
		out << "NULL\n";
		return true;
	}

	writeNode(out, tree.nodes.at(tree.rootNum + 1), tree);
	return out.good();
}

bool saveOBJ(const BRepResult& brep, const std::string& filename)
{
	std::ofstream out(filename);
	if (!out)
		return false;

	out << std::fixed << std::setprecision(6);

	out << "# BRep exported to OBJ\n"
		<< "# Vertexes: " << brep.vertexes.size() << "\n"
		<< "# Faces: " << brep.faces.size() << "\n\n";

	for (const auto& v : brep.vertexes)
		out << "v " << v.x << " " << v.y << " " << v.z << "\n";

	out << "\n";

	for (const auto& verts : brep.faces)
	{
			if (verts.size() >= 3)
		{
			out << "f";
			for (const int v : verts)
				out << " " << (v + 1); // OBJ uses 1-based indexing
			out << "\n";
		}
	}

	return out.good();
}

// Extracts BSP trees for individual areas from the full zone BSP tree.
// Each area gets its own subtree containing only the nodes that can reach
// regions belonging to that area.
std::vector<AreaBSPTree> BuildAreaBSPTrees(const eqg::Terrain& terrain)
{
	std::vector<AreaBSPTree> areaTrees;

	if (!terrain.m_wldBspTree || terrain.m_wldBspTree->nodes.empty())
		return areaTrees;

	const auto& fullTree = terrain.m_wldBspTree->nodes;
	const auto& areas = terrain.m_wldAreas;

	if (areas.empty())
		return areaTrees;

	std::set<uint32_t> unusedRegions;
	for (uint32_t areaNum = 0; areaNum < areas.size(); ++areaNum)
	{
		if (areaNum < terrain.m_wldAreaIndices.size())
			for (uint32_t regionNum : areas[areaNum].regionNumbers)
				unusedRegions.insert(regionNum);
	}

	// For each area of contiguous environment type, extract a subtree
	std::vector<AreaBSPTree> areaBSPTrees;
	struct ExtractInfo
	{
		AreaBSPTree tree;
		eqg::AreaEnvironment::Type envType = eqg::AreaEnvironment::Type_None;
		eqg::AreaEnvironment::Flags envFlags = eqg::AreaEnvironment::Flags_None;

		explicit operator bool() const
		{
			return envType != eqg::AreaEnvironment::Type_None || envFlags != eqg::AreaEnvironment::Flags_None;
		}

		bool operator!() const
		{
			return envType == eqg::AreaEnvironment::Type_None && envFlags == eqg::AreaEnvironment::Flags_None;
		}
	};

	// Recursive function to determine if a subtree contains any regions
	// belonging to this area, and if so, copy the relevant nodes.
	std::function<ExtractInfo(uint32_t, ExtractInfo)> extractSubtree = [&](uint32_t nodeNum, ExtractInfo info) -> ExtractInfo
	{
		if (nodeNum > 0 && nodeNum <= fullTree.size())
		{
			const auto& [plane, region, front, back] = fullTree[nodeNum - 1];

			// check if this is a leaf node (region != 0)
			if (region != 0)
			{
				if (unusedRegions.contains(region - 1))
				{
					const eqg::AreaEnvironment& env = terrain.m_wldAreaEnvironments[region - 1];

					// skip any region that has no environment info
					if (env.type == eqg::AreaEnvironment::Type_None && env.flags == eqg::AreaEnvironment::Flags_None)
					{
						unusedRegions.erase(region - 1);
						// return an empty struct so that we know this wasn't a valid branch
						return {};
					}

					if (!info)
					{
						// not currently in an environment, create a new one and recurse up with the new env set
						unusedRegions.erase(region - 1);
						Node newNode {{}, 0., region, 0, 0 };
						info.tree.nodes[nodeNum] = newNode;

						// set the env in the return
						info.tree.areaNum = terrain.m_wldAreaIndices[region - 1];
						info.envType = env.type;
						info.envFlags = env.flags;

						return info;
					}

					if (info.envType == env.type && info.envFlags == env.flags)
					{
						// we have environment info that matches this region's info, so add this node
						unusedRegions.erase(region - 1);
						Node newNode { {}, 0., region, 0, 0 };
						info.tree.nodes[nodeNum] = newNode;

						return info;
					}

					// otherwise, this region doesn't match, so it needs to be saved for later
				}

				return {};
			}

			// need to check front first, then pass that result into back if it's non-empty
			ExtractInfo frontResult = extractSubtree(front, info);
			ExtractInfo backResult = extractSubtree(back, frontResult ? frontResult : info);

			// if neither child has relevant regions, skip this node
			if (!frontResult && !backResult)
				return {};

			// at least one child has relevant regions - copy this node
			Node newNode { plane.normal, plane.dist, 0, 0, 0 };
			if (frontResult) newNode.front = front;
			if (backResult) newNode.back = back;

			// if we have a back result, that means that either there was a front result
			// and it was passed into it, or there wasn't and the front result was empty
			if (backResult)
			{
				backResult.tree.nodes[nodeNum] = newNode;
				return backResult;
			}

			// we must have a front result at this point, so it was the only one that
			// returned anything
			if (frontResult)
			{
				frontResult.tree.nodes[nodeNum] = newNode;
				return frontResult;
			}
		}

		return {};
	};

	while (!unusedRegions.empty())
	{
		if (ExtractInfo info = extractSubtree(1, {}))
		{
			if (!info.tree.nodes.empty())
			{
				SPDLOG_DEBUG("Built BSP for env {}:{} with {} nodes",
					static_cast<int>(info.envType), static_cast<int>(info.envFlags), info.tree.nodes.size());
				areaTrees.push_back(std::move(info.tree));
			}
			else
				SPDLOG_WARN("Traversed BSP with no new areas");
		}
	}

	SPDLOG_INFO("Built {} area BSP trees from zone BSP tree", areaTrees.size());
	return areaTrees;
}

#pragma endregion

//============================================================================================================
//============================================================================================================

#pragma region Polygon Simplification and Triangulation

//============================================================================================================
// Polygon Simplification for Convex Hulls -> Triangulated BRep
//============================================================================================================

struct MergeFace
{
	std::vector<glm::vec3> vertexes;  // CCW order when viewed from normal direction
	Plane plane;
	bool valid = true;

	MergeFace() = default;
	MergeFace(std::vector<glm::vec3> verts, const Plane& p)
		: vertexes(std::move(verts)), plane(p) {}
};

struct Segment
{
	glm::vec3 start;
	glm::vec3 end;
};

Clipper2Lib::PathsD triangulate(const Clipper2Lib::PathsD& paths)
{
	Clipper2Lib::PathsD faces;

	if (!paths.empty())
	{
		CDT::Triangulation<float> cdt(
			CDT::VertexInsertionOrder::Auto,
			CDT::IntersectingConstraintEdges::TryResolve,
			1);

		// build edges because we need to account for holes and concavity
		std::vector<CDT::V2d<float>> vertexes;
		std::vector<CDT::Edge> edges;
		for (const auto& path : paths)
		{
			vertexes.reserve(vertexes.size() + path.size());
			for (const auto& point : path)
				vertexes.emplace_back(static_cast<float>(point.x), static_cast<float>(point.y));

			auto edgesOffset = static_cast<CDT::VertInd>(edges.size());
			edges.reserve(edgesOffset + path.size());
			for (CDT::VertInd i = 0; i < static_cast<CDT::VertInd>(path.size()); ++i)
				edges.emplace_back(edgesOffset + i, edgesOffset + (i + 1) % static_cast<CDT::VertInd>(path.size()));
		}

		CDT::RemoveDuplicatesAndRemapEdges(vertexes, edges);
		cdt.insertVertices(vertexes);
		cdt.insertEdges(edges);

		cdt.eraseOuterTrianglesAndHoles();

		for (const auto& triangle : cdt.triangles)
		{
			std::vector<float> points;
			points.reserve(triangle.vertices.size() * 2);
			for (const auto& vertex : triangle.vertices)
			{
				points.emplace_back(cdt.vertices[vertex].x);
				points.emplace_back(cdt.vertices[vertex].y);
			}

			faces.push_back(Clipper2Lib::MakePathD(points));
		}
	}

	return faces;
}

Clipper2Lib::PathsD extractPaths(const std::vector<MergeFace>& faces, const PlaneBasis& basis)
{
	Clipper2Lib::PathsD paths;
	paths.reserve(faces.size());
	for (const auto& face : faces)
	{
		std::vector<float> points;
		points.reserve(face.vertexes.size() * 2);
		for (const auto& vertex : face.vertexes)
		{
			auto projected = basis.project(vertex);
			points.emplace_back(projected.x);
			points.emplace_back(projected.y);
		}

		paths.emplace_back(Clipper2Lib::MakePathD(points));
	}

	return paths;
}

int findOrAddVertex(BRep& result, std::vector<glm::vec3>& vertexPositions, const glm::vec3& pos)
{
	constexpr float VERTEX_MERGE_TOL = 1e-2f;

	for (size_t i = 0; i < vertexPositions.size(); ++i)
		if (glm::length(vertexPositions[i] - pos) < VERTEX_MERGE_TOL)
			return static_cast<int>(i);
	vertexPositions.push_back(pos);
	result.addVertex(pos);
	return static_cast<int>(vertexPositions.size() - 1);
}

void addFacesAndEdges(BRep& result, std::vector<glm::vec3>& vertexPositions, const Clipper2Lib::PathsD& paths, const Plane& plane, const PlaneBasis& basis)
{
	if (!paths.empty())
	{
		auto simplified = Clipper2Lib::SimplifyPaths(paths, 1);
		auto triangulated = triangulate(simplified);

		for (const auto& path : triangulated)
		{
			if (!path.empty())
			{
				std::vector<int> vertexIds;
				vertexIds.reserve(path.size());
				for (const auto& point : path)
					vertexIds.push_back(findOrAddVertex(result, vertexPositions, basis.unproject({point.x, point.y})));

				result.faces[result.addFace(plane)].vertexIds = std::move(vertexIds);
			}
		}

		for (const auto& path : simplified)
		{
			if (!path.empty())
			{
				auto trimmedPath = Clipper2Lib::TrimCollinear(path, 1);
				std::vector<int> vertexIds;
				vertexIds.reserve(trimmedPath.size());
				for (const auto& point : trimmedPath)
					vertexIds.push_back(findOrAddVertex(result, vertexPositions, basis.unproject({point.x, point.y})));

				for (size_t i = 0; i < vertexIds.size(); ++i)
					result.outerEdges.emplace_back(vertexIds[i], vertexIds[(i + 1) % vertexIds.size()]);
			}
		}
	}
}

BRep groupFaces(const std::vector<MergeFace>& allFaces)
{
	BRep result;
	std::vector<glm::vec3> vertexPositions;

	// Group faces by coplanar plane (rounded normal + distance)
	auto planeKey = [](const Plane& p) {
		return std::make_tuple(
			static_cast<int>(std::round(p.normal.x * 10)),
			static_cast<int>(std::round(p.normal.y * 10)),
			static_cast<int>(std::round(p.normal.z * 10)),
			static_cast<int>(std::round(p.distance * 1)));
	};

	std::map<std::tuple<int, int, int, int>, std::vector<MergeFace>> groups;
	for (auto& face : allFaces)
		if (face.valid)
			groups[planeKey(face.plane)].push_back(face);

	for (const auto& [key, faces] : groups)
	{
		PlaneBasis basis = PlaneBasis::fromPlane(faces[0].plane);
		auto [x, y, z, d] = key;
		std::tuple<int, int, int, int> inverse = {-x, -y, -z, -d};
		if (groups.contains(inverse) && !faces.empty() && !groups[inverse].empty())
		{
			// the orientation of the face does not appear to matter to the Clipper2 boolean operations
			Clipper2Lib::PathsD inversePaths = extractPaths(groups[inverse], basis);
			Clipper2Lib::PathsD paths = extractPaths(faces, basis);;

			auto diffed = Clipper2Lib::Difference(paths, inversePaths, Clipper2Lib::FillRule::NonZero);
			diffed = Clipper2Lib::Union(diffed, Clipper2Lib::FillRule::NonZero);

			addFacesAndEdges(result, vertexPositions, diffed, faces[0].plane, basis);
		}
		else if (!faces.empty())
		{
			Clipper2Lib::PathsD paths = extractPaths(faces, basis);
			auto unioned = Clipper2Lib::Union(paths, Clipper2Lib::FillRule::NonZero);
			addFacesAndEdges(result, vertexPositions, unioned, faces[0].plane, basis);
		}
	}

	return result;
}

#pragma endregion


//============================================================================================================
//============================================================================================================

template <typename T> Plane ComputeFacePlane(const std::vector<glm::vec3>& vertices, const T& face);
BRepResult convert(const std::vector<ConvexHullResult>& hulls)
{
	BRepResult result;

	std::vector<MergeFace> allCellFaces;
	for (const auto& hull : hulls)
	{
		for (const auto& face : hull.faces)
		{
			auto plane = ComputeFacePlane(hull.vertices, face);
			if (glm::abs(plane.normal.x) > EPSILON ||
				glm::abs(plane.normal.y) > EPSILON ||
				glm::abs(plane.normal.z) > EPSILON)
			{
				std::vector<glm::vec3> vertexes;
				vertexes.reserve(face.size());
				for (const auto& vertexIdx : face)
					vertexes.push_back(hull.vertices[vertexIdx]);

				allCellFaces.emplace_back(std::move(vertexes), plane);
			}
		}
	}

	try
	{
		BRep volume = groupFaces(allCellFaces);

		// convert volume to result
		std::transform(volume.vertexes.begin(), volume.vertexes.end(), std::back_inserter(result.vertexes),
			[](const Vertex& vert) { return vert.position; });

		for (const auto& face : volume.faces)
			if (face.vertexIds.size() >= 3) // discard any vertexes above 3, triangulation failed? anything less isn't a face
				result.faces.emplace_back(std::array{
					static_cast<uint16_t>(face.vertexIds[0]),
					static_cast<uint16_t>(face.vertexIds[1]),
					static_cast<uint16_t>(face.vertexIds[2])});

		for (const auto& edge : volume.outerEdges)
			result.outerEdges.emplace_back(std::array{
				static_cast<uint16_t>(edge.start),
				static_cast<uint16_t>(edge.end),
			});
	}
	catch (CDT::Error& e)
	{
		SPDLOG_ERROR("Error parsing area volume: {}", e.what());
	}

	return result;
}

std::vector<BRepResult> convertBSPToBRepPolyhedraUnion(const eqg::Terrain& terrain)
{
	std::vector<BRepResult> results;

	if (!terrain.m_wldBspTree || terrain.m_wldBspTree->nodes.empty())
		return results;

	auto hulls = BuildConvexHullsFromRegions(terrain);
	std::map<uint32_t, std::vector<ConvexHullResult>> convexHulls;
	std::map<std::pair<eqg::AreaEnvironment::Type, eqg::AreaEnvironment::Flags>, uint32_t> areaTypes;
	for (const auto& hull : hulls)
	{
		auto area = terrain.m_wldAreaIndices[hull.regionIndex];
		auto env = terrain.m_wldAreaEnvironments[hull.regionIndex];

		auto [it, inserted] = areaTypes.try_emplace({env.type, env.flags}, area);
		convexHulls[it->second].emplace_back(hull);
	}

	for (const auto& [area, convexHulls] : convexHulls)
	{
		auto result = convert(convexHulls);
		result.areaIndex = static_cast<int>(area);
		saveOBJ(result, fmt::format("test_{}.obj", area));

		results.push_back(std::move(result));
	}

	SPDLOG_INFO("Built {} BReps from WLD BSP tree", results.size());
	return results;
}

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
template <typename T>
Plane ComputeFacePlane(const std::vector<glm::vec3>& vertices, const T& face)
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
	const std::vector<std::array<uint16_t, 3>>& faces)
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