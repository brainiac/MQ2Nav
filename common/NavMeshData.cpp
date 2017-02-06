//
// NavMeshData.cpp
//

#include "NavMeshData.h"

constexpr unsigned long RGBA(uint8_t iR, uint8_t iG, uint8_t iB, uint8_t iA)
{
	return (iA << 24) | (iB << 16) | (iG << 8) | iR;
}

const std::vector<PolyAreaType> DefaultPolyAreas =
{
	// unwalkable
	PolyAreaType{
		0,                       // id
		"unwalkable",            // name
		RGBA(0, 0, 0, 0),        // color
		0,                       // flags
		0.0f,                    // cost
	},

	// ground
	PolyAreaType{
		1,                       // id
		"ground",                // name
		RGBA(0, 192, 255, 255),  // color
		+PolyFlags::Walk,        // flags
		1.0f,                    // cost
	},

	// water
	PolyAreaType{
		2,                       // id
		"water",                 // name
		RGBA(0, 0, 255, 255),    // color
		+PolyFlags::Swim,        // flags
		10.0f,                   // cost
	},

	// jump
	PolyAreaType{
		3,                       // id
		"jump",                  // name
		RGBA(127, 127, 0, 64),   // color
		+PolyFlags::Jump,        // flags
		1.5f,                    // cost
	},

	// water
	PolyAreaType{
		4,                       // id
		"road",                  // name
		RGBA(50, 20, 12, 255),   // color
		+PolyFlags::Walk,        // flags
		0.8f,                    // cost
	},
};

bool IsUserDefinedPolyArea(uint8_t areaId)
{
	return areaId >= static_cast<uint8_t>(PolyArea::UserDefinedFirst)
		&& areaId <= static_cast<uint8_t>(PolyArea::UserDefinedLast);
}
