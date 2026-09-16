//
// MGSimpleModel.cpp
//

#include "pch.h"
#include "MGSimpleModel.h"

#include "eqglib/eqg_material.h"
#include "mq/base/Color.h"

#include "spdlog/spdlog.h"

#include <map>
#include <unordered_map>

namespace
{
	// Buffers are shared by every instance placed from one definition, and released once
	// the last of those instances is gone.
	//
	// The map is keyed by raw definition pointer, which is only a hint: loading another
	// zone frees the previous zone's definitions and the allocator readily hands the same
	// addresses back out, so an entry found by address may belong to a definition that no
	// longer exists. Each entry therefore also holds a weak reference to the definition it
	// was built from, and a lookup is only trusted when that still resolves to the same
	// object. Without the check a model can be drawn with a completely different model's
	// geometry.
	struct SharedBufferEntry
	{
		std::weak_ptr<eqg::SimpleModelDefinition> definition;
		std::weak_ptr<SharedModelBuffers> buffers;
	};

	std::unordered_map<const eqg::SimpleModelDefinition*, SharedBufferEntry> s_sharedBuffers;
}

SharedModelBuffers::~SharedModelBuffers()
{
	if (bgfx::isValid(vertexBuffer))
		bgfx::destroy(vertexBuffer);

	if (bgfx::isValid(indexBuffer))
		bgfx::destroy(indexBuffer);
}

MGSimpleModel::MGSimpleModel()
{
}

MGSimpleModel::~MGSimpleModel()
{
	DestroyGPUBuffers();
}

const std::vector<MaterialBatch>& MGSimpleModel::GetMaterialBatches() const
{
	static const std::vector<MaterialBatch> empty;

	return m_shared ? m_shared->materialBatches : empty;
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

	// Every instance of a definition produces byte-identical geometry, so build it once
	// and share it.
	auto cached = s_sharedBuffers.find(def.get());
	if (cached != s_sharedBuffers.end())
	{
		if (cached->second.definition.lock() == def)
		{
			if ((m_shared = cached->second.buffers.lock()))
				return true;
		}

		// Either the buffers are gone or the address now belongs to a different
		// definition; in both cases the entry is worthless.
		s_sharedBuffers.erase(cached);
	}

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
	std::vector<MaterialBatch> materialBatches;

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
		materialBatches.push_back(batch);
	}

	if (vertices.empty() || indices.empty())
	{
		SPDLOG_DEBUG("MGSimpleModel::BuildGPUBuffers: No valid geometry for '{}'", def->m_tag);
		return false;
	}

	// Create bgfx buffers
	auto shared = std::make_shared<SharedModelBuffers>();

	shared->vertexBuffer = bgfx::createVertexBuffer(
		bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(StaticMeshVertex))),
		StaticMeshVertex::GetLayout());

	shared->indexBuffer = bgfx::createIndexBuffer(
		bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t))),
		BGFX_BUFFER_INDEX32);

	if (!bgfx::isValid(shared->vertexBuffer) || !bgfx::isValid(shared->indexBuffer))
	{
		SPDLOG_ERROR("MGSimpleModel::BuildGPUBuffers: failed to create buffers for '{}' - bgfx static "
			"buffer handles are exhausted", def->m_tag);
		return false;
	}

	shared->indexCount = static_cast<uint32_t>(indices.size());
	shared->materialBatches = std::move(materialBatches);

	m_shared = shared;
	s_sharedBuffers[def.get()] = SharedBufferEntry{ def, shared };

	SPDLOG_TRACE("MGSimpleModel::BuildGPUBuffers: Built buffers for '{}' ({} verts, {} indices, {} batches)",
		def->m_tag, vertices.size(), indices.size(), m_shared->materialBatches.size());

	return true;
}

void MGSimpleModel::DestroyGPUBuffers()
{
	// The buffers belong to the definition and are destroyed once the last instance
	// referencing them is gone.
	m_shared.reset();
	m_gpuBuffersBuilt = false;
}

