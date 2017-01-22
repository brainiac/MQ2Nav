//
// NavMeshData.h
//

#pragma once

#include <cstdint>

namespace PolyFlags { enum Enum : uint16_t
{
	Walk          = 0x01, // ability to walk (ground, grass, road, etc)
	Swim          = 0x02, // ability to swim (water) (unused)
	Door          = 0x04, // ability to move through doors (unused)
	Jump          = 0x08, // ability to join. (unused)
	Disabled      = 0x10, // disabled polygon

	All           = 0xffff,
};}

namespace PolyArea { enum Enum : uint8_t
{
	Water = 1,
	Road,
	Door,
	Grass,
	Jump,
	Avoid,

	Unwalkable = 0, //RC_NULL_AREA
	Ground = 63, //RC_WALKABLE_AREA
};}
