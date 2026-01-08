#pragma once

#include "eqglib/eqg_geometry.h"

namespace eqg
{
	class Actor;
}

struct ActorComponent
{
	eqg::Actor* actor;
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
