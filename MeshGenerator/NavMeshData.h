//
// NavMeshData.h
//

#pragma once

#include <cstdint>
#include <functional>
#include <memory>

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

enum struct PartitionType : uint32_t
{
	WATERSHED,
	MONOTONE,
	LAYERS,
};

struct NavMeshConfig
{
	uint8_t configVersion = 1;

	float tileSize = 128;
	float cellSize = 0.6f;
	float cellHeight = 0.3f;
	float agentHeight = 6.0f;
	float agentRadius = 2.0f;
	float agentMaxClimb = 4.0f;
	float agentMaxSlope = 75.0f;
	float regionMinSize = 8;
	float regionMergeSize = 20;
	float edgeMaxLen = 12.0f;
	float edgeMaxError = 1.3f;
	float vertsPerPoly = 6.0f;
	float detailSampleDist = 6.0f;
	float detailSampleMaxError = 1.0f;
	PartitionType partitionType = PartitionType::WATERSHED;
};

template <typename T>
using deleting_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;
