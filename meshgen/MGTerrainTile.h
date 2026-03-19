//
// MGTerrainTile.h
//

#pragma once

#include "meshgen/MGSimpleModel.h"

#include "bgfx/bgfx.h"
#include "glm/glm.hpp"

#include <memory>
#include <vector>

namespace eqg { class TerrainTile; class TerrainSystem; }

class MGTerrainTile
{
public:
	MGTerrainTile(eqg::TerrainSystem* terrainSystem, eqg::TerrainTile* tile);
	~MGTerrainTile();

	bool BuildGPUBuffers();
	void DestroyGPUBuffers();

	bool HasGPUBuffers() const { return m_gpuBuffersBuilt; }

	bgfx::VertexBufferHandle GetVertexBuffer() const { return m_vertexBuffer; }
	bgfx::IndexBufferHandle GetIndexBuffer() const { return m_indexBuffer; }
	uint32_t GetIndexCount() const { return m_indexCount; }

	// Get tile info
	eqg::TerrainTile* GetTile() const { return m_tile; }
	eqg::TerrainSystem* GetTerrainSystem() const { return m_terrainSystem; }
	const glm::vec3& GetPosition() const;

	const std::vector<MaterialBatch>& GetMaterialBatches() const { return m_materialBatches; }

private:
	eqg::TerrainSystem* m_terrainSystem = nullptr;
	eqg::TerrainTile* m_tile = nullptr;

	bgfx::VertexBufferHandle m_vertexBuffer = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle m_indexBuffer = BGFX_INVALID_HANDLE;
	uint32_t m_indexCount = 0;
	bool m_gpuBuffersBuilt = false;
	std::vector<MaterialBatch> m_materialBatches;
};

using MGTerrainTilePtr = std::shared_ptr<MGTerrainTile>;
