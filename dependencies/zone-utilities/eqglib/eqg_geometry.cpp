#include "pch.h"
#include "eqg_geometry.h"

#include "eqg_global_data.h"
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

		m_vertices.resize(m_numVertices * m_numFrames);
		m_uvs.resize(m_numVertices);
		m_normals.resize(m_numVertices);
		m_colors.resize(m_numVertices);

		const glm::vec3 centerOffset = pWldData->centerOffset;
		const float scaleFactor = pWldData->vertexScaleFactor;

		for (uint32_t i = 0; i < m_numVertices; ++i)
		{
			m_vertices[i] = centerOffset + glm::vec3(pWldData->vertices[i]) * scaleFactor;

			m_aabb.enclose(m_vertices[i]);
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

					m_aabb.enclose(m_vertices[vertex]);
				}
			}
		}

		glm::vec3 extent =  m_aabb.diagonal() * 0.5f;
		m_centerOffset = m_aabb.min + extent;
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
	std::vector<SEQMVertex>&& vertices,
	std::vector<SEQMFace>&& faces,
	const std::vector<SParticlePoint>& points,
	const std::vector<SActorParticle>& particles,
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
			m_tangents.resize(vertices.size());
			m_binormals.resize(vertices.size());
			break;
		}
	}

	InitVerticesFromEQMData(std::move(vertices));
	InitFacesFromEQMData(std::move(faces));

	InitCollisionData(false);

	if (!points.empty())
	{
		m_pointManager = std::make_unique<ParticlePointDefinitionManager>(points, this);

		if (!particles.empty())
		{
			m_particleManager = std::make_unique<ActorParticleDefinitionManager>(particles, this);
		}
	}

	return true;
}

bool SimpleModelDefinition::InitStaticData()
{
	// TODO: Should create index/vertex buffers here
	return true;
}

bool SimpleModelDefinition::ReleaseStaticData()
{
	// TODO: Should destroy index/vertex buffers here
	return true;
}

void SimpleModelDefinition::InitVerticesFromEQMData(std::vector<SEQMVertex>&& vertices)
{
	m_numVertices = (uint32_t)vertices.size();
	m_vertices.resize(m_numVertices);
	m_uvs.resize(m_numVertices);
	m_uv2s.resize(m_numVertices);
	m_colors.resize(m_numVertices);
	m_colorTint.resize(m_numVertices);
	m_normals.resize(m_numVertices);

	if (m_numVertices > 0)
	{
		m_aabb = aabb{ aabb::init_invalid };

		for (uint32_t i = 0; i < m_numVertices; ++i)
		{
			m_vertices[i] = vertices[i].pos;
			m_uvs[i] = vertices[i].uv;
			m_uv2s[i] = vertices[i].uv2;
			m_colors[i] = 0xffffffff;
			m_colorTint[i] = vertices[i].color;
			m_normals[i] = vertices[i].normal;

			m_aabb.enclose(m_vertices[i]);
		}

		glm::vec3 extent = m_aabb.diagonal() * 0.5f;
		m_centerOffset = m_aabb.min + extent;
		m_boundingRadius = glm::length(extent);
	}
}

void SimpleModelDefinition::InitFacesFromEQMData(std::vector<SEQMFace>&& faces)
{
	m_numFaces = (uint32_t)faces.size();
	m_faces.resize(m_numFaces);
	m_faceNormals.resize(m_numFaces);

	for (uint32_t i = 0; i < m_numFaces; ++i)
	{
		m_faces[i].indices = {faces[i].vertices[0], faces[i].vertices[1], faces[i].vertices[2]};
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
	// an octree, so we'll skip all of that work.
	m_collisionBox = aabb::init_invalid;

	if (forceCollision)
	{
		m_forcedCollision = true;
		hasCollision = true;

		for (uint32_t i = 0; i < m_numFaces; ++i)
		{
			SFace& face = m_faces[i];

			// Clear passable flags so that all faces are collidable
			if (face.IsPassable())
				face.flags = static_cast<EQG_FACEFLAGS>(face.flags & ~EQG_FACEFLAG_PASSABLE);

			m_collisionBox.enclose(m_vertices[face.indices[0]]);
			m_collisionBox.enclose(m_vertices[face.indices[1]]);
			m_collisionBox.enclose(m_vertices[face.indices[2]]);
		}
	}
	else
	{
		m_collidableIndices.reserve(m_numFaces);

		for (uint32_t i = 0; i < m_numFaces; ++i)
		{
			SFace& face = m_faces[i];

			if (face.IsCollidable())
			{
				m_collidableIndices.push_back(i);

				m_collisionBox.enclose(m_vertices[face.indices[0]]);
				m_collisionBox.enclose(m_vertices[face.indices[1]]);
				m_collisionBox.enclose(m_vertices[face.indices[2]]);
			}
		}

		if (!m_collidableIndices.empty())
		{
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

	if (m_definition->GetMaterialPalette())
	{
		m_materialPalette = m_definition->GetMaterialPalette()->Clone();
	}

	if (auto pointMgr = m_definition->GetPointManager())
	{
		m_pointManager = std::make_unique<ParticlePointManager>(pointMgr, this);
	}

	if (auto particleMgr = m_definition->GetParticleManager())
	{
		m_particleManager = std::make_unique<ActorParticleManager>(particleMgr, this);
	}

	return true;
}

ParticlePoint* SimpleModel::GetParticlePoint(int index) const
{
	if (m_pointManager)
	{
		return m_pointManager->GetPoint(index);
	}

	return nullptr;
}

int SimpleModel::GetNumParticlePoints() const
{
	if (m_pointManager)
	{
		return m_pointManager->GetNumPoints();
	}
	return 0;
}

bool SimpleModel::SetRGBs(SDMRGBTrackWLDData* pDMRGBTrackWLDData)
{
	m_bakedDiffuseLighting.resize(pDMRGBTrackWLDData->numRGBs);
	memcpy(m_bakedDiffuseLighting.data(), pDMRGBTrackWLDData->RGBs, pDMRGBTrackWLDData->numRGBs * sizeof(uint32_t));

	return true;
}

bool SimpleModel::SetRGBs(const std::span<uint32_t>& RGBs)
{
	if (RGBs.size() != m_definition->m_numVertices)
	{
		EQG_LOG_WARN("Invalid number of RGB vertices. Has {}, expected {}. tag={}", RGBs.size(),
			m_definition->m_numVertices,
			m_definition->GetTag());
		return false;
	}

	m_bakedDiffuseLighting.resize(RGBs.size());
	memcpy(m_bakedDiffuseLighting.data(), RGBs.data(), RGBs.size_bytes());
	return true;
}

//-------------------------------------------------------------------------------------------------

BoneDefinition::BoneDefinition(const SDagWLDData& wldData)
	: m_tag(wldData.tag)
{
	SFrameTransform* pTransform = &wldData.track->frameTransforms[0];

	if (wldData.attachedActor && wldData.attachedActor->GetParticleCloudDefinition()
		&& pTransform->scale < 0.05f && pTransform->pivot.x == 1.0f && pTransform->pivot.y == 1.0f)
	{
		// don't assign the attached actor def
	}
	else
	{
		m_attachedActorDef = wldData.attachedActor;
	}

	m_subBones.resize(wldData.subDags.size());

	// Combose a matrix from the parameters of the transform.

	if (m_tag != "IT36_TRACK")
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

BoneDefinition::BoneDefinition(const SEQMBone* boneData)
	: m_tag(boneData->name)
{
	if (boneData->num_children > 0)
	{
		m_subBones.resize(boneData->num_children);
	}

	// TODO: Double check matrix math
	m_mtx = glm::scale(glm::translate(glm::identity<glm::mat4x4>(), boneData->pivot), boneData->scale) *
		glm::mat4_cast(boneData->quat);
	m_defaultPoseMtx = glm::inverse(m_mtx);

	if (m_tag == "ROOT_BONE")
	{
		m_mtx = glm::rotate(glm::mat4(1.0f), glm::radians(-90.f), glm::vec3(0, 0, 1.0f)) * m_mtx;
	}
}

void BoneDefinition::AddSubBone(BoneDefinition* subBone)
{
	m_subBones.push_back(subBone);
}

//-------------------------------------------------------------------------------------------------

Bone::Bone(const BoneDefinition* boneDef)
{
	m_definition = boneDef;
	m_currentMtx = &m_positionMtx;
	ResourceManager* resourceMgr = ResourceManager::Get();

	if (m_definition->GetNumSubBones())
	{
		m_subBones.resize(m_definition->GetNumSubBones());
	}

	if (auto pAttachedActorDef = m_definition->GetAttachedActorDef())
	{
		if (auto pSimpleDef = pAttachedActorDef->GetSimpleModelDefinition())
		{
			m_simpleAttachment = resourceMgr->CreateSimpleModel();
			m_simpleAttachment->Init(pSimpleDef);
		}
		else if (auto pHierarchicalDef = pAttachedActorDef->GetHierarchicalModelDefinition())
		{
			m_attachedActor = resourceMgr->CreateHierarchicalActor("", pAttachedActorDef, -1, false, false, true, this);
		}
		else
		{
			m_attachedActor = resourceMgr->CreateParticleActor("", pAttachedActorDef, -1, false, this);
		}
	}

	m_positionMtx = boneDef->GetMatrix();
	m_boneToWorldMtx = m_positionMtx;
	m_attachmentMtx = m_positionMtx;
}

Bone::~Bone()
{
}

Actor* Actor::GetTopLevelActor()
{
	Actor* topActor = nullptr;
	Actor* actor = this;

	while (actor)
	{
		topActor = actor;
		actor = actor->GetParentActor();
	}

	return topActor;
}

void Bone::SetAttachedActor(const ActorPtr& actor)
{
	if (m_attachedActor)
	{
		m_attachedActor->SetParentActor(nullptr);
	}

	m_attachedActor = actor;

	if (m_attachedActor)
	{
		m_attachedActor->SetParentActor(m_parentActor);
	}
}

void Bone::SetParentActor(Actor* actor)
{
	m_parentActor = actor;

	if (m_attachedActor)
	{
		m_attachedActor->SetParentActor(m_parentActor);
	}
}

void Bone::AttachToBone(Bone* otherBone)
{
	m_currentMtx = &otherBone->GetMatrix();
	m_boneAttachedTo = otherBone;
}

void Bone::DetachBone()
{
	m_currentMtx = &m_positionMtx;
	m_boneAttachedTo = nullptr;
}

void Bone::UpdateActorToBoneWorldMatrices(bool updateParticleTime)
{
	if (m_parentActor)
	{
		Actor* topActor = m_parentActor->GetTopLevelActor();

		if (auto pModel = topActor->GetHierarchicalModel())
		{
			if (updateParticleTime)
			{
				// TODO: Update particle update time
			}

			pModel->UpdateBoneToWorldMatrices();
		}
	}
}

//-------------------------------------------------------------------------------------------------

BoneGroup::BoneGroup(int maxBones, int maxAnimations)
	: m_maxBones(maxBones)
	, m_maxAnimations(maxAnimations)
{
}

BoneGroup::~BoneGroup()
{
}

void BoneGroup::AddBone(Bone* bone, bool newBoneNames)
{
	m_bones.push_back(bone);
	// TODO: Implementt me
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

	if (!InitBonesFromWLDData(pWldData))
	{
		EQG_LOG_WARN("Failed to initialize bones from WLD data for model: {}", m_tag);
	}

	if (!InitSkinsFromWLDData(pWldData))
	{
		EQG_LOG_WARN("Failed to initialize skins from WLD data for model: {}", m_tag);
	}

	InitCollisionData();

	if (tag.size() >= 4 && tag[3] == '_')
	{
		std::string_view prefix = tag.substr(0, 3);

		m_disableAttachments = g_dataManager.GetDisableAttachments(prefix);
		m_disableShieldAttachments = g_dataManager.GetDisableShieldAttachments(prefix);
		m_disablePrimaryAttachments = g_dataManager.GetDisablePrimaryAttachments(prefix);
		m_disableSecondaryAttachments = g_dataManager.GetDisableSecondaryAttachments(prefix);
	}

	return true;
}

const BoneDefinition* HierarchicalModelDefinition::GetBoneDefinition(uint32_t boneIndex) const
{
	if (boneIndex < m_numBones)
		return &m_bones[boneIndex];

	return nullptr;
}

bool HierarchicalModelDefinition::HasBoneNamed(std::string_view boneName) const
{
	for (const auto& bone : m_bones)
	{
		if (std::string_view(bone.GetTag()).substr(3) == boneName)
			return true;
	}

	return false;
}

SSkinMesh* HierarchicalModelDefinition::GetAttachedSkin(uint32_t skinIndex) const
{
	if (skinIndex < m_numAttachedSkins)
		return const_cast<SSkinMesh*>(&m_attachedSkins[skinIndex]);

	return nullptr;
}

bool HierarchicalModelDefinition::InitBonesFromWLDData(SHSpriteDefWLDData* pWldData)
{
	// Load bones
	m_numBones = static_cast<uint32_t>(pWldData->dags.size());
	m_numSubBones = 0;

	if (m_numBones != 0)
	{
		m_bones.reserve(m_numBones);

		for (uint32_t i = 0; i < m_numBones; ++i)
		{
			m_bones.emplace_back(pWldData->dags[i]);
		}

		// Link up bones
		for (uint32_t i = 0; i < m_numBones; ++i)
		{
			for (uint32_t j = 0; j < pWldData->dags[i].subDags.size(); ++j)
			{
				m_bones[i].AddSubBone(&m_bones[pWldData->dags[i].subDags[j] - &pWldData->dags[0]]);
				++m_numSubBones;
			}
		}

		// Create the default animation.
		for (uint32_t i = 0; i < m_numBones; ++i)
		{
			if (pWldData->dags[i].track && pWldData->dags[i].track->numFrames > 1)
			{
				auto defaultAnim = std::make_shared<Animation>();
				if (!defaultAnim->InitFromBoneData(m_tag, pWldData->dags))
				{
					EQG_LOG_WARN("Failed to initialize default animation for hierachical model: {}", m_tag);
				}
				else
				{
					m_defaultAnim = defaultAnim;
				}

				break;
			}
		}

		// find attachment points
		std::string_view prefix = m_bones[0].GetTag().substr(0, 3);
		std::vector<SParticlePoint> points;
		points.resize(15);

		// Initialize to defaults
		for (SParticlePoint& point : points)
		{
			point.scale = glm::vec3(1.0f);
			point.position = glm::vec3(0.0f);
			point.orientation = glm::vec3(0.0f);
		}

		uint32_t numPoints = 0;
		points[numPoints].attachment = m_bones[0].GetTag();
		points[numPoints].name = "SPELLPOINT_DEFAULT";
		++numPoints;

		if (HasBoneNamed("CHEST_POINT_TRACK"))
		{
			points[numPoints].attachment = fmt::format("{}{}", prefix, "CHEST_POINT_TRACK");
			points[numPoints].name = "SPELLPOINT_CHEST";
			++numPoints;
		}

		if (HasBoneNamed("CH_TRACK"))
		{
			points[numPoints].attachment = fmt::format("{}{}", prefix, "CH_TRACK");
			points[numPoints].name = "SPELLPOINT_CHEST";
			++numPoints;
		}

		if (HasBoneNamed("HEHEAD_TRACK"))
		{
			points[numPoints].attachment = fmt::format("{}{}", prefix, "HEHEAD_TRACK");
			points[numPoints].name = "SPELLPOINT_HEAD";
			++numPoints;

			points[numPoints].attachment = fmt::format("{}{}", prefix, "HEHEAD_TRACK");
			points[numPoints].name = "SPELLPOINT_MOUTH";
			points[numPoints].position = glm::vec3(0.08f, 0.6f, 0.0f);
			points[numPoints].orientation = glm::vec3(glm::radians(90.f), 0, glm::radians(90.f));
			++numPoints;
		}

		if (HasBoneNamed("HEAD_POINT_TRACK"))
		{
			points[numPoints].attachment = fmt::format("{}{}", prefix, "HEAD_POINT_TRACK");
			points[numPoints].name = "SPELLPOINT_HEAD";
			++numPoints;

			points[numPoints].attachment = fmt::format("{}{}", prefix, "HEAD_POINT_TRACK");
			points[numPoints].name = "SPELLPOINT_MOUTH";
			SpellPointMods mods = g_dataManager.GetSpellPointMods(prefix);
			points[numPoints].position = mods.pos;
			points[numPoints].orientation = mods.rot;
			points[numPoints].scale = mods.scale;

			++numPoints;
		}

		if (HasBoneNamed("L_POINT_TRACK"))
		{
			points[numPoints].attachment = fmt::format("{}{}", prefix, "L_POINT_TRACK");
			points[numPoints].name = "SPELLPOINT_LHAND";
			++numPoints;
		}
		else if (HasBoneNamed("SPELL_POINT_TRACK"))
		{
			points[numPoints].attachment = fmt::format("{}{}", prefix, "SPELL_POINT_TRACK");
			points[numPoints].name = "SPELLPOINT_LHAND";
			++numPoints;
		}

		if (HasBoneNamed("R_POINT_TRACK"))
		{
			points[numPoints].attachment = fmt::format("{}{}", prefix, "R_POINT_TRACK");
			points[numPoints].name = "SPELLPOINT_RHAND";
			++numPoints;
		}
		else if (HasBoneNamed("SPELL_POINT_TRACK"))
		{
			points[numPoints].attachment = fmt::format("{}{}", prefix, "SPELL_POINT_TRACK");
			points[numPoints].name = "SPELLPOINT_RHAND";
			++numPoints;
		}

		if (HasBoneNamed("BOFOOTL_TRACK"))
		{
			points[numPoints].attachment = fmt::format("{}{}", prefix, "BOFOOTL_TRACK");
			points[numPoints].name = "SPELLPOINT_LFOOT";
			++numPoints;
		}
		else if (HasBoneNamed("BO_L_TRACK"))
		{
			points[numPoints].attachment = fmt::format("{}{}", prefix, "BO_L_TRACK");
			points[numPoints].name = "SPELLPOINT_LFOOT";
			++numPoints;
		}

		if (HasBoneNamed("BOFOOTR_TRACK"))
		{
			points[numPoints].attachment = fmt::format("{}{}", prefix, "BOFOOTR_TRACK");
			points[numPoints].name = "SPELLPOINT_RFOOT";
			++numPoints;
		}
		else if (HasBoneNamed("BO_R_TRACK"))
		{
			points[numPoints].attachment = fmt::format("{}{}", prefix, "BO_R_TRACK");
			points[numPoints].name = "SPELLPOINT_RFOOT";
			++numPoints;
		}

		if (numPoints > 0)
		{
			m_pointManager = std::make_unique<ParticlePointDefinitionManager>(points, this);
		}
	}

	return true;
}

bool HierarchicalModelDefinition::InitSkinsFromWLDData(SHSpriteDefWLDData* pWldData)
{
	m_numAttachedSkins = pWldData->numAttachedSkins;

	m_firstDefaultActiveSkin = pWldData->activeSkin;
	m_numDefaultActiveSkins = (uint32_t)pWldData->attachedSkins.size();

	if (m_numAttachedSkins)
	{
		// This is where we generate the skin meshes for the model.
		m_attachedSkins.reserve(m_numAttachedSkins);

		for (uint32_t skin = 0; skin < m_numAttachedSkins; ++skin)
		{
			SDMSpriteDef2WLDData* pDMSpriteDef2 = pWldData->attachedSkins[skin].get();
			if (!pDMSpriteDef2)
				continue;

			SSkinMesh& mesh = m_attachedSkins.emplace_back(pDMSpriteDef2->tag);
			mesh.tag = pDMSpriteDef2->tag;
			mesh.oldModel = true;
			mesh.vertices.resize(pDMSpriteDef2->numVertices);
			mesh.uvs.resize(pDMSpriteDef2->numVertices);
			mesh.normals.resize(pDMSpriteDef2->numVertices);

			// Fill vertices
			const glm::vec3 centerOffset = pDMSpriteDef2->centerOffset;
			const float scaleFactor = pDMSpriteDef2->vertexScaleFactor;

			for (uint32_t vert = 0; vert < pDMSpriteDef2->numVertices; ++vert)
			{
				mesh.vertices[vert] = centerOffset + glm::vec3(pDMSpriteDef2->vertices[vert]) * scaleFactor;

				if (pDMSpriteDef2->numUVs > 0)
				{
					if (pDMSpriteDef2->uvsUsingOldForm)
						mesh.uvs[vert] = glm::vec2(pDMSpriteDef2->uvsOldForm[vert]) * S3D_UV_TO_FLOAT;
					else
						mesh.uvs[vert] = pDMSpriteDef2->uvs[vert];
				}
				else
				{
					mesh.uvs[vert] = glm::vec2(0.0f, 0.0f);
				}

				if (pDMSpriteDef2->numVertexNormals > 0)
				{
					mesh.normals[vert] = glm::vec3(pDMSpriteDef2->vertexNormals[vert]) * S3D_NORM_TO_FLOAT;
				}
				else
				{
					mesh.normals[vert] = glm::vec3(0.0f, 0.0f, 0.0f);
				}
			}

			// Fill indices
			mesh.indices.resize(pDMSpriteDef2->numFaces * 3);
			auto indices = mesh.indices.data();

			uint32_t index = 0;
			for (uint32_t face = 0; face < pDMSpriteDef2->numFaces; ++face)
			{
				indices[index++] = pDMSpriteDef2->faces[face].indices[0];
				indices[index++] = pDMSpriteDef2->faces[face].indices[1];
				indices[index++] = pDMSpriteDef2->faces[face].indices[2];
			}

			// fill material index table for faces
			mesh.materialIndexTable.resize(pDMSpriteDef2->numFaceMaterialGroups);
			auto materialIndices = mesh.materialIndexTable.data();
			mesh.attributes.resize(pDMSpriteDef2->numFaces);
			uint32_t groupStart = 0;

			for (uint32_t groupNum = 0; groupNum < pDMSpriteDef2->numFaceMaterialGroups; ++groupNum)
			{
				auto& pFaceGroup = pDMSpriteDef2->faceMaterialGroups[groupNum];
				uint32_t materialIndex = pFaceGroup.material_index;
				uint32_t groupSize = pFaceGroup.group_size;
				if (materialIndex == 0xffff)
					materialIndex = 0x7fffffff;
				if (materialIndex > pDMSpriteDef2->materialPalette->GetNumMaterials())
					materialIndex = 0x7fffffff;

				materialIndices[groupNum] = materialIndex;

				for (uint32_t i = 0; i < groupSize; ++i)
				{
					mesh.attributes[groupStart + i] = groupNum;
				}

				groupStart += groupSize;
			}

			if (pDMSpriteDef2->numSkinGroups > 0)
			{
				// TODO: Generate mesh from SSkinMesh data
			}
			else
			{
				//mesh.hasBlendIndices = false;
				//mesh.hasBlendWeights = false;
			}

			//mesh.attachPointBoneIndex = pWldData->skeletonDagIndices[skin];

			if (pDMSpriteDef2->numSkinGroups > 0)
			{
				// D3DXBONECOMBINATION stuff...
			}
		}
	}

	return true;
}

bool HierarchicalModelDefinition::InitFromEQMData(
	std::string_view tag,
	const std::vector<SEQMVertex>& vertices,
	const std::vector<SEQMFace>& faces,
	const std::vector<SEQMBone>& bones,
	const std::vector<SParticlePoint>& points,
	const std::vector<SActorParticle>& particles,
	SEQMSkinData* skinData,
	const MaterialPalettePtr& materialPalette)
{
	m_tag = std::string(tag);
	m_materialPalette = materialPalette;
	m_vertexMappings.assign(vertices.size(), 0xffffffff);

	glm::vec3 maxPoint;
	for (const auto& vertex : vertices)
	{
		maxPoint = glm::max(maxPoint, vertex.pos);
	}
	maxPoint.z /= 2.0f;
	m_boundingRadius = glm::length(maxPoint);

	if (tag.size() >= 4 && tag[3] == '_')
	{
		std::string_view prefix = tag.substr(0, 3);

		m_boundingRadius = g_dataManager.GetDefaultBoundingRadius(prefix, m_boundingRadius);
		m_disableAttachments = g_dataManager.GetDisableAttachments(prefix);
		m_disableShieldAttachments = g_dataManager.GetDisableShieldAttachments(prefix);
		m_disablePrimaryAttachments = g_dataManager.GetDisablePrimaryAttachments(prefix);
		m_disableSecondaryAttachments = g_dataManager.GetDisableSecondaryAttachments(prefix);
	}

	InitBonesFromEQMData(bones);
	InitSkinFromEQMData(vertices, faces, skinData);

	InitCollisionData();

	// TODO: Init points
	// TODO: Init particles

	return true;
}

void HierarchicalModelDefinition::InitSkinFromEQMData(
	const std::vector<SEQMVertex>& vertices, const std::vector<SEQMFace>& faces, SEQMSkinData* skinData)
{
	m_numAttachedSkins = 1;
	m_firstDefaultActiveSkin = 0;
	m_numDefaultActiveSkins = 1;
	m_fromEQM = true;

	bool bakedLighting = m_materialPalette->GetMaterial(0)->GetType() == MaterialType_OpaqueCBS1_VSB
		|| m_materialPalette->GetMaterial(0)->GetType() == MaterialType_ChromaCBS1_VSB;

	m_attachedSkins.reserve(m_numAttachedSkins);
	for (uint32_t skinIndex = 0; skinIndex < m_numAttachedSkins; ++skinIndex)
	{
		SSkinMesh& skin = m_attachedSkins.emplace_back(m_tag);
		skin.vertices.resize(vertices.size());
		skin.normals.resize(vertices.size());
		skin.uvs.resize(vertices.size());
		skin.tangents.resize(vertices.size());
		skin.binormals.resize(vertices.size());

		if (bakedLighting)
		{
			skin.colors.resize(vertices.size());
		}
		else
		{
			skin.uvs2.resize(vertices.size());
		}

		for (uint32_t vtxIndex = 0; vtxIndex < vertices.size(); ++vtxIndex)
		{
			skin.vertices[vtxIndex] = vertices[vtxIndex].pos;
			skin.normals[vtxIndex] = vertices[vtxIndex].normal;
			skin.uvs[vtxIndex] = glm::vec2(vertices[vtxIndex].uv.x, -vertices[vtxIndex].uv.y);

			if (bakedLighting)
			{
				skin.colors[vtxIndex] = vertices[vtxIndex].color;
			}
			else
			{
				skin.uvs2[vtxIndex] = glm::vec2(vertices[vtxIndex].uv2.x, -vertices[vtxIndex].uv2.y);
			}
		}

		for (uint32_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
		{
			glm::vec3 v0 = vertices[faces[faceIndex].vertices[0]].pos;
			glm::vec3 v1 = vertices[faces[faceIndex].vertices[1]].pos;
			glm::vec3 v2 = vertices[faces[faceIndex].vertices[2]].pos;

			glm::vec3 e1 = v2 - v1;
			glm::vec3 e2 = v0 - v1;

			float u1 = skin.uvs[faces[faceIndex].vertices[2]].x - skin.uvs[faces[faceIndex].vertices[1]].x;
			float u2 = skin.uvs[faces[faceIndex].vertices[0]].x - skin.uvs[faces[faceIndex].vertices[1]].x;

			glm::vec3 binormal = (u2 * e1) - (u1 * e2);
			binormal = glm::normalize(binormal);

			skin.binormals[faces[faceIndex].vertices[0]] += binormal;
			skin.binormals[faces[faceIndex].vertices[1]] += binormal;
			skin.binormals[faces[faceIndex].vertices[2]] += binormal;

			float v1_ = skin.uvs[faces[faceIndex].vertices[2]].y - skin.uvs[faces[faceIndex].vertices[1]].y;
			float v2_ = skin.uvs[faces[faceIndex].vertices[0]].y - skin.uvs[faces[faceIndex].vertices[1]].y;

			glm::vec3 tangent = (v2_ * e1) - (v1_ * e2);
			tangent = glm::normalize(tangent);

			skin.tangents[faces[faceIndex].vertices[0]] += tangent;
			skin.tangents[faces[faceIndex].vertices[1]] += tangent;
			skin.tangents[faces[faceIndex].vertices[2]] += tangent;
		}

		for (uint32_t vertIdx = 0; vertIdx < vertices.size(); ++vertIdx)
		{
			skin.binormals[vertIdx] = -glm::normalize(skin.binormals[vertIdx]);
			skin.tangents[vertIdx] = glm::normalize(skin.tangents[vertIdx]);

			if (glm::dot(glm::normalize(glm::cross(skin.tangents[vertIdx], skin.binormals[vertIdx])), skin.normals[vertIdx]) < 0.0f)
			{
				skin.binormals[vertIdx] = -skin.binormals[vertIdx];
			}
		}

		skin.attributes.resize(faces.size());
		skin.indices.resize(faces.size() * 3);

		for (uint32_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
		{
			skin.indices[(faceIndex * 3) + 0] = faces[faceIndex].vertices[0];
			skin.indices[(faceIndex * 3) + 1] = faces[faceIndex].vertices[1];
			skin.indices[(faceIndex * 3) + 2] = faces[faceIndex].vertices[2];

			skin.attributes[faceIndex] = faces[faceIndex].material;
		}

		skin.skinInfo.resize(m_numBones);
		for (uint32_t boneIndex = 0; boneIndex < m_numBones; ++boneIndex)
		{
			auto& skinInfo = skin.skinInfo[boneIndex];
			skinInfo.verts.reserve(vertices.size());
			skinInfo.weights.reserve(vertices.size());
			skinInfo.boneName = m_bones[boneIndex].GetTag();
			skinInfo.offsetMatrix = glm::inverse(m_bones[boneIndex].GetMatrix());

			for (uint32_t vertIndex = 0; vertIndex < vertices.size(); ++vertIndex)
			{
				SEQMSkinData* vertSkinData = skinData + vertIndex;
				uint32_t numWeights = std::min(4u, vertSkinData->num_weights);

				for (uint32_t weightIndex = 0; weightIndex < numWeights; ++weightIndex)
				{
					if ((uint32_t)vertSkinData->weights[weightIndex].bone == boneIndex)
					{
						skinInfo.verts.push_back(vertIndex);
						skinInfo.weights.push_back(vertSkinData->weights[weightIndex].weight);
					}
				}
			}
		}
	}
}

void HierarchicalModelDefinition::InitBonesFromEQMData(const std::vector<SEQMBone>& bones)
{
	m_numBones = static_cast<uint32_t>(bones.size());
	m_numSubBones = 0;

	if (!bones.empty())
	{
		// Convert SEQMBones into BoneDefinitions
		m_bones.reserve(bones.size());

		for (const SEQMBone& bone : bones)
		{
			m_bones.emplace_back(&bone);

			if (!m_isNewStyleModel && bone.name == "ROOT_BONE")
			{
				m_isNewStyleModel = true;
			}
		}

		// Update sub-bone hierarchy
		for (uint32_t i = 0; i < m_numBones; ++i)
		{
			if (bones[i].num_children > 0)
			{
				int childIndex = bones[i].first_child_index;

				while (childIndex != -1)
				{
					m_bones[i].AddSubBone(&m_bones[childIndex]);
					++m_numSubBones;

					childIndex = bones[childIndex].next_index;
				}
			}
		}

		UpdateDefaultPoseBoneMatrices(&m_bones[0], nullptr);
	}
}

void HierarchicalModelDefinition::SetCollisionMesh(std::vector<glm::vec3>&& vertices, std::vector<uint32_t>&& indices)
{
	m_collisionVertices = std::move(vertices);
	m_collisionIndices = std::move(indices);

	InitCollisionData();
}

void HierarchicalModelDefinition::InitCollisionData()
{
	if (m_collisionIndices.empty() || m_collisionVertices.empty())
	{
		m_hasCollision = true;
		m_colliSpherePos = glm::vec3(0.0f);
		m_colliSphereRadius = m_boundingRadius;
		m_colliBox = { -m_boundingRadius, m_boundingRadius };
	}
	else
	{
		// Calculate bounding box of collision data
		m_colliBox = aabb::init_invalid;

		for (uint32_t idx = 0; idx < m_collisionIndices.size(); idx += 3)
		{
			glm::vec3 v0 = m_collisionVertices[m_collisionIndices[idx + 0]];
			glm::vec3 v1 = m_collisionVertices[m_collisionIndices[idx + 1]];
			glm::vec3 v2 = m_collisionVertices[m_collisionIndices[idx + 2]];

			m_colliBox.enclose(v0);
			m_colliBox.enclose(v1);
			m_colliBox.enclose(v2);
		}

		// We have collision if the box is valid
		m_hasCollision = m_collisionIndices.size() > 3 && m_colliBox.valid();

		if (m_hasCollision)
		{
			m_colliSpherePos = m_colliBox.center();
			m_colliSphereRadius = glm::length(m_colliBox.max - m_colliSpherePos);
		}
		else
		{
			m_colliBox = aabb::init_zero;
			m_colliSpherePos = glm::vec3(0.0f);
			m_colliSphereRadius = 0.0f;
		}
	}
}

void HierarchicalModelDefinition::UpdateDefaultPoseBoneMatrices(BoneDefinition* bone, glm::mat4x4* parentMtx)
{
	glm::mat4x4 mtx = bone->GetDefaultPoseMatrix();

	if (parentMtx)
	{
		mtx *= *parentMtx;
	}

	bone->SetDefaultPoseMatrix(mtx);

	for (uint32_t i = 0; i < bone->GetNumSubBones(); ++i)
	{
		UpdateDefaultPoseBoneMatrices(bone->GetSubBone(i), &mtx);
	}
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
	m_bones.clear();
	m_materialPalette.reset();
}

void HierarchicalModel::Init(const HierarchicalModelDefinitionPtr& definition)
{
	m_definition = definition;

	if (auto pMaterial = m_definition->GetMaterialPalette())
	{
		m_materialPalette = pMaterial->Clone(m_definition->IsNewStyleModel());
	}

	// Create our set of bones from the list of bone definitions
	if (m_definition->GetNumBones() > 0)
	{
		m_bones.resize(m_definition->GetNumBones());
		const auto& definitionBones = m_definition->GetBones();

		// Create bones
		for (uint32_t i = 0; i < m_definition->GetNumBones(); ++i)
		{
			m_bones.push_back(std::make_shared<Bone>(&definitionBones[i]));
		}

		// Link up the bones
		for (uint32_t boneIndex = 0; boneIndex < m_bones.size(); ++boneIndex)
		{
			for (uint32_t subBoneIndex = 0; subBoneIndex < definitionBones[boneIndex].GetNumSubBones(); ++subBoneIndex)
			{
				const BoneDefinition* subBoneDef = definitionBones[boneIndex].GetSubBone(subBoneIndex);

				for (int parentBoneIndex = 0; parentBoneIndex < m_bones.size(); ++parentBoneIndex)
				{
					if (&definitionBones[parentBoneIndex] == subBoneDef)
					{
						m_bones[boneIndex]->AddSubBone(m_bones[parentBoneIndex].get(), subBoneIndex);
						break;
					}
				}
			}
		}
	}

	// TODO: InitMeshSubSets - creates rendering data for mesh skins

	if (auto pointMgr = m_definition->GetPointManager())
	{
		m_pointManager = std::make_unique<ParticlePointManager>(pointMgr, this);
	}

	if (auto particleMgr = m_definition->GetParticleManager())
	{
		m_particleManager = std::make_unique<ActorParticleManager>(particleMgr, this);
	}
}

ParticlePoint* HierarchicalModel::GetParticlePoint(int index) const
{
	if (m_pointManager)
	{
		return m_pointManager->GetPoint(index);
	}

	return nullptr;
}

int HierarchicalModel::GetNumParticlePoints() const
{
	if (m_pointManager)
	{
		return m_pointManager->GetNumPoints();
	}

	return 0;
}

bool HierarchicalModel::SetSkinMeshActiveState(uint32_t index, bool active)
{
	if (index < m_definition->GetNumAttachedSkins())
	{
		if (active)
		{
			m_activeMeshes |= (1 << index);
		}
		else
		{
			m_activeMeshes &= ~(1 << index);
		}

		return true;
	}

	return false;
}

Bone* HierarchicalModel::GetBone(std::string_view tag) const
{
	if (tag.ends_with("DAG"))
	{
		tag = tag.substr(0, tag.size() - 4); // TODO: Check me
	}

	for (uint32_t i = 0; i < m_bones.size(); ++i)
	{
		if (m_bones[i]->GetTag() == tag)
		{
			return m_bones[i].get();
		}
	}

	return nullptr;
}

Bone* HierarchicalModel::GetBone(uint32_t index) const
{
	if (index < m_bones.size())
	{
		return m_bones[index].get();
	}

	return nullptr;
}

void HierarchicalModel::SetBoneParent(Actor* actor)
{
	for (const auto& bone : m_bones)
	{
		bone->SetParentActor(actor);
	}
}

void HierarchicalModel::UpdateBoneToWorldMatrices(glm::mat4x4* parentMatrix)
{
	// TODO
}

void HierarchicalModel::UpdateBoneToWorldMatrices(Bone* bone, glm::mat4x4* parentMatrix)
{
	// TODO
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

void Actor::SetPosition(const glm::vec3& pos)
{
	m_position = pos;
}

void Actor::SetOrientation(const glm::vec3& orientation)
{
	m_orientation = orientation;
}

void Actor::SetScale(float scale)
{
	m_scale = scale;
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
	const std::span<uint32_t>& RGBs,
	std::string_view actorName)
	: Actor(resourceMgr)
{
	m_tag = std::string(actorTag);
	m_actorName = std::string(actorName);
	m_actorIndex = actorIndex;
	m_definition = actorDef;

	InitLOD();

	SetPosition(position);
	SetOrientation(orientation);
	SetScale(scaleFactor);
	SetBoundingRadius(boundingRadius);
	SetCollisionVolumeType(collisionVolumeType);

	if (DMRGBTrackWLDData != nullptr)
	{
		m_model->SetRGBs(DMRGBTrackWLDData);
	}

	if (!RGBs.empty())
	{
		m_model->SetRGBs(RGBs);
	}

	DoInitCallback();

	m_resourceMgr->AddActor(this);
}

SimpleActor::SimpleActor(ResourceManager* resourceMgr,
	std::string_view actorTag,
	const ActorDefinitionPtr& actorDef,
	int actorIndex,
	bool useDefaultBoundingRadius,
	std::string_view actorName)
	: Actor(resourceMgr)
{
	m_tag = actorTag;
	m_actorName = std::string(actorName);
	m_actorIndex = actorIndex;
	m_definition = actorDef;

	InitLOD();

	if (useDefaultBoundingRadius)
	{
		SetBoundingRadius(m_model->GetDefinition()->GetDefaultBoundingRadius());
	}

	SetCollisionVolumeType(m_definition->GetCollisionVolumeType());

	m_resourceMgr->AddActor(this);
}


SimpleActor::~SimpleActor()
{
	m_resourceMgr->RemoveActor(this);
}

void SimpleActor::InitLOD()
{
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

		// TODO: Support "LOD'd model"
		if (!m_lodModels.empty())
		{
			m_model = m_lodModels[0].model;
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
}

bool SimpleActor::IsCollidable() const
{
	return m_collisionModel && m_collisionModel->GetDefinition()->m_hasCollision;
}

//-------------------------------------------------------------------------------------------------

HierarchicalActor::HierarchicalActor(
	ResourceManager* resourceMgr,
	std::string_view actorTag,
	const ActorDefinitionPtr& actorDef,
	const glm::vec3& position,
	const glm::vec3& orientation,
	float boundingRadius,
	float scaleFactor,
	ECollisionVolumeType collisionVolumeType,
	int actorIndex,
	SDMRGBTrackWLDData* DMRGBTrackWLDData,
	const std::span<uint32_t>& RGBs,
	std::string_view actorName)
	: Actor(resourceMgr)
{
	m_tag = std::string(actorTag);
	m_actorName = std::string(actorName);
	m_actorIndex = actorIndex;
	m_definition = actorDef;

	InitLOD();

	m_model = std::make_shared<HierarchicalModel>();


	m_resourceMgr->AddActor(this);
}

HierarchicalActor::HierarchicalActor(ResourceManager* resourceMgr,
	std::string_view actorTag,
	const ActorDefinitionPtr& actorDef,
	int actorIndex,
	bool allSkinsActive,
	bool useDefaultBoundingRadius,
	bool sharedBoneGroups,
	Bone* pBone,
	std::string_view actorName)
	: Actor(resourceMgr)
{
	m_tag = std::string(actorTag);
	m_actorName = std::string(actorName);
	m_actorIndex = actorIndex;
	m_definition = actorDef;

	m_bones.resize(eNumBones, nullptr);
	m_boneGroups.resize(eNumBoneGroups);

	InitLOD();
	SetAllSkinsActive();
	m_model->SetBoneParent(this);

	if (m_model->GetDefinition()->GetDefaultAnimation())
	{
		if (m_model->GetDefinition()->IsNewStyleModel())
		{
			PutAllBonesInBoneGroup(eBoneGroupUpperBody, 1, true);
		}
		else
		{
			// TODO
		}
	}

	m_resourceMgr->AddActor(this);
}

HierarchicalActor::~HierarchicalActor()
{
	m_resourceMgr->RemoveActor(this);
}

void HierarchicalActor::InitLOD()
{
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
		// Hierarchical models also have a LCOffset

		m_lodModels.reserve(pLODList->GetElements().size());
		for (const LODListElementPtr& element : pLODList->GetElements())
		{
			std::string definitionTag = fmt::format("{}_HMD", element->definition);

			if (HierarchicalModelDefinitionPtr pModelDef = m_resourceMgr->Get<HierarchicalModelDefinition>(
				definitionTag))
			{
				HierarchicalModelPtr pModel = m_resourceMgr->CreateHierarchicalModel();
				pModel->Init(pModelDef);
				pModel->SetActor(this);

				m_lodModels.emplace_back(pModel, element->max_distance);
			}
			else
			{
				EQG_LOG_ERROR("Failed to load LOD'ed model {} from LOD List for {}", definitionTag, defTag);
			}
		}

		for (uint32_t i = 1; i < m_lodModels.size(); ++i)
		{
			AttachLODModels(m_lodModels[0].model.get(), m_lodModels[i].model.get());
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
	}
	else
	{
		m_model = std::make_shared<HierarchicalModel>();

		m_model->Init(m_definition->GetHierarchicalModelDefinition());
		m_model->SetActor(this);
	}
}

void HierarchicalActor::AttachLODModels(HierarchicalModel* parent, HierarchicalModel* child)
{
	if (!parent || !child)
		return;

	for (uint32_t i = 0; i < child->GetDefinition()->GetNumBones(); ++i)
	{
		Bone* bone = child->GetBone(i);

		for (uint32_t parentIndex = 0; parentIndex < parent->GetDefinition()->GetNumBones(); ++parentIndex)
		{
			Bone* parentBone = parent->GetBone(parentIndex);

			if (bone->GetTag() == parentBone->GetTag())
			{
				bone->AttachToBone(parentBone);
			}
		}
	}
}

void HierarchicalActor::SetAllSkinsActive()
{
	HierarchicalModelDefinitionPtr pDefinition = m_model->GetDefinition();
	uint32_t numSkins = pDefinition->GetNumAttachedSkins();

	if (numSkins > 0)
	{
		uint32_t firstActive = pDefinition->GetFirstDefaultActiveSkin();
		for (uint32_t index = 0; index < pDefinition->GetNumDefaultActiveSkins(); ++index)
		{
			m_model->SetSkinMeshActiveState(firstActive + index, true);
		}

		bool newStyle = pDefinition->IsNewStyleModel();

		for (uint32_t index = 0; index < numSkins; ++index)
		{
			std::string_view skinTag = pDefinition->GetAttachedSkin(index)->tag;

			if (skinTag.starts_with("COK") || skinTag.starts_with("TRI"))
				break;
			if (skinTag.size() < 5)
				continue;

			if (skinTag[3] == 'H' && skinTag[4] == 'E')
				m_headSkins |= (1 << index);
			if (newStyle && (skinTag[3] == '_' || (isdigit(skinTag[3]) && isdigit(skinTag[4]))))
				m_bodySkins |= (1 << index);
		}
	}
}

void HierarchicalActor::PutAllBonesInBoneGroup(int groupIndex, int maxNumAnims, bool newBoneNames)
{
	if (m_boneGroups[groupIndex] == nullptr)
	{
		uint32_t numBones = m_model->GetDefinition()->GetNumBones();
		BoneGroupPtr boneGroup = std::make_shared<BoneGroup>(numBones, maxNumAnims);

		for (uint32_t i = 0; i < numBones; ++i)
		{
			boneGroup->AddBone(m_model->GetBone(i), newBoneNames);
		}

		m_boneGroups[groupIndex] = boneGroup;
	}
}

bool HierarchicalActor::IsCollidable() const
{
	return m_model->GetDefinition()->m_hasCollision;
}

//-------------------------------------------------------------------------------------------------

ParticleActor::ParticleActor(ResourceManager* resourceMgr, std::string_view actorTag,
	const ActorDefinitionPtr& actorDef, int actorIndex, bool allSkinsActive, Bone* bone)
	: Actor(resourceMgr)
{
	m_tag = std::string(actorTag);
	m_actorIndex = actorIndex;
	m_definition = actorDef;

	// TODO: Create Emitter

	SetHasParentBone(bone != nullptr);

	resourceMgr->AddActor(this);
}

ParticleActor::~ParticleActor()
{
	m_resourceMgr->RemoveActor(this);
}
} // namespace eqg
