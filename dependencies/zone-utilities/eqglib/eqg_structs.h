
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace eqg {

constexpr float S3D_UV_TO_FLOAT = 1.0f / 256.0f;
constexpr float S3D_NORM_TO_FLOAT = 1.0f / 127.0f;

enum EQG_FACEFLAGS : uint16_t
{
	EQG_FACEFLAG_NONE                = 0,
	EQG_FACEFLAG_PASSABLE            = 0x01,
	EQG_FACEFLAG_TRANSPARENT         = 0x02,
	EQG_FACEFLAG_COLLISION_REQUIRED  = 0x04,
	EQG_FACEFLAG_CULLED              = 0x08,
	EQG_FACEFLAG_DEGENERATE          = 0x10,
};

struct SEQZoneParameters
{
	std::string name;
	int32_t     min_lng = 0;
	int32_t     max_lng = 0;
	int32_t     min_lat = 0;
	int32_t     max_lat = 0;
	float       units_per_vert = 0;
	int         verts_per_tile = 0;
	int         quads_per_tile = 0;
	float       units_per_tile = 0;
	int         tiles_per_chunk = 0;
	float       units_per_chunk = 0;
	int         cover_map_input_size = 0;
	int         layer_map_input_size = 0;
	glm::vec3   min_extents = glm::vec3(0);
	glm::vec3   max_extents = glm::vec3(0);

	int version = 1;
};

struct SZONHeader
{
	char     magic[4];
	uint32_t version;
	uint32_t string_pool_size;
	uint32_t num_meshes;
	uint32_t num_instances;
	uint32_t num_areas;
	uint32_t num_lights;
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

struct SEQMVertexOld
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
};

struct SEQMVertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	uint32_t color;
	glm::vec2 uv;
	glm::vec2 uv2;
};

struct SEQMFace
{
	uint32_t vertices[3];
	uint32_t material;
	uint32_t flags;
};

struct SEQMMaterial
{
	uint32_t index;
	int name_index;
	int effect_name_index;
	uint32_t num_params;
};

enum eEQMFXParameterType
{
	eEQMFXParameterUnused,
	eEQMFXParameterInt,
	eEQMFXParameterTexture,
	eEQMFXParameterColor,
};

struct SEQMFXParameter
{
	int name_index;
	eEQMFXParameterType type;

	union
	{
		uint32_t n_value;
		float f_value;
	};
};

struct SEQMUVSet
{
	glm::vec2 uv;
};

struct SEQMBoneData
{
	int name_index;
	int next_index;
	uint32_t num_children;
	int first_child_index;
	glm::vec3 pivot;
	glm::quat quat;
	glm::vec3 scale;
};

struct SEQMBone
{
	std::string_view name;
	int next_index;
	uint32_t num_children;
	int first_child_index;
	glm::vec3 pivot;
	glm::quat quat;
	glm::vec3 scale;
};

struct SEQMSkinData
{
	uint32_t num_weights;

	struct Weights
	{
		int bone;
		float weight;
	};
	Weights weights[4];
};

struct STERHeader
{
	char magic[4];
	uint32_t version;
	uint32_t string_pool_size;
	uint32_t num_materials;
	uint32_t num_vertices;
	uint32_t num_faces;
};

}; // namespace eqg
