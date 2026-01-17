#pragma once


#include "eqglib/eqg_geometry.h"
#include "eqglib/eqg_terrain.h"
#include "eqglib/wld_types.h"
#include "mq/base/Color.h"

#include <glm/glm.hpp>
#include <vector>

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

struct WireframeMeshComponent
{
	std::vector<glm::vec3> vertices;
	std::vector<uint16_t>  indices;
	std::vector<std::pair<uint16_t, uint16_t>> edges;  // Wireframe edges
};

struct ConvexHullComponent
{
	std::vector<glm::vec3> vertices;
	std::vector<uint16_t>  indices;
};

struct WldAreaComponent
{
	eqg::AreaEnvironment environment;
	eqg::AreaTeleport teleport;

	uint32_t areaIndex;
	const eqg::SArea* area;
	mq::MQColor color;

	std::vector<ConvexHullComponent> hulls;
};

