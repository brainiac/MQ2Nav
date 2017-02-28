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

//----------------------------------------------------------------------------

std::map<std::string, std::pair<glm::vec3, glm::vec3>> MaxZoneExtents = {
	// TBS
	{
		"kattacastrum",
		{
			{ 0., -1324., 0. },
			{ 0., 0., 0. },
		}
	},

	// VOA
	//{
	//	"beastdomain",
	//	{
	//		{ -1907.2, 0., -7469.2 },
	//		{ 5172.6, 0., 1643.6 },
	//	}
	//},

	// ROF
	{
		"heartoffearb",
		{
			{ 0., 0., 0. },
			{ 0., 160.8, 0. },
		}
	},
	{
		"heartoffearc",
		{
			{ 0., 0., 0. },
			{ 0., 160.8, 0. },
		},
	},
	{
		"chelsithreborn",
		{
			{ 0., -540., 0. },
			{ 0., 0., 0. },
		}
	},

	// COF
	{
		"ethernere",
		{
			{ 0., 0., 0. },
			{ 1520.5, 0., 937.1 },
		}
	},
	{
		"neriakd",
		{
			{ 0., -525, 0. },
			{ 0., 0., 0. },
		}
	},
	{
		"thevoidh",
		{
			{ 0., -4314.2, 0. },
			{ 0., 0., 0. },
		}
	},
	{
		"towerofrot",
		{
			{ 0., -954.6, 0. },
			{ 0., 0., 0. },
		}
	},

	// TDS
	{
		"endlesscaverns",
		{
			{ 0., -355.8, 0. },
			{ 0., 0., 0. },
		}
	},
	{
		"dredge",
		{
			{ 0., -220., 0. },
			{ 0., 0., 0. },
		}
	},
	{
		"kattacastrumb",
		{
			{ 0., -1324., 0. },
			{ 0., 0., 0. },
		}
	},

	// TBM
	{
		"exalted",
		{
			{ 0., 0., -1870. },
			{ 0., 0., 0. }
		}
	},
	{
		"exaltedb",
		{
			{ 0., 0., -1870. },
			{ 0., 0., 0. }
		}
	},
	{
		"pohealth", // not sure about this one
		{
			{ 0., -297.4, 0. },
			{ 0., 0., 0. }
		}
	},

	// EoK
	{
		"chardoktwo",
		{
			{ 0., 0., 0. },
			{ 0., 0., 1330. }
		}
	},
	{
		"frontiermtnsb",
		{
			{ 0., 0., 0. },
			{ 5000., 0., 0. }
		}
	},
	{
		"korshaint",
		{
			{ 0., 0., 451. },
			{ 0., 0., 0. }
		}
	},
	{
		"lceanium",
		{
			{ 0., -232., 0. },
			{ 0., 0., 0. }
		}
	},
	{
		"scorchedwoods",
		{
			{ 0., -666., 0. },
			{ 0., 0., 0. }
		},
	}
};
