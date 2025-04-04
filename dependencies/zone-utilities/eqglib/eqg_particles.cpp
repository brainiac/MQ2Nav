#include "pch.h"

#include "eqg_material.h"
#include "eqg_particles.h"

#include "eqg_geometry.h"
#include "log_internal.h"
#include "str_util.h"
#include "wld_types.h"

namespace eqg {

void SEmitterDefData::UpdateFromPCloudDef(std::string_view tag,
	const SParticleCloudDefData& defData,
	const STextureDataDefinition& spriteDef)
{
	emitterName = std::string(tag);
	oldParticleType = (EParticleType)defData.type;
	oldFlags = defData.flags;

	if (defData.flags & PCLOUD_FLAG_OBJECTRELATIVE || defData.flags & PCLOUD_FLAG_PARENTOBJRELATIVE)
	{
		stickToActor = true;
	}

	if (defData.flags & PCLOUD_FLAG_SPAWNSCALERELATIVE || stickToActor)
	{
		scaleWithActor = true;
	}

	relativeToBone = true;
	oldSize = defData.size;
	scalarGravity = defData.scalarGravity;
	gravity = defData.gravity;
	bbMin = defData.bbMin;
	bbMax = defData.bbMax;

	switch (defData.spawnType)
	{
	case PCLOUD_SPAWN_SHAPE_BOX:
		spawnShape = EEmitterShape::EMITTER_SHAPE_CUBE_RANDOM;
		shapeRadius = glm::abs((defData.spawnMax.x - defData.spawnMin.x) / 2);
		shapeRadiusMinor = glm::abs((defData.spawnMax.y - defData.spawnMin.y) / 2);
		shapeHeight = glm::abs((defData.spawnMax.z - defData.spawnMin.z) / 2);
		speedMin = defData.spawnNormal * defData.spawnVelocity;
		speedMax = defData.spawnNormal * defData.spawnVelocity;
		break;

	case PCLOUD_SPAWN_SHAPE_SPHERE:
		spawnShape = EEmitterShape::EMITTER_SHAPE_SPHERE_RANDOM;
		shapeRadius = defData.spawnRadius;
		outwardSpeedMin = defData.spawnVelocity * 0.25f;
		outwardSpeedMax = defData.spawnVelocity;
		break;

	case PCLOUD_SPAWN_SHAPE_RING:
		spawnShape = EEmitterShape::EMITTER_SHAPE_RING_RANDOM;
		shapeRadius = defData.spawnRadius;
		outwardSpeedMax = defData.spawnVelocity;
		outwardSpeedMin = defData.spawnVelocity;
		break;

	case PCLOUD_SPAWN_SHAPE_SPRAY:
		spawnShape = EEmitterShape::EMITTER_SHAPE_DISK_RANDOM;
		shapeRadius = (defData.flags & PCLOUD_FLAG_BROWNIAN) ? 0.1f : 0.0f;

		// TODO: Convert angles
		if (defData.spawnNormal.x == 1.0f)
			shapeTilt.x = 128.0f;
		else if (defData.spawnNormal.z == -1.0f)
			shapeTilt.x = 256.0f;
		else if (defData.spawnNormal.x == -1.0f)
			shapeTilt.x = -128.0f;
		else if (defData.spawnNormal.y == 1.0f)
			shapeTilt.y = -128.0f;
		else if (defData.spawnNormal.y == -1.0f)
			shapeTilt.y = 128.0f;

		speedMin.z = defData.spawnVelocity;
		speedMax.z = defData.spawnVelocity;
		outwardSpeedMin = 0;
		outwardSpeedMax = tan(defData.spawnAngle) * defData.spawnVelocity;
		break;

	case PCLOUD_SPAWN_SHAPE_DISK:
		spawnShape = EEmitterShape::EMITTER_SHAPE_DISK_RANDOM;
		shapeRadius = defData.spawnRadius;
		speedMin = defData.spawnNormal * defData.spawnVelocity;
		speedMax = defData.spawnNormal * defData.spawnVelocity;
		break;

	case PCLOUD_SPAWN_SHAPE_RING2:
		spawnShape = EEmitterShape::EMITTER_SHAPE_RING_RANDOM;
		shapeRadius = defData.spawnRadius;
		speedMin = defData.spawnNormal * defData.spawnVelocity;
		speedMax = defData.spawnNormal * defData.spawnVelocity;
		break;

	default:
		EQG_LOG_WARN("Unknown PCloudDef type: {}", defData.spawnType);
	}

	defaultLifeSpan = defData.spawnTime == 0 ? -9999.0f : -(static_cast<float>(defData.spawnTime) / 1000.0f);
	particleLifeSpan = static_cast<float>(defData.spawnLifespan) / 1000.0f;

	if (defData.flags & PCLOUD_FLAG_FADE)
	{
		fadeOutTime = particleLifeSpan;
		alpha = 1.0f;
	}
	else
	{
		alpha = 0.5f;
	}
	maxAlpha = alpha;

	particlesAtInterval = 1;
	intervalsPerSecond = 1000.0f / static_cast<float>(defData.spawnRate);
	spawnScale = defData.spawnScale;
	tintStartR = defData.color & 0xFF0000 >> 16;
	tintStartG = defData.color & 0x00FF00 >> 8;
	tintStartB = defData.color & 0x0000FF;
	tintEndR = tintStartR;
	tintEndG = tintStartG;
	tintEndB = tintStartB;

	if (spriteDef.updateInterval < 2)
		animationRate = 10.0f;
	else
		animationRate = 1000.0f / static_cast<float>(spriteDef.updateInterval);
	animationFrames = std::max<int>(spriteDef.numFrames, 1);

	uint32_t renderMethod = spriteDef.renderMethod;

	if (renderMethod & RM_CUSTOM_MASK)
	{
		// TODO: Figure out how to get custom render method
	}

	if (renderMethod & RM_TRANSLUCENCY_MASK)
	{
		additiveBlending = (renderMethod & RM_ADDITIVE_LIGHT_MASK) != 0;
	}

	// Lol hacks
	if (tag.starts_with("TFIRE2"))
	{
		particleLifeSpan = 0.08f;
		shapeRadius = 0.05f;
		particlesAtInterval = 1;
		intervalsPerSecond = 100.0f / 3.0f;
		oldSize = 8;
	}

	noOcclusion = true;
}

ParticleCloudDefinition::ParticleCloudDefinition()
	: Resource(GetStaticResourceType())
{
}

ParticleCloudDefinition::~ParticleCloudDefinition()
{
}

bool ParticleCloudDefinition::Init(
	const SEmitterDefData& emitterDef,
	const std::shared_ptr<BlitSpriteDefinition>& textureData)
{
	m_tag = emitterDef.emitterName;
	m_spriteDefinition = textureData;
	m_emitterDef = emitterDef;

	return true;
}

//-------------------------------------------------------------------------------------------------

BlitSpriteDefinition::BlitSpriteDefinition()
	: Resource(GetStaticResourceType())
{
}

BlitSpriteDefinition::~BlitSpriteDefinition()
{
}

bool BlitSpriteDefinition::Init(std::string_view tag, const STextureDataDefinition& definition)
{
	m_tag = tag;
	m_columns = definition.columns;
	m_rows = definition.rows;
	m_width = definition.width;
	m_height = definition.height;
	m_numFrames = definition.numFrames;
	m_currentFrame = definition.currentFrame;
	m_updateInterval = definition.updateInterval;
	m_renderMethod = definition.renderMethod;
	m_valid = definition.valid;
	m_skipFrames = definition.skipFrames;

	// TODO: Init texture from frames of bitmap
	m_sourceTextures = definition.sourceTextures;

	return true;
}

void BlitSpriteDefinition::CopyDefinition(STextureDataDefinition& outDefinition)
{
	outDefinition.columns = m_columns;
	outDefinition.rows = m_rows;
	outDefinition.width = m_width;
	outDefinition.height = m_height;
	outDefinition.numFrames = m_numFrames;
	outDefinition.currentFrame = m_currentFrame;
	outDefinition.updateInterval = m_updateInterval;
	outDefinition.renderMethod = m_renderMethod;
	outDefinition.valid = m_valid;
	outDefinition.skipFrames = m_skipFrames;
	outDefinition.sourceTextures = m_sourceTextures;
}

//-------------------------------------------------------------------------------------------------

ParticlePointDefinition::ParticlePointDefinition(SParticlePoint* point, SimpleModelDefinition* model)
	: name(point->name)
	, position(point->position)
	, orientation(point->orientation)
	, scale(point->scale)
{
	UpdateMatrix();
}

ParticlePointDefinition::ParticlePointDefinition(SParticlePoint* point, HierarchicalModelDefinition* model)
	: name(point->name)
	, position(point->position)
	, orientation(point->orientation)
	, scale(point->scale)
{
	for (uint32_t i = 0; i < model->GetNumBones(); ++i)
	{
		if (const BoneDefinition* pBone = model->GetBoneDefinition(i))
		{
			if (pBone->GetTag() == point->attachment)
			{
				boneIndex = i;
				break;
			}
		}
	}

	UpdateMatrix();
}

ParticlePointDefinition::ParticlePointDefinition(const std::string& name, int boneIndex,
	const glm::vec3& position, const glm::vec3& orientation, const glm::vec3& scale)
	: name(name)
	, boneIndex(boneIndex)
	, position(position)
	, orientation(orientation)
	, scale(scale)
{
	UpdateMatrix();
}

ParticlePointDefinition::ParticlePointDefinition(const ParticlePointDefinition& other)
	: name(other.name)
	, boneIndex(other.boneIndex)
	, position(other.position)
	, orientation(other.orientation)
	, scale(other.scale)
	, transform(other.transform)
{
}

void ParticlePointDefinition::UpdateMatrix()
{
	transform = glm::translate(glm::identity<glm::mat4x4>(), position);
	transform *= glm::mat4_cast(glm::quat(orientation));
	transform = glm::scale(transform, scale);
}

//-------------------------------------------------------------------------------------------------

ParticlePointDefinitionManager::ParticlePointDefinitionManager()
{
}

ParticlePointDefinitionManager::ParticlePointDefinitionManager(uint32_t numPoints, SParticlePoint* points, SimpleModelDefinition* model)
{
	m_points.reserve(numPoints);

	for (uint32_t i = 0; i < numPoints; ++i)
	{
		AddPoint(points + i, model);
	}
}

ParticlePointDefinitionManager::ParticlePointDefinitionManager(uint32_t numPoints, SParticlePoint* points, HierarchicalModelDefinition* model)
{
	m_points.reserve(numPoints);

	for (uint32_t i = 0; i < numPoints; ++i)
	{
		AddPoint(points + i, model);
	}
}

ParticlePointDefinitionManager::~ParticlePointDefinitionManager()
{
}

ParticlePointDefinition* ParticlePointDefinitionManager::GetPointDefinition(uint32_t index) const
{
	if (index < m_points.size())
		return m_points[index].get();

	return nullptr;
}

void ParticlePointDefinitionManager::AddPoint(SParticlePoint* point, SimpleModelDefinition* model)
{
	m_points.push_back(std::make_unique<ParticlePointDefinition>(point, model));
}

void ParticlePointDefinitionManager::AddPoint(SParticlePoint* point, HierarchicalModelDefinition* model)
{
	m_points.push_back(std::make_unique<ParticlePointDefinition>(point, model));
}

void ParticlePointDefinitionManager::AddPointDefinition(const std::string& name, int boneIndex,
	const glm::vec3& position, const glm::vec3& orientation, const glm::vec3& scale)
{
	m_points.push_back(std::make_unique<ParticlePointDefinition>(name, boneIndex, position, orientation, scale));
}

void ParticlePointDefinitionManager::DeletePointDefinition(const std::string& name)
{
	for (auto it = m_points.begin(); it != m_points.end(); ++it)
	{
		if ((*it)->name == name)
		{
			m_points.erase(it);
			break;
		}
	}
}

//-------------------------------------------------------------------------------------------------

ActorParticleDefinition::ActorParticleDefinition(SActorParticle* particle, SimpleModelDefinition* definition)
	: m_emitterDefinitionID(particle->emitterDefinitionID)
	, m_coldEmitterDefinitionID(particle->coldEmitterDefinitionID)
	, m_pointName(particle->particlePointName)
	, m_particleType(particle->particleType)
	, m_animationNumber(particle->animationNumber)
	, m_animationVariation(particle->animationVariation)
	, m_animationRandomVariation(particle->animationRandomVariation)
	, m_startTime(particle->startTime)
	, m_pointIndex(-1)
	, m_lifeSpan(particle->lifeSpan)
	, m_groundBased(particle->groundBased)
	, m_playWithMat(particle->playWithMat)
	, m_sporadic(particle->sporadic)
{
	InitIndex(definition->GetPointManager());
}

ActorParticleDefinition::ActorParticleDefinition(SActorParticle* particle, HierarchicalModelDefinition* definition)
	: m_emitterDefinitionID(particle->emitterDefinitionID)
	, m_coldEmitterDefinitionID(particle->coldEmitterDefinitionID)
	, m_pointName(particle->particlePointName)
	, m_particleType(particle->particleType)
	, m_animationNumber(particle->animationNumber)
	, m_animationVariation(particle->animationVariation)
	, m_animationRandomVariation(particle->animationRandomVariation)
	, m_startTime(particle->startTime)
	, m_pointIndex(-1)
	, m_lifeSpan(particle->lifeSpan)
	, m_groundBased(particle->groundBased)
	, m_playWithMat(particle->playWithMat)
	, m_sporadic(particle->sporadic)
{
	InitIndex(definition->GetPointManager());
}

ActorParticleDefinition::ActorParticleDefinition(const ActorParticleDefinition& other)
	: m_emitterDefinitionID(other.m_emitterDefinitionID)
	, m_coldEmitterDefinitionID(other.m_coldEmitterDefinitionID)
	, m_pointName(other.m_pointName)
	, m_particleType(other.m_particleType)
	, m_animationNumber(other.m_animationNumber)
	, m_animationVariation(other.m_animationVariation)
	, m_animationRandomVariation(other.m_animationRandomVariation)
	, m_startTime(other.m_startTime)
	, m_pointIndex(other.m_pointIndex)
	, m_lifeSpan(other.m_lifeSpan)
	, m_groundBased(other.m_groundBased)
	, m_playWithMat(other.m_playWithMat)
	, m_sporadic(other.m_sporadic)
{
}

void ActorParticleDefinition::InitIndex(ParticlePointDefinitionManager* pPtMgr)
{
	if (pPtMgr)
	{
		for (uint32_t index = 0; index < pPtMgr->GetNumPoints(); ++index)
		{
			if (ParticlePointDefinition* pDef = pPtMgr->GetPointDefinition(index))
			{
				if (pDef->name == m_pointName)
				{
					m_pointIndex = static_cast<int>(index);
					break;
				}
			}
		}
	}
}


//-------------------------------------------------------------------------------------------------

ActorParticleDefinitionManager::ActorParticleDefinitionManager(uint32_t numParticles, SActorParticle* particles,
	SimpleModelDefinition* definition)
{
	if (numParticles > 0)
	{
		for (uint32_t particle = 0; particle < numParticles; ++particle)
		{
			m_particleDefinitions.emplace_back(std::make_unique<ActorParticleDefinition>(particles + particle, definition));
		}
	}
}

ActorParticleDefinitionManager::ActorParticleDefinitionManager(uint32_t numParticles, SActorParticle* particles,
	HierarchicalModelDefinition* definition)
{
	if (numParticles > 0)
	{
		for (uint32_t particle = 0; particle < numParticles; ++particle)
		{
			m_particleDefinitions.emplace_back(std::make_unique<ActorParticleDefinition>(particles + particle, definition));
		}
	}
}

ActorParticleDefinitionManager::~ActorParticleDefinitionManager()
{
}

ActorParticleDefinition* ActorParticleDefinitionManager::GetParticleDefinition(uint32_t index) const
{
	if (index < m_particleDefinitions.size())
		return m_particleDefinitions[index].get();

	return nullptr;
}

void ActorParticleDefinitionManager::AddParticleDefinition(SActorParticle* particle, SimpleModelDefinition* definition)
{
	m_particleDefinitions.emplace_back(std::make_unique<ActorParticleDefinition>(particle, definition));
}

void ActorParticleDefinitionManager::AddParticleDefinition(SActorParticle* particle, HierarchicalModelDefinition* definition)
{
	m_particleDefinitions.emplace_back(std::make_unique<ActorParticleDefinition>(particle, definition));
}

void ActorParticleDefinitionManager::DeleteParticleDefinition(uint32_t index)
{
	m_particleDefinitions.erase(m_particleDefinitions.begin() + index);
}

} // namespace eqg
