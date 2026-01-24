//
// MGTerrain.cpp
//

#include "pch.h"
#include "meshgen/MGTerrain.h"

#include "spdlog/spdlog.h"

MGTerrain::MGTerrain()
{
}

MGTerrain::~MGTerrain()
{
	DestroyGPUBuffers();
}

bool MGTerrain::BuildGPUBuffers()
{
	if (m_gpuBuffersBuilt)
		return true;

	if (m_vertices.empty() || m_faces.empty())
	{
		SPDLOG_DEBUG("MGTerrain::BuildGPUBuffers: Empty geometry");
		return false;
	}

	// Build vertex buffer
	std::vector<StaticMeshVertex> vertices;
	vertices.reserve(m_vertices.size());

	bool hasUVs = !m_uvs.empty();
	bool hasNormals = !m_normals.empty();
	bool hasColors = !m_rgbColors.empty();

	for (size_t i = 0; i < m_vertices.size(); ++i)
	{
		StaticMeshVertex v;
		v.position = m_vertices[i];
		v.normal = hasNormals ? m_normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
		v.uv = hasUVs ? m_uvs[i] : glm::vec2(0.0f, 0.0f);

		// Convert color to ABGR format
		if (hasColors)
		{
			v.color = m_rgbColors[i];
		}
		else
		{
			v.color = 0xFFFFFFFF;  // White, full alpha
		}

		vertices.push_back(v);
	}

	// Build index buffer from faces
	std::vector<uint32_t> indices;
	indices.reserve(m_faces.size() * 3);

	for (const auto& face : m_faces)
	{
		indices.push_back(face.indices.x);
		indices.push_back(face.indices.y);
		indices.push_back(face.indices.z);
	}

	if (vertices.empty() || indices.empty())
	{
		SPDLOG_DEBUG("MGTerrain::BuildGPUBuffers: No valid geometry");
		return false;
	}

	// Create bgfx buffers
	m_vertexBuffer = bgfx::createVertexBuffer(
		bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(StaticMeshVertex))),
		StaticMeshVertex::ms_layout);

	m_indexBuffer = bgfx::createIndexBuffer(
		bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t))),
		BGFX_BUFFER_INDEX32);

	m_indexCount = static_cast<uint32_t>(indices.size());
	m_gpuBuffersBuilt = true;

	SPDLOG_INFO("MGTerrain::BuildGPUBuffers: Built buffers ({} verts, {} indices)",
		vertices.size(), indices.size());

	return true;
}

void MGTerrain::DestroyGPUBuffers()
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
	m_gpuBuffersBuilt = false;
}

