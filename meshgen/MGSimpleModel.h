//
// MGSimpleModel.h
//

#pragma once

#include "StaticMeshRenderSystem.h"
#include "eqglib/eqg_geometry.h"

#include "bgfx/bgfx.h"
#include "glm/glm.hpp"

namespace eqg { class Material; }

// GPU-aware SimpleModel that manages bgfx vertex/index buffers
class MGSimpleModel : public eqg::SimpleModel
{
public:
	MGSimpleModel();
	~MGSimpleModel() override;

	bool BuildGPUBuffers();
	void DestroyGPUBuffers();

	bool HasGPUBuffers() const { return m_gpuBuffersBuilt; }

	// Get buffer handles for rendering
	bgfx::VertexBufferHandle GetVertexBuffer() const { return m_vertexBuffer; }
	bgfx::IndexBufferHandle GetIndexBuffer() const { return m_indexBuffer; }
	uint32_t GetIndexCount() const { return m_indexCount; }

	// Get material batches for textured rendering
	const std::vector<MaterialBatch>& GetMaterialBatches() const { return m_materialBatches; }

private:
	void CreateMaterialBatches();

	bgfx::VertexBufferHandle m_vertexBuffer = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle m_indexBuffer = BGFX_INVALID_HANDLE;
	uint32_t m_indexCount = 0;
	bool m_gpuBuffersBuilt = false;
	std::vector<MaterialBatch> m_materialBatches;
};

using MGSimpleModelPtr = std::shared_ptr<MGSimpleModel>;

