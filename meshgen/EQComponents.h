#pragma once

//
// EQComponents.h
// Component definitions for EverQuest zone entities
//

#include "eqglib/eqg_geometry.h"
#include "eqglib/eqg_terrain.h"
#include "eqglib/wld_types.h"
#include "mq/base/Color.h"

#include <bgfx/bgfx.h>
#include <glm/glm.hpp>
#include <vector>

class MGSimpleModel;
class MGHierarchicalModel;
class MGTerrain;
class MGTerrainTile;

namespace eqg
{
	class Actor;
}

struct ActorComponent
{
	eqg::ActorPtr actor;
};

struct CollisionComponent
{
	eqg::SimpleModelPtr collisionModel;
	float boundingRadius;
};

struct PointLightComponent
{
	float radius;
	bool dynamic;
	eqg::LightDefinitionPtr definition;
};

struct AreaComponent
{
	eqg::TerrainAreaPtr area;
	mq::MQColor color;
};

// Convex hull - primitive shape useful for applying modifications to the navmesh.
struct ConvexHullComponent
{
	std::vector<glm::vec3> vertices;
	std::vector<uint16_t> indices;
};

// The individual WLD Area. Also stores the list of convex hulls that compose the area,
// since we may need it for the navmesh. AreaVolumeComponent will merge these, which
// may result in a shape that is no longer convex.
struct WldAreaComponent
{
	eqg::AreaEnvironment environment;
	eqg::AreaTeleport teleport;

	uint32_t areaIndex;
	const eqg::SArea* area;
	mq::MQColor color;

	std::vector<ConvexHullComponent> hulls;
};

// Merged volumes that correspond with a single wld area.
struct AreaVolumeComponent
{
	std::vector<glm::vec3> vertices;
	std::vector<std::vector<uint16_t>> faces;         // Polygon faces (not triangulated)
};

// Render configuration for AreaVolumeComponent.
struct AreaVolumeRenderComponent
{
	uint32_t color = 0x33FFFFFF;    // ABGR, 20% alpha default
};

// Component for entities with renderable static mesh geometry (SimpleModel)
struct StaticMeshRenderComponent
{
	MGSimpleModel* model = nullptr;
};

// Component for entities with renderable skeletal mesh geometry (HierarchicalModel)
struct SkeletalMeshRenderComponent
{
	MGHierarchicalModel* model = nullptr;
};

// Component for terrain geometry rendering (WLD zones)
struct TerrainRenderComponent
{
	MGTerrain* terrain = nullptr;
};

// Component for terrain tile geometry rendering (EQG zones with TerrainSystem)
struct TerrainTileRenderComponent
{
	MGTerrainTile* tile = nullptr;
};

// Invisible wall geometry - world-space vertices defining the base line, extruded by wallHeight
struct InvisibleWallComponent
{
	std::vector<glm::vec3> vertices;  // World-space base vertices
	float wallHeight = 100.0f;        // Height to extrude upward
};

// Render configuration for invisible walls
struct InvisibleWallRenderComponent
{
	uint32_t fillColor = 0x40808080;     // ABGR: 25% alpha grey
	uint32_t outlineColor = 0xFFFFFFFF;  // ABGR: white
	float lineWidth = 2.0f;
};
