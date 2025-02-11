#include "pch.h"

#include "eqg_material.h"
#include "eqg_particles.h"
#include "log_internal.h"
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

} // namespace eqg
