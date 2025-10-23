//
// NavMeshData.h
//

#pragma once

#include "mq/base/Enum.h"
#include "glm/glm.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

enum struct PolyFlags : uint16_t
{
	Walk          = 0x01, // ability to walk (ground, grass, road, etc)
	Swim          = 0x02, // ability to swim (water) (unused)
	Jump          = 0x04, // ability to jump. (unused)
	Disabled      = 0x08, // disabled polygon
	Door          = 0x10, // ability to open doors

	All           = 0xffff,
};
constexpr bool has_bitwise_operations(PolyFlags) { return true; }

enum struct PolyArea : uint8_t
{
	Unwalkable = 0,        // RC_NULL_AREA
	Ground     = 1,        // RC_WALKABLE_AREA
	Jump       = 2,
	Water      = 3,
	Door       = 4,
	Prefer     = 5,
	Avoid      = 6,

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
	bool selectable;
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

enum struct NavMeshHeaderVersion : uint16_t {
	Version4 = 4,                // base version
	Version5 = 5,                // version 5 introduced headerSize and uncompressedSize

	Latest = Version5,
};

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

struct MeshFileHeaderV5 : MeshFileHeader
{
	uint32_t uncompressedSize;
	uint32_t headerSize;
};

// compatibility version of the navmesh data
const int NAVMESH_TILE_COMPAT_VERSION = 1;

// Maximum number of nodes in navigation query
const int NAVMESH_QUERY_MAX_NODES = 16384;

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

//----------------------------------------------------------------------------

// Max Zone Extents
// these are the limits to the extents of geometry we are willing to load. This
// is used to exclude junk geometry from very far away parts of the mesh. Values
// of 0 are ignored.

extern std::map<std::string, std::pair<glm::vec3, glm::vec3>> MaxZoneExtents;

//----------------------------------------------------------------------------

// Connections
// these describe connections a.k.a. off-mesh connections between two points on
// the mesh. At first connections are only supported between two adjacent tiles,
// but hopefully we can extend that in the future.

enum struct ConnectionType : uint8_t
{
	// Basic connections are the initial type of connection. They don't support
	// much, but they work for short distances.
	Basic = 0,
};

struct OffMeshConnection
{
	uint32_t id = 0;
	glm::vec3 start = { 0, 0, 0 };
	glm::vec3 end = { 0, 0, 0 };
	ConnectionType type = ConnectionType::Basic;
	uint8_t areaType = (uint8_t)PolyArea::Ground;
	bool bidirectional = true;

	// optional name provided by the user
	std::string name;

	// false if start/end positions aren't valid. Invalid connections don't get saved.
	bool valid = true;
};
