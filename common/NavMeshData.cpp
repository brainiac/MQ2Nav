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
		"Not Walkable",          // name
		RGBA(0, 0, 0, 0),        // color
		0,                       // flags
		0.0f,                    // cost
		true,                    // valid
		true,                    // selectable
	},

	// ground
	PolyAreaType{
		1,                      // id
		"Walkable",              // name
		RGBA(0, 192, 255, 255),  // color
		+PolyFlags::Walk,        // flags
		1.0f,                    // cost
		true,                    // valid
		true,                    // selectable
	},

	// jump
	PolyAreaType{
		2,                       // id
		"Jump",                  // name
		RGBA(127, 127, 0, 64),   // color
		+PolyFlags::Jump,        // flags
		1.5f,                    // cost
		true,                    // valid
		true,                    // selectable
	},

	// water
	PolyAreaType{
		3,                       // id
		"Water",                 // name
		RGBA(0, 0, 255, 255),    // color
		+PolyFlags::Swim,        // flags
		10.0f,                   // cost
		true,                    // valid
		true,                    // selectable
	},

	PolyAreaType{
		4,                       // id
		"Door",                  // name
		RGBA(0, 255, 255, 255),  // color
		+(PolyFlags::Walk | PolyFlags::Door),
		1.1f,                    // cost
		true,                    // valid
		false,                   // selectable
	},
};

bool IsUserDefinedPolyArea(uint8_t areaId)
{
	return areaId >= static_cast<uint8_t>(PolyArea::UserDefinedFirst)
		&& areaId <= static_cast<uint8_t>(PolyArea::UserDefinedLast);
}

bool operator==(const PolyAreaType& a, const PolyAreaType& b)
{
	auto make_tuple = [](const PolyAreaType& a) {
		return std::tie(a.id, a.name, a.color, a.flags, a.cost, a.valid);
	};

	return make_tuple(a) == make_tuple(b);
}
