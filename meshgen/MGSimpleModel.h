//
// MGSimpleModel.h
//
// GPU-aware SimpleModel subclass for MeshGenerator rendering
//

#pragma once

#include "eqglib/eqg_geometry.h"

#include <bgfx/bgfx.h>
#include <glm/glm.hpp>

namespace eqg { class Material; }

// Material batch for rendering - groups faces by material for texture binding
struct MaterialBatch
{
	eqg::Material* material = nullptr;  // Material for this batch (may be null)
	uint32_t startIndex = 0;            // Start index in index buffer
	uint32_t indexCount = 0;            // Number of indices in this batch
	bool isTransparent = false;         // Requires alpha blending
};

// Vertex format for static mesh rendering
// Structured for future texture support
struct StaticMeshVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;
	uint32_t  color;  // ABGR

	static void Init()
	{
		ms_layout
			.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();
	}

	inline static bgfx::VertexLayout ms_layout;
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

	static void Init()
	{
		ms_layout
			.begin()
				.add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
			.end();
	}

	inline static bgfx::VertexLayout ms_layout;
};

// GPU-aware SimpleModel that manages bgfx vertex/index buffers
class MGSimpleModel : public eqg::SimpleModel
{
public:
	MGSimpleModel();
	~MGSimpleModel() override;

	// Build GPU buffers from definition data. Call before first render.
	// Returns true if buffers are valid and ready for rendering.
	bool BuildGPUBuffers();

	// Release GPU buffers
	void DestroyGPUBuffers();

	// Check if GPU buffers are built and valid
	bool HasGPUBuffers() const { return m_gpuBuffersBuilt; }

	// Get buffer handles for rendering
	bgfx::VertexBufferHandle GetVertexBuffer() const { return m_vertexBuffer; }
	bgfx::IndexBufferHandle GetIndexBuffer() const { return m_indexBuffer; }
	uint32_t GetIndexCount() const { return m_indexCount; }

	// Get material batches for textured rendering
	const std::vector<MaterialBatch>& GetMaterialBatches() const { return m_materialBatches; }

private:
	bgfx::VertexBufferHandle m_vertexBuffer = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle m_indexBuffer = BGFX_INVALID_HANDLE;
	uint32_t m_indexCount = 0;
	bool m_gpuBuffersBuilt = false;
	std::vector<MaterialBatch> m_materialBatches;
};

using MGSimpleModelPtr = std::shared_ptr<MGSimpleModel>;

