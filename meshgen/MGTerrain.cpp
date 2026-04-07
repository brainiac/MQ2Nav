//
// MGTerrain.cpp
//

#include "pch.h"
#include "meshgen/MGTerrain.h"

#include "eqglib/eqg_material.h"
#include "mq/base/Color.h"

#include "spdlog/spdlog.h"

#include <map>

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
	vertices.resize(m_vertices.size());

	for (size_t i = 0; i < m_vertices.size(); ++i)
	{
		StaticMeshVertex& v = vertices[i];
		v.position = m_vertices[i];
		v.normal = m_normals[i];
		v.uv = m_uvs[i];

		// This needs to be updated with inputs from material and global ambient
		v.colorDiffuse = mq::MQColor(m_rgbColors[i]).ToABGR();
		vertices.push_back(v);
	}

	struct FacesByMaterial
	{
		eqg::Material* material;
		std::vector<uint32_t> faces;
	};

	// Group faces by material index for batched rendering
	std::vector<FacesByMaterial> facesByMaterial;
	facesByMaterial.resize(m_materialPalette->GetNumMaterials() + 1);

	for (uint32_t i = 0; i < m_materialPalette->GetNumMaterials(); ++i)
	{
		facesByMaterial[i].material = m_materialPalette->GetMaterial(i);
	}

	for (const auto& face : m_faces)
	{
		uint16_t materialIndex = face.materialIndex;
		if (materialIndex == 0xffff)
			materialIndex = static_cast<uint16_t>(m_materialPalette->GetNumMaterials());

		facesByMaterial[materialIndex].faces.push_back(face.indices.x);
		facesByMaterial[materialIndex].faces.push_back(face.indices.y);
		facesByMaterial[materialIndex].faces.push_back(face.indices.z);
	}

	// Build index buffer in material order and create batches
	std::vector<uint32_t> indices;
	indices.reserve(m_faces.size() * 3);

	std::vector<MaterialBatch> materialBatches;
	materialBatches.reserve(facesByMaterial.size());

	for (FacesByMaterial& materialFaces : facesByMaterial)
	{
		MaterialBatch batch;
		batch.startIndex = static_cast<uint32_t>(indices.size());
		batch.indexCount = static_cast<uint32_t>(materialFaces.faces.size());
		batch.material = materialFaces.material;
		if (batch.material)
		{
			batch.isAlphaBlend = materialFaces.material->m_renderMaterial == eqg::RenderMaterial_AlphaBatch
				|| materialFaces.material->m_renderMaterial == eqg::RenderMaterial_AlphaBatchAdditive;
		}


		indices.insert(indices.end(), materialFaces.faces.begin(), materialFaces.faces.end());
		materialBatches.push_back(batch);
	}

	// draw alpha blended batches after everything else.
	std::sort(materialBatches.begin(), materialBatches.end(),
		[&](const auto& a, const auto& b) { return a.isAlphaBlend < b.isAlphaBlend; });

	// Create bgfx buffers
	m_vertexBuffer = bgfx::createVertexBuffer(
		bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(StaticMeshVertex))),
		StaticMeshVertex::ms_layout);

	m_indexBuffer = bgfx::createIndexBuffer(
		bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t))),
		BGFX_BUFFER_INDEX32);

	m_materialBatches = std::move(materialBatches);
	m_indexCount = static_cast<uint32_t>(indices.size());
	m_gpuBuffersBuilt = true;

	SPDLOG_INFO("MGTerrain::BuildGPUBuffers: Built buffers ({} verts, {} indices, {} batches)",
		vertices.size(), indices.size(), m_materialBatches.size());

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

