#include "pch.h"
#include "eqg_geometry.h"

#include "eqg_material.h"
#include "str_util.h"
#include "wld_types.h"

namespace eqg {

SimpleModelDefinition::SimpleModelDefinition()
	: Resource(GetStaticResourceType())
{
}

SimpleModelDefinition::~SimpleModelDefinition()
{
}

bool SimpleModelDefinition::InitFromWLDData(std::string_view tag, SDMSpriteDef2WLDData* pWldData)
{
	m_tag = tag;
	m_materialPalette = pWldData->materialPalette;
	m_boundingRadius = pWldData->boundingRadius;
	m_centerOffset = pWldData->centerOffset;
	m_numVertices = pWldData->numVertices;
	m_numFaces = pWldData->numFaces;

	if (pWldData->trackDefinition != nullptr)
	{
		WLD_OBJ_DMTRACKDEFINITION2* trackDef = pWldData->trackDefinition;

		if (trackDef->num_vertices != m_numVertices)
		{
			m_numFrames = trackDef->num_frames;
		}

		m_updateInterval = trackDef->sleep;
	}

	uint32_t numMaterials = pWldData->materialPalette->GetNumMaterials();

	// Set up material groups
	m_materialGroups.resize(numMaterials);
	memset(m_materialGroups.data(), 0, sizeof(SMaterialGroup) * numMaterials);

	uint32_t currFace = 0;
	uint32_t currVert = 0;
	for (uint32_t group = 0; group < pWldData->numFaceMaterialGroups; ++group)
	{
		uint32_t materialIndex = pWldData->faceMaterialGroups[group].material_index;
		if (materialIndex < numMaterials)
		{
			m_materialGroups[materialIndex].faceStart = currFace;
			m_materialGroups[materialIndex].faceCount = pWldData->faceMaterialGroups[group].group_size;
			currFace += pWldData->faceMaterialGroups[group].group_size;

			m_materialGroups[materialIndex].vertexStart = currVert;
			m_materialGroups[materialIndex].vertexCount = pWldData->vertexMaterialGroups[group].group_size;
			currVert += pWldData->vertexMaterialGroups[group].group_size;
		}
	}

	// Initialize vertices
	constexpr float S3D_UV_TO_FLOAT = 1.0f / 256.0f;
	constexpr float S3D_NORM_TO_FLOAT = 1.0f / 127.0f;

	if (m_numVertices > 0)
	{
		glm::vec3 maxPos = glm::vec3(std::numeric_limits<float>::min());
		glm::vec3 minPos = glm::vec3(std::numeric_limits<float>::max());

		m_vertices.resize(m_numVertices * m_numFrames);
		m_uvs.resize(m_numVertices);
		m_normals.resize(m_numVertices);
		m_colors.resize(m_numVertices);

		const glm::vec3 centerOffset = pWldData->centerOffset;
		const float scaleFactor = pWldData->vertexScaleFactor;

		for (uint32_t i = 0; i < m_numVertices; ++i)
		{
			m_vertices[i] = centerOffset + glm::vec3(pWldData->vertices[i]) * scaleFactor;

			maxPos = glm::max(m_vertices[i], maxPos);
			minPos = glm::min(m_vertices[i], minPos);
		}

		if (pWldData->numUVs > 0)
		{
			if (pWldData->uvsUsingOldForm)
			{
				for (uint32_t i = 0; i < m_numVertices; ++i)
				{
					m_uvs[i] = glm::vec2(pWldData->uvsOldForm[i]) * S3D_UV_TO_FLOAT;
				}
			}
			else
			{
				for (uint32_t i = 0; i < m_numVertices; ++i)
				{
					m_uvs[i] = pWldData->uvs[i];
				}
			}
		}
		else
		{
			memset(m_uvs.data(), 0, sizeof(glm::vec2) * pWldData->numUVs);
		}

		if (pWldData->numVertexNormals > 0)
		{
			for (uint32_t i = 0; i < m_numVertices; ++i)
			{
				m_normals[i] = glm::vec3(pWldData->vertexNormals[i]) * S3D_NORM_TO_FLOAT;
			}
		}
		else
		{
			memset(m_normals.data(), 0, sizeof(glm::vec3) * m_numVertices);
		}

		if (pWldData->numRGBs > 0)
		{
			memcpy(m_colors.data(), pWldData->rgbData, sizeof(uint32_t) * m_numVertices);
		}
		else
		{
			memset(m_colors.data(), -1, sizeof(uint32_t) * m_numVertices);
		}

		if (m_numFrames > 1)
		{
			WLD_OBJ_DMTRACKDEFINITION2* trackDef = pWldData->trackDefinition;

			// Vertices follow the header
			glm::i16vec3* trackVertices = (glm::i16vec3*)(trackDef + 1);
			const float scaleFactor = 1.0f / (float)(1 << trackDef->scale);
			const glm::vec3 centerOffset = pWldData->centerOffset;

			for (uint32_t curFrame = 0; curFrame < m_numFrames; ++curFrame)
			{
				uint32_t offset = curFrame * m_numVertices;

				for (uint32_t frameVertex = 0; frameVertex < m_numVertices; ++frameVertex)
				{
					uint32_t vertex = offset + frameVertex;

					m_vertices[vertex] = centerOffset + glm::vec3(trackVertices[vertex]) * scaleFactor;

					maxPos = glm::max(m_vertices[vertex], maxPos);
					minPos = glm::min(m_vertices[vertex], minPos);
				}
			}
		}

		m_aabbMin = minPos;
		m_aabbMax = maxPos;
		glm::vec3 extent = (maxPos - minPos) / 2.0f;
		m_centerOffset = minPos + extent;
		m_boundingRadius = glm::length(extent);
	}

	// Initialize faces
	WLD_MATERIALGROUP* faceGroups = pWldData->faceMaterialGroups;

	m_faces.resize(m_numFaces);
	m_faceNormals.resize(m_numFaces);

	uint32_t curGroup = 0;
	uint32_t groupSize = faceGroups[curGroup].group_size;
	int16_t materialIndex = faceGroups[curGroup].material_index;
	if (materialIndex > (int)numMaterials)
		materialIndex = -1;

	uint32_t groupStart = 0;

	for (uint32_t face = 0; face < m_numFaces; ++face)
	{
		if (face > groupStart + groupSize)
		{
			// Increment to next group
			if (curGroup < pWldData->numFaceMaterialGroups - 1)
				++curGroup;

			groupSize = faceGroups[curGroup].group_size;
			materialIndex = faceGroups[curGroup].material_index;
			if (materialIndex > (int)numMaterials)
				materialIndex = -1;

			groupStart = face;
		}

		m_faces[face].indices = glm::u32vec3(pWldData->faces[face].indices).zyx;
		m_faces[face].materialIndex = materialIndex;

		EQG_FACEFLAGS flags = EQG_FACEFLAG_NONE;
		if (pWldData->faces[face].flags & S3D_FACEFLAG_PASSABLE)
			flags = EQG_FACEFLAG_PASSABLE;

		m_faces[face].flags = flags;

		glm::vec3 e1 = m_vertices[m_faces[face].indices[1]] - m_vertices[m_faces[face].indices[2]];
		glm::vec3 e2 = m_vertices[m_faces[face].indices[0]] - m_vertices[m_faces[face].indices[1]];
		m_faceNormals[face] = glm::normalize(glm::cross(e1, e2));
	}

	// Initialize collision data
	if (ci_starts_with(m_tag, "GLOBE_DMSPRITEDEF") || m_tag == "SKYGLOB_DMSPRITEDEF"
		|| ci_starts_with(m_tag, "BASE02_DMSPRITEDEF"))
	{
		InitCollisionData(true);
	}
	else if (pWldData->noCollision)
	{
		m_hasCollision = false;
		m_defaultCollisionType = eCollisionVolumeNone;
	}
	else
	{
		InitCollisionData(false);
	}

	return true;
}

void SimpleModelDefinition::InitCollisionData(bool forceModel)
{
	bool forceCollision;

	if (forceModel)
	{
		m_defaultCollisionType = eCollisionVolumeModel;
		forceCollision = true;
	}
	else
	{
		if (ci_starts_with(m_tag, "LADDER")
			|| m_tag == "HHCELL_DMSPRITEDEF"
			|| m_tag.starts_with("SBDOOR103_")
			|| m_tag == "SPEARDOWN_DMSPRITEDEF"
			|| m_tag == "PORT1414_DMSPRITEDEF")
		{
			m_defaultCollisionType = eCollisionVolumeBox;
			forceCollision = true;
		}
		else if (ci_starts_with(m_tag, "IT66"))
		{
			m_defaultCollisionType = eCollisionVolumeModel;
			forceCollision = true;
		}
		else
		{
			m_defaultCollisionType = eCollisionVolumeNone;
			forceCollision = false;
		}
	}

	bool hasCollision = false;

	// eqg will create an octree of collidable facets, to improve collision detection. We don't need
	// an octree, so we'll skip all of that work. We might have some limited benefit to creating a
	// index buffer with just the collidable facets but maybe I'll do that later...

	if (forceCollision)
	{
		for (uint32_t i = 0; i < m_numFaces; ++i)
		{
			SFace& face = m_faces[i];

			if (face.IsPassable())
				face.flags = static_cast<EQG_FACEFLAGS>(face.flags & ~EQG_FACEFLAG_PASSABLE);
			else
				hasCollision = true;
		}
	}

	m_hasCollision = hasCollision;
}

//-------------------------------------------------------------------------------------------------

SimpleModel::SimpleModel()
	: m_worldTransform(1.0f)
	, m_currentFrame(0)
	, m_lastUpdate(steady_clock::now())
	, m_nextUpdate(steady_clock::now())
	, m_shadeFactor(1.0f)
{
}

SimpleModel::~SimpleModel()
{
}

void SimpleModel::Init(const SimpleModelDefinitionPtr& definition)
{
	m_definition = definition;

	if (definition->GetMaterialPalette())
	{
		m_materialPalette = definition->GetMaterialPalette()->Clone();
	}
}

//-------------------------------------------------------------------------------------------------

BoneDefinition::BoneDefinition(const SDagWLDData& wldData)
{
	m_tag = wldData.tag;

	// TODO: Some sort of exclusion for particle clouds
	m_attachedActor = wldData.attachedActor;

	m_subBones.reserve(wldData.subDags.size());

	// Combose a matrix from the parameters of the transform.
	SFrameTransform* pTransform = &wldData.track->frameTransforms[0];

	if (m_tag.empty() || m_tag != "IT36_TRACK")
	{
		m_mtx = glm::toMat4(pTransform->rotation);
		m_mtx[3] = glm::vec4(pTransform->pivot, 1.0f);
		m_mtx = glm::scale(m_mtx, glm::vec3(pTransform->scale));
	}
	else
	{
		m_mtx = glm::identity<glm::mat4x4>();
	}

	m_defaultPoseMtx = glm::identity<glm::mat4x4>();
}

void BoneDefinition::AddSubBone(BoneDefinition* subBone)
{
	m_subBones.push_back(subBone);
}

//-------------------------------------------------------------------------------------------------

HierarchicalModelDefinition::HierarchicalModelDefinition()
	: Resource(GetStaticResourceType())
{
}

HierarchicalModelDefinition::~HierarchicalModelDefinition()
{
}

bool HierarchicalModelDefinition::InitFromWLDData(std::string_view tag, SHSpriteDefWLDData* pWldData)
{
	m_tag = tag;
	m_materialPalette = pWldData->materialPalette;
	m_boundingRadius = pWldData->boundingRadius;
	m_centerOffset = pWldData->centerOffset;

	// Load bones
	m_numBones = (uint32_t)pWldData->dags.size();
	m_numSubBones = 0;

	if (m_numBones != 0)
	{
		m_bones.reserve(m_numBones);

		for (uint32_t i = 0; i < m_numBones; ++i)
		{
			m_bones.emplace_back(pWldData->dags[i]);
		}

		for (uint32_t i = 0; i < m_numBones; ++i)
		{
			for (uint32_t j = 0; j < (uint32_t)pWldData->dags[i].subDags.size(); ++j)
			{
				m_bones[i].AddSubBone(&m_bones[pWldData->dags[i].subDags[j] - &pWldData->dags[0]]);
				++m_numSubBones;
			}
		}

		for (uint32_t i = 0; i < m_numBones; ++i)
		{
			if (pWldData->dags[i].track && pWldData->dags[i].track->numFrames > 1)
			{
				// TODO: Create animation
				break;
			}
		}

		// TODO: Attachment points
		// TODO: SPOffsets.ini
	}

	// TODO: Load Skins

	// TODO: Read additional properties from Resources\\moddat.ini

	return true;
}

//-------------------------------------------------------------------------------------------------

ActorDefinition::ActorDefinition(std::string_view tag, const SimpleModelDefinitionPtr& simpleModel)
	: Resource(GetStaticResourceType())
	, m_tag(tag)
	, m_simpleModelDefinition(simpleModel)
{
}

ActorDefinition::ActorDefinition(std::string_view tag, const HierarchicalModelDefinitionPtr& hierchicalModel)
	: Resource(GetStaticResourceType())
	, m_tag(tag)
	, m_hierarchicalModelDefinition(hierchicalModel)
{
}

ActorDefinition::~ActorDefinition()
{
}

//-------------------------------------------------------------------------------------------------

HierarchicalModel::HierarchicalModel()
{
}

HierarchicalModel::~HierarchicalModel()
{
}

void HierarchicalModel::Init(const HierarchicalModelDefinitionPtr& definition)
{
	m_definition = definition;
}

//-------------------------------------------------------------------------------------------------

ActorInstance::ActorInstance()
{
}

ActorInstance::~ActorInstance()
{
}

//-------------------------------------------------------------------------------------------------

SimpleActor::SimpleActor(
	ResourceManager* resourceMgr,
	std::string_view actorTag,
	const ActorDefinitionPtr& actorDef,
	const glm::vec3& position,
	const glm::vec3& orientation,
	float scalFactor,
	float boundingRadius,
	ECollisionVolumeType collisionVolumeType,
	int actorIndex,
	SDMRGBTrackWLDData* DMRGBTrackWLDData,
	uint32_t* RGBs,
	uint32_t numRGBs,
	std::string_view actorName)
{
	m_tag = std::string(actorTag);
	m_actorName = std::string(actorName);
	m_actorIndex = actorIndex;
	m_definition = actorDef;

	// TODO: Get LOD data

	m_model = std::make_shared<SimpleModel>();
}

//-------------------------------------------------------------------------------------------------

HierarchicalActor::HierarchicalActor(
	ResourceManager* resourceMgr,
	std::string_view actorTag,
	ActorDefinitionPtr actorDef,
	const glm::vec3& position,
	const glm::vec3& orientation,
	float boundingRadius,
	float scaleFactor,
	ECollisionVolumeType collisionVolumeType,
	int actorIndex,
	SDMRGBTrackWLDData* DMRGBTrackWLDData,
	uint32_t* RGBs,
	uint32_t numRGBs,
	std::string_view actorName)
{
	m_tag = std::string(actorTag);
	m_actorName = std::string(actorName);
	m_actorIndex = actorIndex;
	m_definition = actorDef;

	m_model = std::make_shared<HierarchicalModel>();
}

//-------------------------------------------------------------------------------------------------

} // namespace eqg
