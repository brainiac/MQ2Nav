#pragma once

#include "eqg_resource.h"

namespace eqg {

struct SParticleCloudDefData;
struct STextureDataDefinition;

// Emitter Shape
enum EEmitterShape
{
	EMITTER_SHAPE_POINT = 0,
	EMITTER_SHAPE_RING_UNIFORM,
	EMITTER_SHAPE_CYLINDER_UNIFORM,
	EMITTER_SHAPE_CYLINDER_RANDOM,
	EMITTER_SHAPE_DISK_RANDOM,
	EMITTER_SHAPE_SPHERE_RANDOM,
	EMITTER_SHAPE_CUBE_RANDOM,
	EMITTER_SHAPE_CONE_RANDOM,
	EMITTER_SHAPE_TORUS_RANDOM,
	EMITTER_SHAPE_RING_RANDOM,
	EMITTER_SHAPE_COUNT,
};

enum EParticleType
{
	PARTICLE_TYPE_NONE = 0,
	PARTICLE_TYPE_POINT,
	PARTICLE_TYPE_LINE,
	PARTICLE_TYPE_SPRITE,
	PARTICLE_TYPE_SPLASH,
};

enum EParticleOrientation
{
	PARTICLE_ORIENTATION_NORMAL = 0,
	PARTICLE_ORIENTATION_POSITIVE_X,
	PARTICLE_ORIENTATION_POSITIVE_Y,
	PARTICLE_ORIENTATION_POSITIVE_Z,
	PARTICLE_ORIENTATION_NEGATIVE_X,
	PARTICLE_ORIENTATION_NEGATIVE_Y,
	PARTICLE_ORIENTATION_NEGATIVE_Z,
};

// Definition of a particle emitter
struct SEmitterDefData
{
	std::string emitterName;
	std::string textureName;
	bool relativeToBone = false;
	bool noOcclusion = false;
	bool additiveBlending = false;
	bool scaleWithActor = false;
	bool spriteOrientation = false;
	bool stickToActor = false;
	float defaultLifeSpan = 0;
	float particleLifeSpan = -1.0f;
	int particlesAtCreation = 1;
	int particlesAtInterval = 0;
	float intervalsPerSecond = 0;
	float spawnDelay = 0;
	float fadeInTime = 0;
	float fadeOutTime = 0;
	float scaleInTime = 0;
	float scaleOutTime = 0;
	float reductionDistance = 80.0f;
	float maxAlpha = 1.0f;
	EEmitterShape spawnShape = EMITTER_SHAPE_POINT;
	float shapeRadius = 0;
	float shapeRadiusMinor = 0;
	float shapeHeight = 0;
	glm::vec3 shapeOffset = glm::vec3(0);
	glm::vec2 shapeTilt = glm::vec2(0);
	float particleWidthMin = 1.0f;
	float particleZBias = 0;
	uint8_t tintStartR = 255;
	uint8_t tintStartG = 255;
	uint8_t tintStartB = 255;
	uint8_t tintEndR = 255;
	uint8_t tintEndG = 255;
	uint8_t tintEndB = 255;
	glm::vec3 speedMin = glm::vec3(0);
	glm::vec3 speedMax = glm::vec3(0);
	glm::vec3 acceleration = glm::vec3(0);
	float outwardSpeedMin = 0;
	float outwardSpeedMax = 0;
	float outwardSpeedAcceleration = 0;
	float orbitalSpeedMin = 0;
	float orbitalSpeedMax = 0;
	float orbitalSpeedAcceleration = 0;
	float scalarGravity = 0;
	float windSpeed = 0;
	int animationFrames = 16;
	float animationRate = 16.0f;
	float particleSpinRate = 0;
	EParticleType oldParticleType = PARTICLE_TYPE_NONE;
	uint32_t oldFlags = 0;
	uint32_t oldSize = 0;
	glm::vec3 gravity = glm::vec3(0, 0, 1.0f);
	glm::vec3 bbMin = glm::vec3(0);
	glm::vec3 bbMax = glm::vec3(0);
	float spawnScale = 1.0f;
	float alpha = 1.0f;
	int randomRotation = 0;
	int particleOrientation = PARTICLE_ORIENTATION_NORMAL;
	float particleHeightMin = 1.0f;
	float particleHeightMax = 1.0f;
	float particleWidthMax = 1.0f;
	float particleSpinRateMax = 0;
	int proportionalSizeScaling = 1;
	float heightSquashTime = 0;
	float widthSquashTime = 0;
	int allowCenterPassThrough = 0;
	int scaleEmitterWithActor = 1;

	void UpdateFromPCloudDef(std::string_view tag, const SParticleCloudDefData& defData,
		const STextureDataDefinition& spriteDef);
};

class BlitSpriteDefinition : public Resource
{
public:
	BlitSpriteDefinition();
	~BlitSpriteDefinition() override;

	static ResourceType GetStaticResourceType() { return ResourceType::BlitSpriteDefinition; }

	std::string_view GetTag() const override { return m_tag; }

	virtual bool Init(std::string_view tag, const STextureDataDefinition& definition);

	void CopyDefinition(STextureDataDefinition& outDefinition);

	std::string m_tag;

	uint32_t m_columns = 0;
	uint32_t m_rows = 0;
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	uint32_t m_numFrames = 0;
	uint32_t m_currentFrame = 0;
	uint32_t m_updateInterval = 1;
	uint32_t m_renderMethod = 0;
	bool m_valid = false;
	bool m_skipFrames = false;
	std::vector<BitmapPtr> m_sourceTextures;
};
	
class ParticleCloudDefinition : public Resource
{
public:
	ParticleCloudDefinition();
	~ParticleCloudDefinition() override;

	bool Init(const SEmitterDefData& emitterDef, const std::shared_ptr<BlitSpriteDefinition>& spriteDef);

	static ResourceType GetStaticResourceType() { return ResourceType::ParticleCloudDefinition; }

	std::string_view GetTag() const override { return m_tag; }

protected:
	std::string                   m_tag;

	SEmitterDefData               m_emitterDef;
	std::shared_ptr<BlitSpriteDefinition> m_spriteDefinition;
};

} // namespace eqg
