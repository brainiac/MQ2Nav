//
// MGSimpleModel.cpp
//

#include "pch.h"
#include "meshgen/MGSimpleModel.h"

#include "spdlog/spdlog.h"

MGSimpleModel::MGSimpleModel()
{
}

MGSimpleModel::~MGSimpleModel()
{
	DestroyGPUBuffers();
}

bool MGSimpleModel::BuildGPUBuffers()
{
	if (m_gpuBuffersBuilt)
		return true;

	if (!m_definition)
	{
		SPDLOG_WARN("MGSimpleModel::BuildGPUBuffers: No definition set");
		return false;
	}

	const auto& def = m_definition;

	if (def->m_vertices.empty() || def->m_faces.empty())
	{
		SPDLOG_DEBUG("MGSimpleModel::BuildGPUBuffers: Empty geometry for '{}'", def->m_tag);
		return false;
	}

	// Build vertex buffer
	std::vector<StaticMeshVertex> vertices;
	vertices.reserve(def->m_vertices.size());

	bool hasUVs = !def->m_uvs.empty();
	bool hasNormals = !def->m_normals.empty();
	bool hasColors = !def->m_colors.empty();

	for (size_t i = 0; i < def->m_vertices.size(); ++i)
	{
		StaticMeshVertex v;
		v.position = def->m_vertices[i];
		v.normal = hasNormals ? def->m_normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
		v.uv = hasUVs ? def->m_uvs[i] : glm::vec2(0.0f, 0.0f);

		// Convert color to ABGR format
		if (hasColors)
		{
			v.color = def->m_colors[i];
		}
		else
		{
			v.color = 0xFFFFFFFF;  // White, full alpha
		}

		vertices.push_back(v);
	}

	// Build index buffer from faces
	std::vector<uint32_t> indices;
	indices.reserve(def->m_faces.size() * 3);

	for (const auto& face : def->m_faces)
	{
		indices.push_back(face.indices.x);
		indices.push_back(face.indices.y);
		indices.push_back(face.indices.z);
	}

	if (vertices.empty() || indices.empty())
	{
		SPDLOG_DEBUG("MGSimpleModel::BuildGPUBuffers: No valid geometry for '{}'", def->m_tag);
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

	SPDLOG_DEBUG("MGSimpleModel::BuildGPUBuffers: Built buffers for '{}' ({} verts, {} indices)",
		def->m_tag, vertices.size(), indices.size());

	return true;
}

void MGSimpleModel::DestroyGPUBuffers()
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

