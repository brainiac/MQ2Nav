
#pragma once

#include "s3d_types.h"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <memory>

namespace eqg::s3d {

class TextureBrushSet;
class TextureBrush;

class BSPRegion
{
public:
	uint32_t flags;
	std::vector<uint32_t> regions;
	std::string tag;
	std::string old_style_tag;
};

struct SPlanarEquation
{
	glm::vec3 normal;
	float dist;
};

class BSPTree // SWorldTreeWLDData
{
public:
	struct BSPNode // SAreaBSPTree
	{
		SPlanarEquation plane;
		uint32_t region;
		uint32_t front;
		uint32_t back;
	};

	std::vector<BSPNode>& GetNodes() { return nodes; }

	std::vector<BSPNode> nodes;
};

class Geometry
{
public:
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec2 tex;
		glm::vec3 nor;
	};

	struct Polygon
	{
		uint32_t flags;
		uint32_t verts[3];
		uint32_t tex;
	};

	void SetName(std::string nname) { tag = nname; }
	void SetTextureBrushSet(std::shared_ptr<TextureBrushSet> tbs) { tex = tbs; }

	std::vector<Vertex>& GetVertices() { return verts; }
	std::vector<Polygon>& GetPolygons() { return polys; }
	std::string& GetName() { return tag; }
	std::shared_ptr<TextureBrushSet> GetTextureBrushSet() { return tex; }

	std::string tag;
	std::vector<Vertex> verts;
	std::vector<Polygon> polys;
	std::shared_ptr<TextureBrushSet> tex;
};

class SkeletonTrack // SHSpriteDefWLDData
{
public:
	struct FrameTransform
	{
		glm::quat rotation;
		glm::vec3 pivot;
		float scale;
	};

	struct Bone // SDagWLDData, SDMSpriteDef2WLDData
	{
		std::vector<FrameTransform> transforms;
		std::shared_ptr<Geometry> model;
		std::vector<std::shared_ptr<Bone>> children;
	};

	std::vector<std::shared_ptr<Bone>>& GetBones() { return bones; }

	std::string_view name;
	glm::vec3 center_offset = glm::vec3(0.0f);
	float bounding_radius = 1.0f;
	std::vector<std::shared_ptr<Bone>> bones;

	std::vector<int> attached_skeleton_dag_index;
};


class Texture
{
public:
	std::vector<std::string>& GetTextureFrames() { return frames; }

	std::vector<std::string> frames;
};


class TextureBrushSet
{
public:
	std::vector<std::shared_ptr<TextureBrush>>& GetTextureSet() { return texture_sets; }

	std::vector<std::shared_ptr<TextureBrush>> texture_sets;
};


class TextureBrush
{
public:
	void SetFlags(uint32_t f) { flags = f; }

	std::vector<std::shared_ptr<Texture>>& GetTextures() { return brush_textures; }
	uint32_t GetFlags() { return flags; }

	std::vector<std::shared_ptr<Texture>> brush_textures;
	uint32_t flags = 0;
};

} // namespace eqg::s3d
