
#pragma once

#include "eqg_material.h"
#include "eqg_structs.h"

#include <glm/glm.hpp>
#include <vector>

namespace eqg {


struct SFace
{
	glm::u32vec3 indices;
	EQG_FACEFLAGS flags;
	uint16_t materialIndex;

	SFace() = default; // default uninit

	bool IsCollidable() const
	{
		return (flags & EQG_FACEFLAG_COLLISION_REQUIRED) != 0
			|| (flags & EQG_FACEFLAG_PASSABLE) == 0;
	}

	bool IsPassable() const
	{
		return (flags & EQG_FACEFLAG_PASSABLE) != 0;
	}
	
	bool IsTransparent() const
	{
		return (flags & EQG_FACEFLAG_TRANSPARENT) != 0;
	}
};

class Geometry
{
public:
	std::string tag;

	std::vector<SEQMVertex> vertices;
	std::vector<SFace> faces;

	std::shared_ptr<MaterialPalette> material_palette;
};

} // namespace eqg
