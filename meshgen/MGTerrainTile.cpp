//
// MGTerrainTile.cpp
//

#include "pch.h"
#include "meshgen/MGTerrainTile.h"

#include "eqglib/eqg_terrain_loader.h"

#include "spdlog/spdlog.h"

//============================================================================

MGTerrainTile::MGTerrainTile(eqg::TerrainSystem* terrainSystem, eqg::TerrainTile* tile)
	: m_terrainSystem(terrainSystem)
	, m_tile(tile)
{
}

MGTerrainTile::~MGTerrainTile()
{
	DestroyGPUBuffers();
}

const glm::vec3& MGTerrainTile::GetPosition() const
{
	return m_tile->m_tilePos;
}

void MGTerrainTile::DestroyGPUBuffers()
{
	if (bgfx::isValid(m_vertexBuffer))
	{
		bgfx::destroy(m_vertexBuffer);
		m_vertexBuffer = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_indexBuffer))
	{
		bgfx::destroy(m_indexBuffer);
		m_indexBuffer = BGFX_INVALID_HANDLE;
	}

	m_indexCount = 0;
	m_materialBatches.clear();
	m_gpuBuffersBuilt = false;
}

bool MGTerrainTile::BuildGPUBuffers()
{
	if (m_gpuBuffersBuilt)
		return true;

	if (!m_terrainSystem || !m_tile)
		return false;

	uint32_t quads_per_tile = m_terrainSystem->GetQuadsPerTile();
	float units_per_vertex = m_terrainSystem->GetUnitsPerVertex();
	uint32_t quadCount = m_terrainSystem->GetQuadCount();

	std::vector<StaticMeshVertex> vertices;
	std::vector<uint16_t> indices;

	float baseY = m_tile->m_tilePos.x;
	float baseX = m_tile->m_tilePos.y;

	if (m_tile->m_flat)
	{
		// Flat tile - single quad covering entire tile

		float z = m_tile->m_heightField[0];
		float dt = quads_per_tile * units_per_vertex;

		uint32_t color = 0xFFFFFFFF;
		if (!m_tile->m_vertexColor.empty())
			color = m_tile->m_vertexColor[0];

		// Create 4 vertices for the flat tile
		StaticMeshVertex v0, v1, v2, v3;

		v0.position = glm::vec3(baseY, z, baseX);
		v0.normal = glm::vec3(0.0f, 1.0f, 0.0f);
		v0.uv = glm::vec2(0.0f, 0.0f);
		v0.colorDiffuse = color;

		v1.position = glm::vec3(baseY, z, baseX + dt);
		v0.normal = glm::vec3(0.0f, 1.0f, 0.0f);
		v1.uv = glm::vec2(1.0f, 0.0f);
		v1.colorDiffuse = color;

		v2.position = glm::vec3(baseY + dt, z, baseX + dt);
		v0.normal = glm::vec3(0.0f, 1.0f, 0.0f);
		v2.uv = glm::vec2(1.0f, 1.0f);
		v2.colorDiffuse = color;

		v3.position = glm::vec3(baseY + dt, z, baseX);
		v0.normal = glm::vec3(0.0f, 1.0f, 0.0f);
		v3.uv = glm::vec2(0.0f, 1.0f);
		v3.colorDiffuse = color;

		vertices.push_back(v0);
		vertices.push_back(v1);
		vertices.push_back(v2);
		vertices.push_back(v3);

		// Two triangles for the quad
		indices.push_back(0);
		indices.push_back(2);
		indices.push_back(1);

		indices.push_back(2);
		indices.push_back(0);
		indices.push_back(3);
	}
	else
	{
		auto& heightField = m_tile->m_heightField;
		auto& vertexColors = m_tile->m_vertexColor;
		auto& quadFlags = m_tile->m_quadFlags;

		int row_number = -1;

		for (uint32_t quad = 0; quad < quadCount; ++quad)
		{
			if (quad % quads_per_tile == 0)
				++row_number;

			// Skip quads marked as holes
			if (quadFlags[quad] & 0x01)
				continue;

			// Calculate position within tile
			float _x = baseX + (row_number * units_per_vertex);
			float _y = baseY + (quad % quads_per_tile) * units_per_vertex;
			float dt = units_per_vertex;

			// Get heights for quad corners
			float z1 = heightField[quad + row_number];
			float z2 = heightField[quad + row_number + quads_per_tile + 1];
			float z3 = heightField[quad + row_number + quads_per_tile + 2];
			float z4 = heightField[quad + row_number + 1];

			uint32_t c1 = 0xFFFFFFFF;
			uint32_t c2 = 0xFFFFFFFF;
			uint32_t c3 = 0xFFFFFFFF;
			uint32_t c4 = 0xFFFFFFFF;

			if (!vertexColors.empty())
			{
				size_t idx1 = quad + row_number;
				size_t idx2 = quad + row_number + quads_per_tile + 1;
				size_t idx3 = quad + row_number + quads_per_tile + 2;
				size_t idx4 = quad + row_number + 1;

				if (idx1 < vertexColors.size()) c1 = vertexColors[idx1];
				if (idx2 < vertexColors.size()) c2 = vertexColors[idx2];
				if (idx3 < vertexColors.size()) c3 = vertexColors[idx3];
				if (idx4 < vertexColors.size()) c4 = vertexColors[idx4];
			}

			// Calculate normals for the quad
			glm::vec3 p1(_x, z1, _y);
			glm::vec3 p2(_x + dt, z2, _y);
			glm::vec3 p3(_x + dt, z3, _y + dt);
			glm::vec3 p4(_x, z4, _y + dt);

			glm::vec3 edge1 = p2 - p1;
			glm::vec3 edge2 = p4 - p1;
			glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

			uint16_t baseIndex = static_cast<uint16_t>(vertices.size());

			// Create 4 vertices for this quad
			StaticMeshVertex v0, v1, v2, v3;

			v0.position = p1;
			v0.normal = normal;
			v0.uv = glm::vec2(0.0f, 0.0f);
			v0.colorDiffuse = c1;

			v1.position = p2;
			v1.normal = normal;
			v1.uv = glm::vec2(1.0f, 0.0f);
			v1.colorDiffuse = c2;

			v2.position = p3;
			v2.normal = normal;
			v2.uv = glm::vec2(1.0f, 1.0f);
			v2.colorDiffuse = c3;

			v3.position = p4;
			v3.normal = normal;
			v3.uv = glm::vec2(0.0f, 1.0f);
			v3.colorDiffuse = c4;

			vertices.push_back(v0);
			vertices.push_back(v1);
			vertices.push_back(v2);
			vertices.push_back(v3);

			// Two triangles for the quad
			indices.push_back(baseIndex + 0);
			indices.push_back(baseIndex + 2);
			indices.push_back(baseIndex + 1);

			indices.push_back(baseIndex + 2);
			indices.push_back(baseIndex + 0);
			indices.push_back(baseIndex + 3);
		}
	}

	if (vertices.empty() || indices.empty())
	{
		SPDLOG_DEBUG("MGTerrainTile: No geometry generated for tile ({}, {})",
			m_tile->m_tileLoc.x, m_tile->m_tileLoc.y);
		return false;
	}

	// Create vertex buffer
	m_vertexBuffer = bgfx::createVertexBuffer(
		bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(StaticMeshVertex))),
		StaticMeshVertex::GetLayout());

	// Create index buffer
	m_indexBuffer = bgfx::createIndexBuffer(
		bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t))));

	m_indexCount = static_cast<uint32_t>(indices.size());

	// Create a single untextured material batch for the entire tile
	MaterialBatch batch;
	batch.material = nullptr;
	batch.startIndex = 0;
	batch.indexCount = m_indexCount;
	m_materialBatches.push_back(batch);

	m_gpuBuffersBuilt = true;

	SPDLOG_DEBUG("MGTerrainTile: Built GPU buffers for tile ({}, {}): {} vertices, {} indices",
		m_tile->m_tileLoc.x, m_tile->m_tileLoc.y, vertices.size(), indices.size());

	return true;
}
