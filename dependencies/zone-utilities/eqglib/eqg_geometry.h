
#pragma once

#include "eqg_resource.h"
#include "eqg_structs.h"

#include <glm/glm.hpp>
#include <chrono>
#include <vector>

using std::chrono::steady_clock;

namespace eqg {
struct SDMSpriteDef2WLDData;

class MaterialPalette;

struct SFrameTransform
{
	glm::quat rotation;
	glm::vec3 pivot;
	float     scale;
};

// Maybe this goes into eqg_animation.h?
struct STrack
{
	std::string_view tag;
	float            speed;
	bool             reverse;
	bool             interpolate;
	uint32_t         sleepTime;
	uint32_t         numFrames;
	std::vector<SFrameTransform> frameTransforms;
	bool             attachedToModel;
};

struct SFace
{
	glm::u32vec3  indices;
	EQG_FACEFLAGS flags;
	int16_t       materialIndex;

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

struct SMaterialGroup
{
	uint32_t vertexStart;
	uint32_t vertexCount;
	uint32_t faceStart;
	uint32_t faceCount;
};

class Geometry
{
public:
	std::string tag;

	std::vector<SEQMVertex> vertices;
	std::vector<SFace> faces;

	std::shared_ptr<MaterialPalette> material_palette;
};

enum ECollisionVolumeType
{
	eCollisionVolumeNone = 0,
	eCollisionVolumeModel,
	eCollisionVolumeSphere,
	eCollisionVolumeDag,
	eCollisionVolumeBox,
};

class SimpleModelDefinition : public Resource
{
public:
	SimpleModelDefinition();
	~SimpleModelDefinition() override;

	static ResourceType GetStaticResourceType() { return ResourceType::SimpleModelDefinition; }

	std::string_view GetTag() const override { return m_tag; }

	bool InitFromWLDData(std::string_view tag, SDMSpriteDef2WLDData* pWldData);

private:
	void InitCollisionData(bool forceModel);

public:
	std::string                   m_tag;

	uint32_t                      m_numVertices = 0;
	uint32_t                      m_numFaces = 0;
	uint32_t                      m_numFrames = 1;
	std::vector<glm::vec3>        m_vertices;
	std::vector<glm::vec2>        m_uvs;
	std::vector<glm::vec2>        m_uv2s;
	std::vector<glm::vec3>        m_normals;
	std::vector<uint32_t>         m_colors;
	std::vector<uint32_t>         m_colorTint;
	std::vector<SFace>            m_faces;
	std::vector<glm::vec3>        m_faceNormals;

	glm::vec3                     m_aabbMin = glm::vec3(0.0f);
	glm::vec3                     m_aabbMax = glm::vec3(0.0f);
	glm::vec3                     m_centerOffset = glm::vec3(0.0f);
	float                         m_boundingRadius = 0.0f;

	bool                          m_hasCollision = false;
	ECollisionVolumeType          m_defaultCollisionType = eCollisionVolumeNone;

	steady_clock::time_point      m_lastUpdate = steady_clock::now();
	uint32_t                      m_updateInterval;
	std::shared_ptr<MaterialPalette> m_materialPalette;
	std::vector<SMaterialGroup>   m_materialGroups;
};

} // namespace eqg
