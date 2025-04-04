#include "pch.h"
#include "eqg_geometry.h"

#include "eqg_loader.h"
#include "eqg_material.h"
#include "log_internal.h"
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

bool SimpleModelDefinition::InitFromEQMData(std::string_view tag,
	uint32_t numVertices, SEQMVertex* vertices,
	uint32_t numFaces, SEQMFace* faces,
	uint32_t numPoints, SParticlePoint* points,
	uint32_t numParticles, SActorParticle* particles,
	const MaterialPalettePtr& materialPalette)
{
	m_tag = std::string(tag);
	m_materialPalette = materialPalette;
	m_useLitBatches = false;
	m_numFrames = 1;

	for (uint32_t materialNum = 0; materialNum < m_materialPalette->GetNumMaterials(); ++materialNum)
	{
		Material* material = m_materialPalette->GetMaterial(materialNum);
		if (material && material->HasBumpMap())
		{
			m_tangents.resize(numVertices);
			m_binormals.resize(numVertices);
			break;
		}
	}

	InitVerticesFromEQMData(numVertices, vertices);
	InitFacesFromEQMData(numFaces, faces);

	InitCollisionData(false);

	if (numPoints > 0)
	{
		m_pointManager = std::make_unique<ParticlePointDefinitionManager>(numPoints, points, this);

		if (numParticles > 0)
		{
			m_particleManager = std::make_unique<ActorParticleDefinitionManager>(numParticles, particles, this);
		}
	}

	return true;
}

bool SimpleModelDefinition::InitStaticData()
{
	// TODO
	return true;
}

void SimpleModelDefinition::InitVerticesFromEQMData(uint32_t numVertices, SEQMVertex* vertices)
{
	m_numVertices = numVertices;
	m_vertices.resize(numVertices);
	m_uvs.resize(numVertices);
	m_uv2s.resize(numVertices);
	m_colors.resize(numVertices);
	m_colorTint.resize(numVertices);
	m_normals.resize(numVertices);

	if (m_numVertices > 0)
	{
		glm::vec3 maxPos = glm::vec3(std::numeric_limits<float>::min());
		glm::vec3 minPos = glm::vec3(std::numeric_limits<float>::max());
		for (uint32_t i = 0; i < m_numVertices; ++i)
		{
			m_vertices[i] = vertices[i].pos;
			m_uvs[i] = vertices[i].uv;
			m_uv2s[i] = vertices[i].uv2;
			m_colors[i] = 0xffffffff;
			m_colorTint[i] = vertices[i].color;
			m_normals[i] = vertices[i].normal;

			maxPos = glm::max(m_vertices[i], maxPos);
			minPos = glm::min(m_vertices[i], minPos);
		}

		m_aabbMin = minPos;
		m_aabbMax = maxPos;

		glm::vec3 extent = (maxPos - minPos) / 2.0f;
		m_centerOffset = minPos + extent;
		m_boundingRadius = glm::length(extent);
	}
}

void SimpleModelDefinition::InitFacesFromEQMData(uint32_t numFaces, SEQMFace* faces)
{
	m_numFaces = numFaces;
	m_faces.resize(numFaces);
	m_faceNormals.resize(numFaces);

	for (uint32_t i = 0; i < m_numFaces; ++i)
	{
		m_faces[i].indices = { faces[i].vertices[0], faces[i].vertices[1], faces[i].vertices[2] };
		m_faces[i].materialIndex = (int16_t)static_cast<int>(faces[i].material);
		m_faces[i].flags = faces[i].flags & 0xffff;

		glm::vec3 e1 = m_vertices[m_faces[i].indices[1]] - m_vertices[m_faces[i].indices[2]];
		glm::vec3 e2 = m_vertices[m_faces[i].indices[0]] - m_vertices[m_faces[i].indices[1]];
		m_faceNormals[i] = glm::normalize(glm::cross(e1, e2));
	}

	if (!m_tangents.empty())
	{
		// TODO: Build tangents/binormals
	}
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

bool SimpleModel::Init(const SimpleModelDefinitionPtr& definition)
{
	m_definition = definition;

	if (definition->GetMaterialPalette())
	{
		m_materialPalette = definition->GetMaterialPalette()->Clone();
	}

	// TODO: Init point manager

	// TODO: Init particle manager

	return true;
}

bool SimpleModel::SetRGBs(SDMRGBTrackWLDData* pDMRGBTrackWLDData)
{
	m_bakedDiffuseLighting.resize(pDMRGBTrackWLDData->numRGBs);
	memcpy(m_bakedDiffuseLighting.data(), pDMRGBTrackWLDData->RGBs, pDMRGBTrackWLDData->numRGBs * sizeof(uint32_t));

	return true;
}

bool SimpleModel::SetRGBs(uint32_t* pRGBs, uint32_t numRGBs)
{
	if (numRGBs != m_definition->m_numVertices)
	{
		EQG_LOG_WARN("Invalid number of RGB vertices. Has {}, expected {}. tag={}", numRGBs, m_definition->m_numVertices,
			m_definition->GetTag());
		return false;
	}

	m_bakedDiffuseLighting.resize(numRGBs);
	memcpy(m_bakedDiffuseLighting.data(), pRGBs, numRGBs * sizeof(uint32_t));
	return true;
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

const BoneDefinition* HierarchicalModelDefinition::GetBoneDefinition(uint32_t boneIndex) const
{
	if (boneIndex < m_numBones)
		return &m_bones[boneIndex];
	return nullptr;
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

ActorDefinition::ActorDefinition(std::string_view tag, const ParticleCloudDefinitionPtr& particleCloudDef)
	: Resource(GetStaticResourceType())
	, m_tag(tag)
	, m_particleCloudDefinition(particleCloudDef)
{
}

ActorDefinition::~ActorDefinition()
{
}

void ActorDefinition::SetCallbackTag(std::string_view tag)
{
	m_callbackTag = tag;

	if (tag.empty())
	{
		m_callbackType = UnusedCallback;
	}
	else
	{
		switch (tag[0])
		{
		case 'S':
			if (tag.size() == 15 && tag[14] == '2')
			{
				m_callbackType = SpriteCallback2;
			}
			else
			{
				m_callbackType = SpriteCallback;
			}
			break;
		case 'L':
			m_callbackType = LadderCallback;
			break;
		case 'T':
			m_callbackType = TreeCallback;
			break;

		default:
			m_callbackType = UnusedCallback;
			break;
		}
	}
}

void ActorDefinition::SetCallbackType(CallbackTagType type)
{
	m_callbackType = type;
}

float ActorDefinition::CalculateBoundingRadius()
{
	if (m_simpleModelDefinition)
	{
		m_boundingRadius = m_simpleModelDefinition->GetDefaultBoundingRadius();
	}
	else if (m_hierarchicalModelDefinition)
	{
		m_boundingRadius = m_hierarchicalModelDefinition->GetDefaultBoundingRadius();
	}
	else
	{
		m_boundingRadius = 1.0f;
	}

	return m_boundingRadius;
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

Actor::Actor(ResourceManager* resourceMgr)
	: m_resourceMgr(resourceMgr)
{
}

Actor::~Actor()
{
}

static float SetActorBoundingRadius(Actor* actor, float multiplier, float radius)
{
	float boundingRadius = actor->GetDefinition()->CalculateBoundingRadius();

	boundingRadius = std::max(radius, boundingRadius);

	if (multiplier > 1.0f)
	{
		boundingRadius *= multiplier;
	}

	actor->SetBoundingRadius(boundingRadius, false);

	return boundingRadius;
}


void Actor::DoInitCallback()
{
	switch (m_definition->GetCallbackType())
	{
	case SpriteCallback:
		{
			float boundingRadius = SetActorBoundingRadius(this, 1.0f, 1.0f);

			if (starts_with(m_definition->GetTag(), "LADDER")
				|| starts_with(m_definition->GetTag(), "OBJ_LADDER"))
			{
				SetActorType(eActorTypeLadder);
			}
			else if (starts_with(m_definition->GetTag(), "ISLANDPALM"))
			{
				SetActorType(eActorTypeTree);
			}
			else if (boundingRadius > 80.0f)
			{
				SetActorType(eActorTypeLargeObject);
			}
			else
			{
				SetActorType(eActorTypeObject);
			}
		}
		break;

	case SpriteCallback2:
		{
			float boundingRadius = SetActorBoundingRadius(this, 1.0f, 1.0f);

			if (boundingRadius > 80.0f)
			{
				SetActorType(eActorTypeLargeObject);
			}
			else
			{
				SetActorType(eActorTypeObject);
			}
		}
		break;

	case LadderCallback:
		SetActorBoundingRadius(this, 1.0f, 1.0f);
		SetActorType(eActorTypeLadder);
		break;

	case TreeCallback:
		SetActorBoundingRadius(this, 1.0f, 1.0f);
		SetActorType(eActorTypeTree);
		break;

	default: break;
	}
}

void Actor::SetBoundingRadius(float radius, bool adjustScale)
{
	if (adjustScale)
	{
		m_scale = m_boundingRadius == 0 ? radius : m_scale * (radius / m_boundingRadius);
	}

	m_boundingRadius = radius;
}

//-------------------------------------------------------------------------------------------------

SimpleActor::SimpleActor(
	ResourceManager* resourceMgr,
	std::string_view actorTag,
	const ActorDefinitionPtr& actorDef,
	const glm::vec3& position,
	const glm::vec3& orientation,
	float scaleFactor,
	float boundingRadius,
	ECollisionVolumeType collisionVolumeType,
	int actorIndex,
	SDMRGBTrackWLDData* DMRGBTrackWLDData,
	uint32_t* RGBs,
	uint32_t numRGBs,
	std::string_view actorName)
	: Actor(resourceMgr)
{
	m_tag = std::string(actorTag);
	m_actorName = std::string(actorName);
	m_actorIndex = actorIndex;
	m_definition = actorDef;

	std::string_view defTag = m_definition->GetTag();
	if (defTag.ends_with("_ACTORDEF"))
	{
		defTag = defTag.substr(0, defTag.size() - 9);
	}

	// Load LOD Data
	if (LODListPtr pLODList = m_resourceMgr->Get<LODList>(fmt::format("{}_LODLIST", defTag));
		pLODList && !pLODList->GetElements().empty())
	{
		// Note: LOD Min distance for model comes from Resources/moddat.ini, using `defTag`

		m_lodModels.reserve(pLODList->GetElements().size());
		for (const LODListElementPtr& element : pLODList->GetElements())
		{
			std::string definitionTag = fmt::format("{}_SMD", element->definition);

			if (SimpleModelDefinitionPtr pModelDef = m_resourceMgr->Get<SimpleModelDefinition>(definitionTag))
			{
				SimpleModelPtr pModel = m_resourceMgr->CreateSimpleModel();
				pModel->Init(pModelDef);
				pModel->SetActor(this);

				m_lodModels.emplace_back(pModel, element->max_distance);
			}
			else
			{
				EQG_LOG_ERROR("Failed to load LOD'ed model {} from LOD List for {}", definitionTag, defTag);
			}
		}

		if (const LODListElementPtr& element = pLODList->GetCollision())
		{
			std::string definitionTag = fmt::format("{}_SMD", element->definition);

			if (SimpleModelDefinitionPtr pModelDef = m_resourceMgr->Get<SimpleModelDefinition>(definitionTag))
			{
				SimpleModelPtr pModel = m_resourceMgr->CreateSimpleModel();
				pModel->Init(pModelDef);
				pModel->SetActor(this);

				m_collisionModel = pModel;
			}
			else
			{
				EQG_LOG_ERROR("Failed to load collision model {} from LOD List for {}", definitionTag, defTag);
			}
		}

		if (!m_collisionModel)
		{
			m_collisionModel = m_lodModels[0].model;
		}
	}
	else
	{
		m_model = std::make_shared<SimpleModel>();

		m_model->Init(m_definition->GetSimpleModelDefinition());
		m_model->SetActor(this);
		m_collisionModel = m_model;
	}

	SetPosition(position);
	SetOrientation(orientation);
	SetScale(scaleFactor);
	SetBoundingRadius(boundingRadius);
	SetCollisionVolumeType(collisionVolumeType);
	
	if (DMRGBTrackWLDData != nullptr)
	{
		m_model->SetRGBs(DMRGBTrackWLDData);
	}

	if (RGBs != nullptr)
	{
		m_model->SetRGBs(RGBs, numRGBs);
	}

	DoInitCallback();

	m_resourceMgr->AddActor(this);
}

SimpleActor::~SimpleActor()
{
	m_resourceMgr->RemoveActor(this);
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
	: Actor(resourceMgr)
{
	m_tag = std::string(actorTag);
	m_actorName = std::string(actorName);
	m_actorIndex = actorIndex;
	m_definition = actorDef;

	m_model = std::make_shared<HierarchicalModel>();

	// TODO: Init the actor


	m_resourceMgr->AddActor(this);
}

HierarchicalActor::~HierarchicalActor()
{
	m_resourceMgr->RemoveActor(this);
}

//-------------------------------------------------------------------------------------------------

} // namespace eqg
