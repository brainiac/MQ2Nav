//
// NavMeshData.h
//

#pragma once

#include "Enum.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

enum struct PolyFlags : uint16_t
{
	Walk          = 0x01, // ability to walk (ground, grass, road, etc)
	Swim          = 0x02, // ability to swim (water) (unused)
	Jump          = 0x04, // ability to jump. (unused)
	Disabled      = 0x08, // disabled polygon

	All           = 0xffff,
};
constexpr bool has_bitwise_operations(PolyFlags) { return true; }

enum struct PolyArea : uint8_t
{
	Unwalkable = 0,        // RC_NULL_AREA
	Ground     = 1,        // RC_WALKABLE_AREA
	Jump       = 2,
	Water      = 3,

	UserDefinedFirst = 10,
	UserDefinedLast  = 60,

	Last = 63,
};

struct PolyAreaType
{
	uint8_t id;             // PolyArea
	std::string name;
	uint32_t color;
	uint16_t flags;
	float cost;
	bool valid;
};

bool operator==(const PolyAreaType& a, const PolyAreaType& b);

extern const std::vector<PolyAreaType> DefaultPolyAreas;

bool IsUserDefinedPolyArea(uint8_t areaId);


enum struct PartitionType : uint32_t
{
	WATERSHED = 0,
	MONOTONE = 1,
	LAYERS = 2,
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

//----------------------------------------------------------------------------
// file format data

// extension used for navmesh files
const char* const NAVMESH_FILE_EXTENSION = ".navmesh";

// header constants
const int NAVMESH_FILE_MAGIC = 'MSET';
const int NAVMESH_FILE_VERSION = 4;

enum struct NavMeshFileFlags : uint16_t {
	COMPRESSED = 0x0001
};
constexpr bool has_bitwise_operations(NavMeshFileFlags) { return true; }

struct MeshFileHeader
{
	uint32_t magic;
	uint16_t version;
	NavMeshFileFlags flags;
};

// compatibility version of the navmesh data
const int NAVMESH_TILE_COMPAT_VERSION = 1;

// Maximum number of nodes in navigation query
const int NAVMESH_QUERY_MAX_NODES = 4096;

//----------------------------------------------------------------------------

// Convex Volumes

struct ConvexVolume
{
	uint32_t id;
	std::vector<glm::vec3> verts;
	float hmin, hmax;
	uint8_t areaType;

	std::string name;
};

