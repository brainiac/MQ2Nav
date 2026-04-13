//
// RenderBatch.h
//

#pragma once

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"

#include <cstdint>

namespace bgfx
{
	struct VertexLayout;
}

namespace eqg
{
	class Material;
}

// Material batch for rendering - groups faces by material for texture binding
struct MaterialBatch
{
	eqg::Material* material = nullptr;  // Material for this batch
	uint32_t startIndex = 0;            // Start index in index buffer
	uint32_t indexCount = 0;            // Number of indices in this batch

	bool isAlphaBlend = false;
	bool isTint = false;
};

// Vertex format for static mesh rendering
// Structured for future texture support
struct StaticMeshVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;
	uint32_t  colorDiffuse;  // ABGR
	uint32_t  colorTint;

	static void Init();
	static bgfx::VertexLayout& GetLayout();
};

// Instance data for instanced rendering
struct MeshInstanceData
{
	glm::vec4 row0;  // Transform matrix row 0
	glm::vec4 row1;  // Transform matrix row 1
	glm::vec4 row2;  // Transform matrix row 2
	glm::vec4 row3;  // Transform matrix row 3

	MeshInstanceData() = default;

	explicit MeshInstanceData(const glm::mat4& transform)
		: row0(transform[0])
		, row1(transform[1])
		, row2(transform[2])
		, row3(transform[3])
	{
	}

	static void Init();
	static bgfx::VertexLayout& GetLayout();
};
