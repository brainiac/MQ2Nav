//
// MGTerrain.h
//
// GPU-aware Terrain subclass for MeshGenerator rendering
//

#pragma once

#include "meshgen/MGSimpleModel.h"  // For StaticMeshVertex
#include "eqglib/eqg_terrain.h"

#include <bgfx/bgfx.h>

// GPU-aware Terrain that manages bgfx vertex/index buffers
class MGTerrain : public eqg::Terrain
{
public:
	MGTerrain();
	~MGTerrain() override;

	// Build GPU buffers from terrain data. Call before first render.
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

using MGTerrainPtr = std::shared_ptr<MGTerrain>;

