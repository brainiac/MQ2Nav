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

#pragma region BRep Merging


constexpr double PLANE_THICKNESS = 1e-6;

// Extracts BSP trees for individual areas from the full zone BSP tree.
// Each area gets its own subtree containing only the nodes that can reach
// regions belonging to that area.
std::unordered_map<uint32_t, AreaBSPTree> BuildAreaBSPTrees(const eqg::Terrain& terrain)
{
	std::unordered_map<uint32_t, AreaBSPTree> areaTrees;

	if (!terrain.m_wldBspTree || terrain.m_wldBspTree->nodes.empty())
		return areaTrees;

	const auto& fullTree = terrain.m_wldBspTree->nodes;
	const auto& areas = terrain.m_wldAreas;

	if (areas.empty())
		return areaTrees;

	// Build a set of regions for each area for fast lookup
	std::unordered_map<uint32_t, std::set<uint32_t>> areaRegionSets;
	for (uint32_t areaNum = 0; areaNum < areas.size(); ++areaNum)
	{
		if (areaNum < terrain.m_wldAreaIndices.size())
		{
			const eqg::AreaEnvironment& env = terrain.m_wldAreaEnvironmentsPerArea[areaNum];
			// Skip regions with no special environment
			if (env.type != eqg::AreaEnvironment::Type_None || env.flags != eqg::AreaEnvironment::Flags_None)
				for (uint32_t regionNum : areas[areaNum].regionNumbers)
					areaRegionSets[areaNum].insert(regionNum);
		}
	}

	// For each area, extract a subtree
	for (auto [areaNum, regionSet] : areaRegionSets)
	{
		AreaBSPTree areaTree;
		areaTree.areaNum = areaNum;

		// Recursive function to determine if a subtree contains any regions
		// belonging to this area, and if so, copy the relevant nodes.
		// Returns the new node index (1-based) if the subtree was included, 0 otherwise.
		std::function<uint32_t(uint32_t)> extractSubtree;
		extractSubtree = [&](uint32_t nodeNum) -> uint32_t
		{
			if (nodeNum > 0 && nodeNum <= fullTree.size())
			{
				const auto& [plane, region, front, back] = fullTree[nodeNum - 1];

				// Check if this is a leaf node (region != 0)
				if (region != 0)
				{
					// Leaf node - check if region belongs to this area
					if (regionSet.contains(region - 1))
					{
						// Copy this leaf node
						AreaBSPTree::Node newNode;
						newNode.region = region - 1;
						newNode.front = 0;
						newNode.back = 0;
						areaTree.nodes[nodeNum] = std::move(newNode);

						return nodeNum;
					}

					return 0;
				}

				// Internal node - recurse to children
				uint32_t frontResult = extractSubtree(front);
				uint32_t backResult = extractSubtree(back);

				// If neither child has relevant regions, skip this node
				if (frontResult == 0 && backResult == 0)
					return 0;

				// At least one child has relevant regions - copy this node
				AreaBSPTree::Node newNode;
				newNode.normal = plane.normal;
				newNode.dist = plane.dist;
				newNode.region = 0;
				newNode.front = frontResult;
				newNode.back = backResult;
				areaTree.nodes[nodeNum] = std::move(newNode);

				return nodeNum;
			}

			return 0;
		};

		// Start extraction from root (index 1, which is nodes[0])
		areaTree.rootNum = extractSubtree(1);
		if (!areaTree.nodes.empty())
		{
			SPDLOG_DEBUG("Built BSP tree for area {} with {} nodes (from {} regions)",
			             areaNum, areaTree.nodes.size(), regionSet.size());
			areaTrees[areaNum] = std::move(areaTree);
		}
	}

	SPDLOG_INFO("Built {} area BSP trees from zone BSP tree", areaTrees.size());
	return areaTrees;
}

bool BRepVolume::Segment::isCoincident(const std::vector<Segment>& boundary) const
{
	return std::any_of(boundary.begin(), boundary.end(),
		[this](const Segment& segment) { return isCollinearAndOverlapping(segment); });
}

bool BRepVolume::Segment::isCollinearAndOverlapping(const Segment& other) const
{
	auto cross = [](const Vec2& a, const Vec2& b) { return a.x * b.y - a.y * b.x; };
	Vec2 dA = end - start;

	// check collinearity: directions parallel and other.start on a line through this
	if (glm::abs(cross(dA, other.end - other.start)) > PLANE_THICKNESS ||
		glm::abs(cross(dA, start - start)) > PLANE_THICKNESS)
		return false;

	double lenA = glm::length(dA);
	if (lenA < glm::epsilon<double>())
		return false;

	Vec2 dir = dA / lenA;
	double bStart = glm::dot(other.start - start, dir);
	double bEnd = glm::dot(other.end - start, dir);
	if (bStart > bEnd)
		std::swap(bStart, bEnd);

	return glm::min(lenA, bEnd) > glm::max(0., bStart) + glm::epsilon<double>();
}

bool isPointInsidePolygon(
	const BRepVolume::Vec2& point,
	const std::vector<BRepVolume::Segment>& boundary)
{
	using Vec2 = BRepVolume::Vec2;
	// ray casting algorithm: count intersections with ray going in +x direction
	// using "count lower vertex" rule for consistent vertex handling
	// the direction is completely arbitrary, any direction works
	int intersections = 0;
	for (const auto& seg : boundary)
	{
		const Vec2& a = seg.start;
		const Vec2& b = seg.end;
		double minY = glm::min(a.y, b.y);
		double maxY = glm::max(a.y, b.y);

		if (point.y >= minY && point.y <= maxY &&
			glm::abs(b.y - a.y) > glm::epsilon<double>()) // skip horizontal segments
		{
			double t = (point.y - a.y) / (b.y - a.y);
			double xIntersect = a.x + t * (b.x - a.x);

			// count only if crossing upward and intersection is to the right
			if (xIntersect > point.x + glm::epsilon<double>() && b.y > a.y)
				++intersections;
		}
	}

	return intersections % 2 == 1;
}

BRepVolume::PlaneBasis basisFromPlane(const PolyPlane& plane)
{
	using PlaneBasis = BRepVolume::PlaneBasis;
	PlaneBasis basis;

	// compute origin: closest point to the world origin on the plane (really any point will do)
	basis.origin = -plane.dist * plane.normal;

	// compute orthonormal basis vectors on the plane
	VA::VECTOR up = glm::abs(plane.normal.y) < 0.9 ? VA::VECTOR(0, 1, 0) : VA::VECTOR(1, 0, 0);
	basis.u = glm::normalize(glm::cross(plane.normal, up));
	basis.v = glm::cross(plane.normal, basis.u);

	return basis;
}

BRepVolume::Vec2 BRepVolume::PlaneBasis::project(const Vec3& point) const
{
	Vec3 relative = point - origin;
	return Vec2(glm::dot(relative, u), glm::dot(relative, v));
}

BRepVolume::Vec3 BRepVolume::PlaneBasis::unproject(const Vec2& point) const
{
	return origin + point.x * u + point.y * v;
}

std::vector<BRepVolume::Segment> projectFaceToSegments(const BRepVolume::Face& face, const BRepVolume::PlaneBasis& basis)
{
	std::vector<BRepVolume::Segment> segments;
	segments.reserve(face.vertexes.size());
	for (size_t i = 0; i < face.vertexes.size(); ++i)
		segments.emplace_back(basis.project(face.vertexes[i]),
		                      basis.project(face.vertexes[(i + 1) % face.vertexes.size()]));

	return segments;
}

std::vector<BRepVolume::Segment> removeCancellingEdges(std::vector<BRepVolume::Segment> segments)
{
	for (size_t i = 0; i < segments.size(); ++i)
	{
		for (size_t j = i + 1; j < segments.size(); ++j)
		{
			if (glm::length2(segments[i].start -  segments[j].end) < PLANE_THICKNESS * PLANE_THICKNESS &&
				glm::length2(segments[i].end - segments[j].start) < PLANE_THICKNESS * PLANE_THICKNESS)
			{
				segments[i] = segments.back();
				segments.pop_back();
				if (j < segments.size())
				{
					segments[j] = segments.back();
					segments.pop_back();
				}
				--i;
				break;
			}
		}
	}

	return segments;
}

std::optional<double> segmentIntersection(
	const BRepVolume::Vec2& a1, const BRepVolume::Vec2& a2,
	const BRepVolume::Vec2& b1, const BRepVolume::Vec2& b2)
{
	BRepVolume::Vec2 da = a2 - a1;
	BRepVolume::Vec2 db = b2 - b1;
	BRepVolume::Vec2 dc = b1 - a1;

	double cross = da.x * db.y - da.y * db.x;
	if (glm::abs(cross) < glm::epsilon<double>())
		return {};

	double t = (dc.x * db.y - dc.y * db.x) / cross;
	double s = (dc.x * da.y - dc.y * da.x) / cross;
	// t is strictly interior to a, s is anywhere on b
	if (t > glm::epsilon<double>() && t < 1. - glm::epsilon<double>() &&
		s >= -glm::epsilon<double>() && s <= 1. + glm::epsilon<double>())
		return t;

	return {};
}

std::vector<BRepVolume::Segment> splitSegmentAtBoundary(
	const BRepVolume::Segment& seg,
	const std::vector<BRepVolume::Segment>& boundary)
{
	std::vector<double> params = { 0., 1. };
	for (const auto& bSeg : boundary)
		if (auto t = segmentIntersection(seg.start, seg.end, bSeg.start, bSeg.end))
			params.push_back(*t);

	std::sort(params.begin(), params.end());
	params.erase(std::unique(params.begin(), params.end(),
		[](double a, double b) { return glm::abs(a - b) < glm::epsilon<double>(); }),
		params.end());

	std::vector<BRepVolume::Segment> result;
	BRepVolume::Vec2 dir = seg.end - seg.start;
	for (size_t i = 0; i + 1 < params.size(); ++i)
		if (params[i + 1] - params[i] > glm::epsilon<double>())
			result.emplace_back(seg.start + params[i] * dir, seg.start + params[i + 1] * dir);

	return result;
}

std::vector<BRepVolume::ClassifiedSegment> classifySegments(
	const std::vector<BRepVolume::Segment>& segments,
	const std::vector<BRepVolume::Segment>& boundary)
{
	using Seg = BRepVolume::ClassifiedSegment;
	using Cls = BRepVolume::ClassifiedSegment::Classification;

	std::vector<Seg> result;
	for (const auto& seg : segments)
	{
		for (const auto& subSeg : splitSegmentAtBoundary(seg, boundary))
		{
			if (subSeg.isCoincident(boundary))
				result.emplace_back(subSeg, Cls::COINCIDENT);
			else if (isPointInsidePolygon(subSeg.midpoint(), boundary))
				result.emplace_back(subSeg, Cls::INSIDE);
			else
				result.emplace_back(subSeg, Cls::OUTSIDE);
		}
	}

	return result;
}

// find all disjoint loops from a set of segments
std::vector<std::vector<BRepVolume::Vec2>> findAllLoops(const std::vector<BRepVolume::Segment>& segments)
{
	using Vec2 = BRepVolume::Vec2;
	std::vector<std::vector<Vec2>> loops;
	if (segments.empty())
		return loops;

	std::vector<bool> used(segments.size(), false);
	auto connects = [](const Vec2& a, const Vec2& b) { return glm::length2(a - b) < PLANE_THICKNESS * PLANE_THICKNESS; };

	for (auto it = std::find(used.begin(), used.end(), false);
		it != used.end(); it = std::find(it + 1, used.end(), false))
	{
		size_t startIdx = std::distance(used.begin(), it);

		std::vector<Vec2> loop { segments[startIdx].start, segments[startIdx].end };
		used[startIdx] = true;

		bool found = true;
		while (found && !connects(loop.back(), loop.front()))
		{
			found = false;
			for (size_t i = 0; !found && i < segments.size(); ++i)
			{
				if (!used[i])
				{
					const auto& seg = segments[i];
					if (connects(seg.start, loop.back()))
					{
						loop.push_back(seg.end);
						used[i] = true;
						found = true;
					}
					else if (connects(seg.end, loop.back()))
					{
						loop.push_back(seg.start);
						used[i] = true;
						found = true;
					}
				}
			}
		}

		if (connects(loop.front(), loop.back())) loop.pop_back();
		if (loop.size() >= 3) loops.push_back(std::move(loop));
	}

	return loops;
}

std::vector<BRepVolume::Face> rebuildFacesFromSegments(
	const std::vector<BRepVolume::Segment>& segments,
	const BRepVolume::Face& ref,
	const BRepVolume::PlaneBasis& basis)
{
	std::vector<BRepVolume::Face> faces;
	for (const auto& loop : findAllLoops(segments))
	{
		if (loop.size() >= 3)
		{
			std::vector<BRepVolume::Vec3> vertexes;
			vertexes.reserve(loop.size());
			for (const auto& p2d : loop)
				vertexes.push_back(basis.unproject(p2d));

			faces.emplace_back(std::move(vertexes), ref.planeId, ref.areaId);
		}
	}

	return faces;
}

// compute difference: primary OUTSIDE + secondary INSIDE (excluding coincident overlaps)
// ie, this is primary - secondary
std::vector<BRepVolume::Segment> computeDifference(
	const std::vector<BRepVolume::ClassifiedSegment>& primary,
	const std::vector<BRepVolume::ClassifiedSegment>& secondary)
{
	using Seg = BRepVolume::Segment;
	using CSeg = BRepVolume::ClassifiedSegment;

	auto isCoincident = [](const Seg& seg, const std::vector<CSeg>& segs)
	{
		return std::any_of(segs.begin(), segs.end(), [&seg](const CSeg& cs)
		{
			return cs.classification == CSeg::Classification::COINCIDENT &&
				seg.isCollinearAndOverlapping(cs);
		});
	};

	std::vector<Seg> result;
	for (const auto& cs : primary)
		if (cs.classification == CSeg::Classification::OUTSIDE)
			result.push_back(cs);

	for (const auto& cs : secondary)
		if (cs.classification == CSeg::Classification::INSIDE && !isCoincident(cs, primary))
			result.push_back(cs);

	return result;
}

BRepVolume mergeAlongHyperplane(
	const BRepVolume& a,
	const BRepVolume& b,
	const PolyPlane& plane)
{
	using Face = BRepVolume::Face;
	using Segment = BRepVolume::Segment;

	BRepVolume result;
	std::vector<const Face*> aOnPlane, bOnPlane;

    // Partition faces: copy non-plane faces to result, collect on-plane faces
	auto partitionFaces = [&](const BRepVolume& brep, std::vector<const Face*>& onPlane)
	{
		for (const auto& face : brep.faces)
			if (face.valid)
			{
				if (face.planeId == plane.ID) onPlane.push_back(&face);
				else result.faces.push_back(face);
			}
	};

	partitionFaces(a, aOnPlane);
	partitionFaces(b, bOnPlane);

	// if both sides have faces on the plane, perform the 2D merge
	if (!aOnPlane.empty() && !bOnPlane.empty())
	{
		BRepVolume::PlaneBasis basis = basisFromPlane(plane);

		// project faces to 2D segments
		auto projectAll = [&basis](const std::vector<const Face*>& faces)
		{
			std::vector<Segment> segments;
			for (const auto* face : faces)
			{
				auto segs = projectFaceToSegments(*face, basis);
				segments.insert(segments.end(), segs.begin(), segs.end());
			}

			return removeCancellingEdges(std::move(segments));
		};

		auto aSegments = projectAll(aOnPlane);
		auto bSegments = projectAll(bOnPlane);

		auto aClassified = classifySegments(aSegments, bSegments);
		auto bClassified = classifySegments(bSegments, aSegments);

		// rebuild faces from differences A-B and B-A
		auto rebuildAndAdd = [&basis, &result](const std::vector<Segment>& segs, const Face* ref)
		{
			for (auto& face : rebuildFacesFromSegments(segs, *ref, basis))
				result.faces.push_back(std::move(face));
		};

		auto aMinusB = computeDifference(aClassified, bClassified);
		auto bMinusA = computeDifference(bClassified, aClassified);

		if (!aMinusB.empty()) rebuildAndAdd(aMinusB, aOnPlane[0]);
		if (!bMinusA.empty()) rebuildAndAdd(bMinusA, bOnPlane[0]);
	}
	else
	{
		// only one side (or neither) has faces on the plane, keep as-s
		for (const auto* face : aOnPlane) result.faces.push_back(*face);
		for (const auto* face : bOnPlane) result.faces.push_back(*face);
	}

	return result;
}

BRepVolume BuildBRepFromBSP(
	const eqg::Terrain& terrain,
	const AreaBSPTree& bspTree,
	uint32_t nodeNum,
	std::vector<PolyPlane>& currentPlanes)
{
	if (bspTree.nodes.find(nodeNum) == bspTree.nodes.end())
		return {};

	const auto& node = bspTree.nodes.at(nodeNum);

	if (node.region != 0)
	{
		BRepVolume result;

		// the PolyPlane ID is set in the clips member of each poly vertex. Use that to find boundary planes
		Polyhedron poly = BuildPolyhedron(terrain.m_aabb.min, terrain.m_aabb.max, currentPlanes);

		// Extract faces as polygons (not triangulated)
		std::vector<std::vector<int>> faces = PolyClipper::extractFaces(poly);
		result.faces.reserve(faces.size());

		for (const auto& faceIndexes : faces)
		{
			BRepVolume::Face face;
			face.areaId = bspTree.areaNum; // TODO: is this - 1?

			// find the plane that contains this face with the intersection of planeIds
			std::set<int> planeIds;
			if (!faceIndexes.empty())
			{
				planeIds.insert(poly[faceIndexes.front()].clips.begin(), poly[faceIndexes.front()].clips.end());
				face.vertexes.reserve(faceIndexes.size());
				for (int idx : faceIndexes)
				{
					std::set<int> intersection;
					std::set_intersection(
						poly[idx].clips.begin(), poly[idx].clips.end(),
						planeIds.begin(), planeIds.end(),
						std::inserter(intersection, intersection.begin()));

					planeIds = std::move(intersection);
					face.vertexes.push_back(poly[idx].position);
				}
			}
			else
				SPDLOG_ERROR("Empty face for {}", nodeNum);


			if (planeIds.size() > 0)
				face.planeId = *planeIds.begin();
			else
				SPDLOG_ERROR("Got 0 intersecting planes for face");

			if (planeIds.size() > 1)
				SPDLOG_WARN("Got more than 1 intersecting planes for face");

			result.faces.push_back(std::move(face));
		}

		return result;
	}

	auto plane = PolyPlane(
		node.dist,
		VA::Vector(node.normal.x, node.normal.y, node.normal.z),
		nodeNum);

	currentPlanes.push_back(plane);
	auto frontBRep = BuildBRepFromBSP(terrain, bspTree, node.front, currentPlanes);
	currentPlanes.pop_back();

	plane.normal = -plane.normal;
	plane.dist = -plane.dist;
	currentPlanes.push_back(plane);
	auto backBRep = BuildBRepFromBSP(terrain, bspTree, node.back, currentPlanes);
	currentPlanes.pop_back();

	if (frontBRep.faces.empty()) return backBRep;
	if (backBRep.faces.empty()) return frontBRep;

	return mergeAlongHyperplane(frontBRep, backBRep, plane);
}

std::vector<BRepResult> BuildBRepsFromAreas(const eqg::Terrain& terrain)
{
	std::vector<BRepResult> results;

	if (!terrain.m_wldBspTree || terrain.m_wldBspTree->nodes.empty())
		return results;

	auto areaTrees = BuildAreaBSPTrees(terrain);
	for (const auto& [_, areaTree] : areaTrees)
	{
		std::vector<PolyPlane> currentPlanes;
		auto volume = BuildBRepFromBSP(terrain, areaTree, 1, currentPlanes);

		BRepResult brep;
		brep.areaIndex = areaTree.areaNum;
		std::vector<BRepVolume::Vec3> vertexPositions;

		auto findOrAddVertex = [&vertexPositions, &brep](const BRepVolume::Vec3& pos)
		{
			for (size_t i = 0; i < vertexPositions.size(); ++i)
				if (glm::length2(vertexPositions[i] - pos) < PLANE_THICKNESS * PLANE_THICKNESS)
					return static_cast<uint16_t>(i);

			vertexPositions.push_back(pos);
			brep.vertexes.push_back(pos);

			return static_cast<uint16_t>(vertexPositions.size() - 1);
		};

		for (const auto& face : volume.faces)
		{
			if (face.valid && face.vertexes.size() >= 3)
			{
				std::vector<uint16_t> ids;
				ids.reserve(face.vertexes.size());
				for (const auto& vert : face.vertexes)
					ids.push_back(findOrAddVertex(vert));

				brep.faces.push_back(std::move(ids));
			}
		}

		results.push_back(brep);
	}

	SPDLOG_INFO("Built {} BReps from WLD BSP tree", results.size());
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
	return { QuantizeVec3(p.normal, 0.01f), QuantizeFloat(p.distance, 0.01f) };
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
		plane.distance = glm::dot(plane.normal, centroid);
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