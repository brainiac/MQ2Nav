//
// MGSimpleModel.h
//

#pragma once

#include "meshgen/RenderBatch.h"
#include "eqglib/eqg_geometry.h"

#include "bgfx/bgfx.h"
#include "glm/glm.hpp"

namespace eqg { class Material; }

class MGSimpleModelDefinition : public eqg::SimpleModel
{
public:
};

// The GPU-side geometry of one model definition, shared by every instance placed from it.
// A zone can place thousands of copies of a few dozen models - candlemakers places 7000
// from 160 definitions - and bgfx's static buffer handle pools are 4096 entries each, so
// a buffer pair per instance exhausts them and every later creation, including the
// navmesh's, silently fails.
struct SharedModelBuffers
{
	~SharedModelBuffers();

	bgfx::VertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle indexBuffer = BGFX_INVALID_HANDLE;
	uint32_t indexCount = 0;
	std::vector<MaterialBatch> materialBatches;
};
using SharedModelBuffersPtr = std::shared_ptr<SharedModelBuffers>;

// GPU-aware SimpleModel that manages bgfx vertex/index buffers
class MGSimpleModel : public eqg::SimpleModel
{
public:
	MGSimpleModel();
	virtual ~MGSimpleModel() override;

	virtual bool InitBatchInstances() override;

	bool BuildGPUBuffers();
	void DestroyGPUBuffers();

	bool HasGPUBuffers() const { return m_gpuBuffersBuilt; }

	// Get buffer handles for rendering. These belong to the definition, not to this
	// instance - every instance of the same model returns the same handles.
	bgfx::VertexBufferHandle GetVertexBuffer() const { return m_shared ? m_shared->vertexBuffer : bgfx::VertexBufferHandle{ bgfx::kInvalidHandle }; }
	bgfx::IndexBufferHandle GetIndexBuffer() const { return m_shared ? m_shared->indexBuffer : bgfx::IndexBufferHandle{ bgfx::kInvalidHandle }; }
	uint32_t GetIndexCount() const { return m_shared ? m_shared->indexCount : 0; }

	// Get material batches for textured rendering
	const std::vector<MaterialBatch>& GetMaterialBatches() const;

private:
	SharedModelBuffersPtr m_shared;
	bool m_gpuBuffersBuilt = false;
};

using MGSimpleModelPtr = std::shared_ptr<MGSimpleModel>;

