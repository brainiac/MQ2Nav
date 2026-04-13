//
// MGHierarchicalModel.cpp
//

#include "pch.h"
#include "MGHierarchicalModel.h"

#include "eqglib/eqg_material.h"

#include "mq/base/Color.h"
#include "spdlog/spdlog.h"

#include <map>

MGHierarchicalModel::MGHierarchicalModel()
{
}

MGHierarchicalModel::~MGHierarchicalModel()
{
	DestroyGPUBuffers();
}

bool MGHierarchicalModel::BuildGPUBuffers()
{
	if (m_gpuBuffersBuilt)
		return true;
	m_gpuBuffersBuilt = true;

	auto definition = GetDefinition();
	if (!definition)
	{
		SPDLOG_WARN("MGHierarchicalModel::BuildGPUBuffers: No definition set");
		return false;
	}

	uint32_t numSkins = definition->GetNumAttachedSkins();

	// Determine which skins to render (only the default active set)
	uint32_t firstSkin = definition->GetFirstDefaultActiveSkin();
	uint32_t numActiveSkins = definition->GetNumDefaultActiveSkins();

	if (numActiveSkins == 0)
	{
		// If no default active skins defined, render all skins
		firstSkin = 0;
		numActiveSkins = numSkins;
	}

	// Clamp range to valid bounds
	if (firstSkin >= numSkins)
		firstSkin = 0;
	if (firstSkin + numActiveSkins > numSkins)
		numActiveSkins = numSkins - firstSkin;

	const auto& bones = definition->GetBones();
	uint32_t numBones = definition->GetNumBones();

	// Accumulate all skin meshes into a single vertex/index buffer
	std::vector<StaticMeshVertex> allVertices;
	std::vector<uint32_t> allIndices;
	m_materialBatches.clear();

	eqg::MaterialPalette* palette = definition->GetMaterialPalette().get();

	for (uint32_t skinIdx = firstSkin; skinIdx < firstSkin + numActiveSkins; ++skinIdx)
	{
		eqg::SSkinMesh* skin = definition->GetAttachedSkin(skinIdx);
		if (!skin || skin->vertices.empty() || skin->indices.empty())
			continue;

		uint32_t vertexOffset = static_cast<uint32_t>(allVertices.size());
		uint32_t numVerts = static_cast<uint32_t>(skin->vertices.size());

		bool hasUVs = !skin->uvs.empty();
		bool hasNormals = !skin->normals.empty();
		bool hasColors = !skin->colors.empty();

		// Perform CPU skinning using default pose bone matrices.
		// For each vertex, accumulate weighted bone transforms.
		std::vector<glm::vec3> skinnedPositions(numVerts, glm::vec3(0.0f));
		std::vector<glm::vec3> skinnedNormals(numVerts, glm::vec3(0.0f));
		std::vector<float> totalWeights(numVerts, 0.0f);

		for (uint32_t boneIdx = 0; boneIdx < skin->skinInfo.size() && boneIdx < numBones; ++boneIdx)
		{
			const auto& info = skin->skinInfo[boneIdx];
			if (info.verts.empty())
				continue;

			// The skinning transform: defaultPoseMtx * offsetMatrix
			// defaultPoseMtx: accumulated world-space default pose for this bone
			// offsetMatrix: inverse bind-pose (transforms from model space to bone-local space)
			const glm::mat4x4& defaultPoseMtx = bones[boneIdx].GetDefaultPoseMatrix();
			glm::mat4x4 skinTransform = defaultPoseMtx * info.offsetMatrix;
			glm::mat3 normalTransform = glm::mat3(skinTransform);

			for (size_t i = 0; i < info.verts.size(); ++i)
			{
				uint32_t vertIdx = info.verts[i];
				if (vertIdx >= numVerts)
					continue;

				float weight = info.weights[i];

				glm::vec3 srcPos = skin->vertices[vertIdx];
				skinnedPositions[vertIdx] += weight * glm::vec3(skinTransform * glm::vec4(srcPos, 1.0f));

				if (hasNormals)
				{
					glm::vec3 srcNorm = skin->normals[vertIdx];
					skinnedNormals[vertIdx] += weight * (normalTransform * srcNorm);
				}

				totalWeights[vertIdx] += weight;
			}
		}

		// Build StaticMeshVertex array from skinned data
		for (uint32_t i = 0; i < numVerts; ++i)
		{
			StaticMeshVertex v;

			if (totalWeights[i] > 0.0f)
			{
				v.position = skinnedPositions[i];
				v.normal = hasNormals ? glm::normalize(skinnedNormals[i]) : glm::vec3(0.0f, 1.0f, 0.0f);
			}
			else
			{
				// Unskinned vertex - use raw position
				v.position = skin->vertices[i];
				v.normal = hasNormals ? skin->normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
			}

			v.uv = hasUVs ? skin->uvs[i] : glm::vec2(0.0f, 0.0f);
			v.colorDiffuse = hasColors ? skin->colors[i] : 0xFFFFFFFF;

			allVertices.push_back(std::move(v));
		}

		// Group faces by material (attributes array stores material index per triangle)
		std::map<uint32_t, std::vector<uint32_t>> facesByMaterial;

		for (size_t i = 0; i + 2 < skin->indices.size(); i += 3)
		{
			uint32_t matIdx = 0;
			uint32_t triIdx = static_cast<uint32_t>(i / 3);
			if (triIdx < skin->attributes.size())
				matIdx = skin->attributes[triIdx];

			// Offset indices by the accumulated vertex count
			facesByMaterial[matIdx].push_back(static_cast<uint32_t>(skin->indices[i + 0]) + vertexOffset);
			facesByMaterial[matIdx].push_back(static_cast<uint32_t>(skin->indices[i + 1]) + vertexOffset);
			facesByMaterial[matIdx].push_back(static_cast<uint32_t>(skin->indices[i + 2]) + vertexOffset);
		}

		// Create material batches
		for (auto& [matIndex, matIndices] : facesByMaterial)
		{
			MaterialBatch batch;
			batch.startIndex = static_cast<uint32_t>(allIndices.size());
			batch.indexCount = static_cast<uint32_t>(matIndices.size());

			if (palette && matIndex < palette->GetNumMaterials())
			{
				batch.material = palette->GetMaterial(matIndex);
			}

			allIndices.insert(allIndices.end(), matIndices.begin(), matIndices.end());
			m_materialBatches.push_back(batch);
		}
	}

	if (allVertices.empty() || allIndices.empty())
	{
		SPDLOG_DEBUG("MGHierarchicalModel::BuildGPUBuffers: No valid geometry for '{}'", definition->m_tag);
		return false;
	}

	// Create bgfx buffers
	m_vertexBuffer = bgfx::createVertexBuffer(
		bgfx::copy(allVertices.data(), static_cast<uint32_t>(allVertices.size() * sizeof(StaticMeshVertex))),
		StaticMeshVertex::GetLayout());

	m_indexBuffer = bgfx::createIndexBuffer(
		bgfx::copy(allIndices.data(), static_cast<uint32_t>(allIndices.size() * sizeof(uint32_t))),
		BGFX_BUFFER_INDEX32);

	m_indexCount = static_cast<uint32_t>(allIndices.size());

	SPDLOG_DEBUG("MGHierarchicalModel::BuildGPUBuffers: Built buffers for '{}' ({} verts, {} indices, {} batches)",
		definition->m_tag, allVertices.size(), allIndices.size(), m_materialBatches.size());

	return true;
}

void MGHierarchicalModel::DestroyGPUBuffers()
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
	m_materialBatches.clear();
}
