//
// MGSimpleModel.cpp
//

#include "pch.h"
#include "MGSimpleModel.h"

#include "eqglib/eqg_material.h"
#include "mq/base/Color.h"

#include "spdlog/spdlog.h"

#include <map>

MGSimpleModel::MGSimpleModel()
{
}

MGSimpleModel::~MGSimpleModel()
{
	DestroyGPUBuffers();
}

bool MGSimpleModel::InitBatchInstances()
{
	return true;
}

bool MGSimpleModel::BuildGPUBuffers()
{
	if (m_gpuBuffersBuilt)
		return true;

	m_gpuBuffersBuilt = true;

	if (!m_definition)
	{
		SPDLOG_WARN("MGSimpleModel::BuildGPUBuffers: No definition set");
		return false;
	}

	const auto& def = m_definition;

	if (def->m_vertices.empty() || def->m_faces.empty())
	{
		//SPDLOG_DEBUG("MGSimpleModel::BuildGPUBuffers: Empty geometry for '{}'", def->m_tag);
		return false;
	}

	// Build vertex buffer
	std::vector<StaticMeshVertex> vertices;
	vertices.reserve(def->m_vertices.size());

	bool hasUVs = !def->m_uvs.empty();
	bool hasNormals = !def->m_normals.empty();
	bool hasTint = !def->m_colorTint.empty();

	for (size_t i = 0; i < def->m_vertices.size(); ++i)
	{
		StaticMeshVertex v;
		v.position = def->m_vertices[i];
		v.normal = hasNormals ? def->m_normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
		v.uv = hasUVs ? def->m_uvs[i] : glm::vec2(0.0f, 0.0f);

		// Simple model doesn't use vertex coloring
		v.colorDiffuse = 0xFF000000;

		if (hasTint)
		{
			v.colorTint = mq::MQColor(def->m_colorTint[i]).ToABGR();
		}
		else
		{
			v.colorTint = 0xFFFFFFFF;
		}

		vertices.push_back(v);
	}

	// Group faces by material index for batched rendering
	std::map<int16_t, std::vector<uint32_t>> facesByMaterial;
	for (const auto& face : def->m_faces)
	{
		facesByMaterial[face.materialIndex].push_back(face.indices.x);
		facesByMaterial[face.materialIndex].push_back(face.indices.y);
		facesByMaterial[face.materialIndex].push_back(face.indices.z);
	}

	// Build index buffer in material order and create batches
	std::vector<uint32_t> indices;
	indices.reserve(def->m_faces.size() * 3);
	m_materialBatches.clear();

	eqg::MaterialPalette* palette = def->m_materialPalette.get();

	for (auto& [matIndex, matIndices] : facesByMaterial)
	{
		MaterialBatch batch;
		batch.startIndex = static_cast<uint32_t>(indices.size());
		batch.indexCount = static_cast<uint32_t>(matIndices.size());
		
		if (palette && matIndex >= 0 && matIndex < static_cast<int16_t>(palette->GetNumMaterials()))
		{
			batch.material = palette->GetMaterial(matIndex);
			if (batch.material)
			{
				batch.isAlphaBlend = batch.material->IsAlphaBlend()
					|| batch.material->IsAdditiveAlpha();
				batch.isTint = batch.material->m_hasVertexTint;
			}
		}

		indices.insert(indices.end(), matIndices.begin(), matIndices.end());
		m_materialBatches.push_back(batch);
	}

	if (vertices.empty() || indices.empty())
	{
		SPDLOG_DEBUG("MGSimpleModel::BuildGPUBuffers: No valid geometry for '{}'", def->m_tag);
		return false;
	}

	// Create bgfx buffers
	m_vertexBuffer = bgfx::createVertexBuffer(
		bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(StaticMeshVertex))),
		StaticMeshVertex::GetLayout());

	m_indexBuffer = bgfx::createIndexBuffer(
		bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t))),
		BGFX_BUFFER_INDEX32);

	m_indexCount = static_cast<uint32_t>(indices.size());
	m_gpuBuffersBuilt = true;

	SPDLOG_TRACE("MGSimpleModel::BuildGPUBuffers: Built buffers for '{}' ({} verts, {} indices, {} batches)",
		def->m_tag, vertices.size(), indices.size(), m_materialBatches.size());

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

