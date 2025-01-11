
#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace eqg {

enum EQG_FACEFLAGS
{
	EQG_FACEFLAG_PASSABLE            = 0x01,
	EQG_FACEFLAG_TRANSPARENT         = 0x02,
	EQG_FACEFLAG_COLLISION_REQUIRED  = 0x04,
	EQG_FACEFLAG_CULLED              = 0x08,
	EQG_FACEFLAG_DEGENERATE          = 0x10,
};

struct SEQZoneParameters
{
	std::string name;
	int32_t min_lng;
	int32_t max_lng;
	int32_t min_lat;
	int32_t max_lat;
	float units_per_vert;
	int verts_per_tile;
	int quads_per_tile;
	float units_per_tile;
	int tiles_per_chunk;
	float units_per_chunk;
	int cover_map_input_size;
	int layer_map_input_size;
	glm::vec3 min_extents;
	glm::vec3 max_extents;

	int version = 1;
};

struct SZONHeader
{
	char magic[4];
	uint32_t version;
	uint32_t string_pool_size; // list_length;
	uint32_t num_meshes; // model_count;
	uint32_t num_instances;// object_count;
	uint32_t num_areas;// region_count;
	uint32_t num_lights;// light_count;
};

struct SZONInstance
{
	int mesh_id;
	int name;
	glm::vec3 translation;
	glm::vec3 rotation;
	float scale;
};

struct SZONArea
{
	int name;
	glm::vec3 center;
	glm::vec3 orientation;
	glm::vec3 extents;
};

struct SZONLight
{
	int name;
	glm::vec3 pos;
	glm::vec3 color;
	float radius;
};


struct SEQMHeader
{
	char magic[4];
	uint32_t version;
	uint32_t string_pool_size;
	uint32_t num_materials;
	uint32_t num_vertices;
	uint32_t num_faces;
	uint32_t num_bones;
};

struct mod_header
{
	char magic[4];
	uint32_t version;
	uint32_t list_length;
	uint32_t material_count;
	uint32_t vert_count;
	uint32_t tri_count;
};


struct mod_material
{
	uint32_t index;
	uint32_t name_offset;
	uint32_t shader_offset;
	uint32_t property_count;
};

struct mod_material_property
{
	uint32_t name_offset;
	uint32_t type;
	union
	{
		uint32_t i_value;
		float f_value;
	};
};

struct mod_vertex
{
	float x;
	float y;
	float z;
	float i;
	float j;
	float k;
	float u;
	float v;
};

struct mod_vertex3
{
	float x;
	float y;
	float z;
	float i;
	float j;
	float k;
	uint32_t color;
	float unknown036;
	float unknown040;
	float u;
	float v;
};

struct mod_polygon
{
	uint32_t v1;
	uint32_t v2;
	uint32_t v3;
	int32_t material;
	uint32_t flags;
};

} // namespace eqg
