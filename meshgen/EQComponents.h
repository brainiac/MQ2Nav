#pragma once


#include "eqglib/eqg_geometry.h"
#include "eqglib/eqg_terrain.h"
#include "mq/base/Color.h"

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
	bool draw;
};
