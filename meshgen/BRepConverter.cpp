//
// Created by dannu on 2/9/2026.
//

#include "meshgen/GeometryUtils.h"

#include "eqglib/eqg_terrain.h"
#include "spdlog/spdlog.h"
#include <glm/gtx/norm.hpp>
#include <clipper2/clipper.h>
#include "triangle.h"
#include "CDT.h"

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <numeric>
#include <set>
#include <nlohmann/detail/input/parser.hpp>

using Distance = float;
using Vec3 = glm::vec<3, Distance>;
using Vec2 = glm::vec<2, Distance>;

constexpr Distance EPSILON = glm::epsilon<Distance>();
constexpr Distance PLANE_THICKNESS = static_cast<Distance>(1e-6);

// Orthonormal basis for projecting 3D points onto a plane
struct PlaneBasis
{
	Vec3 origin;    // A point on the plane
	Vec3 u;         // First basis vector (tangent)
	Vec3 v;         // Second basis vector (bitangent)

	// Create basis from a plane
	static PlaneBasis fromPlane(const Plane& plane)
	{
		PlaneBasis basis;
		// Compute origin: closest point to world origin on the plane
		basis.origin = plane.distance * plane.normal;

		// Compute orthonormal basis vectors on the plane
		Vec3 up = glm::abs(plane.normal.y) < 0.9 ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
		basis.u = glm::normalize(glm::cross(plane.normal, up));
		basis.v = glm::cross(plane.normal, basis.u);
		return basis;
	}

	// Project 3D point to 2D coordinates in this basis
	[[nodiscard]] Vec2 project(const Vec3& point) const
	{
		Vec3 relative = point - origin;
		return {glm::dot(relative, u), glm::dot(relative, v)};
	}

	// Unproject 2D coordinates back to 3D point on the plane
	[[nodiscard]] Vec3 unproject(const Vec2& point) const
	{
		return origin + point.x * u + point.y * v;
	}
};

// BSP node for area-specific trees
struct Node
{
	Vec3 normal;
	Distance dist = 0.;
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
	Vec3 position;
	int halfEdge = -1; // one of the outgoing half edges for traversal
	int id = -1;

	Vertex() = default;
	explicit Vertex(const Vec3& position, int id)
		: position(position), id(id) {}
};

struct Face
{
	std::vector<Vec3> vertexes;
	std::vector<int> vertexIds;
	Plane plane;
	int id = -1;

	Face() = default;
	explicit Face(std::vector<Vec3> vertexes, const Plane& plane, int id)
		: vertexes(std::move(vertexes)), plane(plane), id(id) {}

	explicit Face(const Plane& plane, int id)
		: plane(plane), id(id) {}
};

struct BRep
{

	std::vector<Vertex> vertexes;
	std::vector<Face> faces;

    int addVertex(const Vec3& position)
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


#pragma region BSP Debugging Functions


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


#pragma region Volume Simplification


struct MergeFace
{
	std::vector<Vec3> vertexes;  // CCW order when viewed from normal direction
	Plane plane;
	bool valid = true;

	MergeFace() = default;
	MergeFace(std::vector<Vec3> verts, const Plane& p)
		: vertexes(std::move(verts)), plane(p) {}
};

struct Segment
{
	Vec3 start;
	Vec3 end;
};

Clipper2Lib::PathsD triangulate(const Clipper2Lib::PathsD& paths)
{
	Clipper2Lib::PathsD faces;

	if (!paths.empty())
	{
		CDT::Triangulation<Distance> cdt(
			CDT::VertexInsertionOrder::Auto,
			CDT::IntersectingConstraintEdges::TryResolve,
			1);

		// build edges because we need to account for holes and concavity
		std::vector<CDT::V2d<Distance>> vertexes;
		std::vector<CDT::Edge> edges;
		for (const auto& path : paths)
		{
			vertexes.reserve(vertexes.size() + path.size());
			for (const auto& point : path)
				vertexes.emplace_back(static_cast<Distance>(point.x), static_cast<Distance>(point.y));

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
			std::vector<Distance> points;
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

BRep groupFaces(const std::vector<MergeFace>& allFaces)
{
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

	std::map<std::tuple<int, int, int, int>, std::vector<MergeFace>> test;
	for (const auto& [key, faces] : groups)
	{
		PlaneBasis basis = PlaneBasis::fromPlane(faces[0].plane);
		test[key].reserve(faces.size());
		for (const auto& face : faces)
		{
			std::vector<Vec3> vertexes;
			vertexes.reserve(face.vertexes.size());
			for (const auto& vertex : face.vertexes)
			{
				auto projected = basis.project(vertex);
				auto unprojected = basis.unproject(projected);
				vertexes.emplace_back(unprojected);
			}

			test[key].emplace_back(vertexes, face.plane);
		}
	}

	std::map<std::tuple<int, int, int, int>, std::vector<MergeFace>> xoredFaces;
	for (const auto& [key, faces] : groups)
	{
		PlaneBasis basis = PlaneBasis::fromPlane(faces[0].plane);
		auto [x, y, z, d] = key;
		std::tuple<int, int, int, int> inverse = {-x, -y, -z, -d}; // TODO: might need to check +/- 1 for all of these
		if (groups.contains(inverse)) // can assume non-empty here
		{
			auto inverseFaces = groups[inverse];
			Clipper2Lib::PathsD inversePaths;
			inversePaths.reserve(inverseFaces.size());
			// the orientation of the face does not appear to matter to the Clipper2 boolean operations
			for (const auto& face : inverseFaces)
			{
				std::vector<Distance> inversePoints;
				inversePoints.reserve(face.vertexes.size() * 2);
				for (const auto& vertex : face.vertexes)
				{
					auto projected = basis.project(vertex);
					inversePoints.emplace_back(projected.x);
					inversePoints.emplace_back(projected.y);
				}

				inversePaths.emplace_back(Clipper2Lib::MakePathD(inversePoints));
			}

			Clipper2Lib::PathsD paths;
			paths.reserve(faces.size());
			for (const auto& face : faces)
			{
				std::vector<Distance> points;
				points.reserve(face.vertexes.size() * 2);
				for (const auto& vertex : face.vertexes)
				{
					auto projected = basis.project(vertex);
					points.emplace_back(projected.x);
					points.emplace_back(projected.y);
				}

				paths.emplace_back(Clipper2Lib::MakePathD(points));
			}

			auto diffed = Clipper2Lib::Difference(paths, inversePaths, Clipper2Lib::FillRule::NonZero);
			diffed = Clipper2Lib::Union(diffed, Clipper2Lib::FillRule::NonZero);
			diffed = Clipper2Lib::SimplifyPaths(diffed, 1);

			Clipper2Lib::PathsD solution = triangulate(diffed);
			//Clipper2Lib::Triangulate(diffed, 0, solution);
			solution = Clipper2Lib::SimplifyPaths(solution, 1);

			if (!solution.empty())
			{
				xoredFaces[key].reserve(solution.size());
				for (const auto& path : solution)
				{
					if (!path.empty())
					{
						MergeFace newFace;
						newFace.plane = faces[0].plane;
						newFace.vertexes.reserve(path.size());
						for (const auto& point : path)
							newFace.vertexes.push_back(basis.unproject({point.x, point.y}));

						xoredFaces[key].push_back(newFace);
					}
				}
			}
		}
		else if (!faces.empty())
		{
			Clipper2Lib::PathsD paths;
			paths.reserve(faces.size());
			for (const auto& face : faces)
			{
				std::vector<Distance> points;
				points.reserve(face.vertexes.size() * 2);
				for (const auto& vertex : face.vertexes)
				{
					auto projected = basis.project(vertex);
					points.emplace_back(projected.x);
					points.emplace_back(projected.y);
				}

				paths.emplace_back(Clipper2Lib::MakePathD(points));
			}

			auto unioned = Clipper2Lib::Union(paths, Clipper2Lib::FillRule::NonZero);
			unioned = Clipper2Lib::SimplifyPaths(unioned, 1);

			Clipper2Lib::PathsD solution = triangulate(unioned);
			// Clipper2Lib::Triangulate(unioned, 0, solution);
			solution = Clipper2Lib::SimplifyPaths(solution, 1);

			if (!solution.empty())
			{
				xoredFaces[key].reserve(solution.size());
				for (const auto& path : solution)
				{
					if (!path.empty())
					{
						MergeFace newFace;
						newFace.plane = faces[0].plane;
						newFace.vertexes.reserve(path.size());
						for (const auto& point : path)
							newFace.vertexes.push_back(basis.unproject({point.x, point.y}));

						xoredFaces[key].push_back(newFace);
					}
				}
			}
		}
	}

	BRep result;
	constexpr Distance VERTEX_MERGE_TOL = 1e-2f;
	std::vector<Vec3> vertexPositions;

	auto findOrAddVertex = [&result, &vertexPositions](const Vec3& pos) -> int
	{
		for (size_t i = 0; i < vertexPositions.size(); ++i)
			if (glm::length(vertexPositions[i] - pos) < VERTEX_MERGE_TOL)
				return static_cast<int>(i);
		vertexPositions.push_back(pos);
		result.addVertex(pos);
		return static_cast<int>(vertexPositions.size() - 1);
	};

	for (const auto& [key, faces] : xoredFaces)
	{
		for (const auto& face : faces)
		{
			std::vector<int> vertexIds;
			for (const auto& vertex : face.vertexes)
				vertexIds.push_back(findOrAddVertex(vertex));

			int faceId = result.addFace(face.plane);
			result.faces[faceId].vertexIds = std::move(vertexIds);
		}
	}

	return result;
}

#pragma endregion


#pragma region Polyhedra Union

class PolyhedraUnionConverter
{
public:
	static BRepResult convert(const std::vector<ConvexHullResult>& hulls);
};

// --- Main conversion ---

BRepResult PolyhedraUnionConverter::convert(const std::vector<ConvexHullResult>& hulls)
{
	BRepResult result;

	std::vector<MergeFace> allCellFaces;
	for (const auto& hull : hulls)
	{
		std::vector<Vec3> hullVertexes;
		hullVertexes.reserve(hull.vertices.size());
		for (const auto& vertex : hull.vertices)
			hullVertexes.emplace_back(vertex.x, vertex.y, vertex.z);

		for (const auto& face : hull.faces)
		{
			if (face.size() >= 3)
			{
				std::vector<Vec3> vertexes;
				vertexes.reserve(face.size());
				for (const auto& vertexIdx : face)
					vertexes.push_back(hullVertexes[vertexIdx]);

				// Use Newell's method to compute robust normal for arbitrary polygon
				Vec3 normal;
				Vec3 centroid;
				for (size_t i = 0; i < vertexes.size(); ++i)
				{
					const Vec3& current = vertexes[i];
					const Vec3& next = vertexes[(i + 1) % vertexes.size()];

					normal.x += (current.y - next.y) * (current.z + next.z);
					normal.y += (current.z - next.z) * (current.x + next.x);
					normal.z += (current.x - next.x) * (current.y + next.y);

					centroid += current;
				}

				Distance len = glm::length(normal);
				if (len > EPSILON)
				{
					Plane plane;
					plane.normal = normal / len;
					centroid /= static_cast<Distance>(vertexes.size());
					plane.distance = glm::dot(plane.normal, centroid);

					allCellFaces.emplace_back(vertexes, plane);
				}
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
			if (face.vertexIds.size() >= 3)
				result.faces.emplace_back(face.vertexIds.begin(), face.vertexIds.end());
	}
	catch (CDT::Error& e)
	{
		SPDLOG_ERROR(e.what());
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
		auto result = PolyhedraUnionConverter::convert(convexHulls);
		result.areaIndex = area;
		saveOBJ(result, fmt::format("test_{}.obj", area));

		results.push_back(std::move(result));
	}

	SPDLOG_INFO("Built {} BReps from WLD BSP tree", results.size());
	return results;
}


#pragma endregion
