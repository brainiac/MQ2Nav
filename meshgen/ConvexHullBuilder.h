//
// ConvexHullBuilder.h
//

#pragma once

#include "eqglib/eqg_terrain.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace eqg
{
	class Terrain;
}

// Result of building a convex hull from BSP planes
struct ConvexHullResult
{
	std::string areaTag;
	eqg::AreaEnvironment environment;
	const eqg::AreaTeleport* teleport = nullptr;

	std::vector<glm::vec3> vertices;
	std::vector<uint16_t> triangleIndices;
	std::vector<std::pair<uint16_t, uint16_t>> edges;
};

// Build all convex hulls for regions in areas
std::vector<ConvexHullResult> BuildConvexHullsFromTerrain(const eqg::Terrain& terrain);
