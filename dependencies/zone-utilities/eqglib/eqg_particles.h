#pragma once

#include "eqg_resource.h"
#include "eqg_types_fwd.h"

namespace eqg {

struct SParticleCloudDefData;
struct STextureDataDefinition;

class Bone;
class SimpleModelDefinition;
class SimpleModel;
class HierarchicalModelDefinition;
class HierarchicalModel;
class ParticleEmitter;

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

//-------------------------------------------------------------------------------------------------

struct SParticlePoint
{
	std::string name;
	std::string attachment;
	glm::vec3   position;
	glm::vec3   orientation;
	glm::vec3   scale;
};

struct ParticlePointDefinition
{
	ParticlePointDefinition(SParticlePoint* point, SimpleModelDefinition* model);
	ParticlePointDefinition(SParticlePoint* point, HierarchicalModelDefinition* model);
	ParticlePointDefinition(const std::string& name, int boneIndex, const glm::vec3& position,
		const glm::vec3& orientation, const glm::vec3& scale);
	ParticlePointDefinition(const ParticlePointDefinition&);

	std::string name;
	int         boneIndex = - 1;
	glm::vec3   position;
	glm::vec3   orientation;
	glm::vec3   scale;
	glm::mat4x4 transform;

	void UpdateMatrix();
};

// Manages attachment points for particle emitters on a model
class ParticlePointDefinitionManager
{
public:
	ParticlePointDefinitionManager();
	~ParticlePointDefinitionManager();
	ParticlePointDefinitionManager(uint32_t numPoints, SParticlePoint* points, SimpleModelDefinition* model);
	ParticlePointDefinitionManager(uint32_t numPoints, SParticlePoint* points, HierarchicalModelDefinition* model);

	uint32_t GetNumPoints() const { return (uint32_t)m_points.size(); }
	ParticlePointDefinition* GetPointDefinition(uint32_t index) const;

	void AddPoint(SParticlePoint* point, SimpleModelDefinition* model);
	void AddPoint(SParticlePoint* point, HierarchicalModelDefinition* model);

	void AddPointDefinition(const std::string& name, int boneIndex, const glm::vec3& position,
		const glm::vec3& orientation, const glm::vec3& scale);
	void DeletePointDefinition(const std::string& name);

private:
	std::vector<std::unique_ptr<ParticlePointDefinition>> m_points;
};

//-------------------------------------------------------------------------------------------------

class ParticlePoint
{
public:
	ParticlePoint(ParticlePointDefinition* definition, HierarchicalModel* model);
	ParticlePoint(ParticlePointDefinition* definition, SimpleModel* model);
	~ParticlePoint();

	ParticlePointDefinition* GetDefinition() const { return m_definition; }
	const std::string& GetName() const { return m_definition->name; }
	const glm::mat4x4& GetWorldSpaceMatrix();
	Actor* GetActor() const;

	bool IsInvisible() const;
	bool IsDisabled() const;

private:
	Actor* GetTopParent() const;
	bool ShouldShowParticlesWhenInvisibile() const;
	bool ShouldUpdateAttachedModelWorldSpaceMatrix() const;

	ParticlePointDefinition* m_definition;
	glm::mat4x4              m_worldMtx;

	HierarchicalModel*       m_hierarchicalModel = nullptr;
	SimpleModel*             m_simpleModel = nullptr;
	Bone*                    m_bone = nullptr;
};

class ParticlePointManager
{
public:
	ParticlePointManager(ParticlePointDefinitionManager* definitionMgr, HierarchicalModel* model);
	ParticlePointManager(ParticlePointDefinitionManager* definitionMgr, SimpleModel* model);
	~ParticlePointManager();

	uint32_t GetNumPoints() const;
	ParticlePoint* GetPoint(uint32_t index) const;

	int GetPointIndex(std::string_view tag) const;

private:
	std::vector<std::unique_ptr<ParticlePoint>> m_points;
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

struct SActorParticle
{
	int  emitterDefinitionID;
	char particlePointName[64];
	int  particleType;
	int  animationNumber;
	int  animationVariation;
	int  animationRandomVariation;
	int  startTime;
	int  lifeSpan;
	int  groundBased;
	int  playWithMat;
	int  sporadic;
	int  coldEmitterDefinitionID;
};

constexpr int ParticleType_Persistent = 0;
constexpr int ParticleType_AnimationBased = 1;

class ActorParticleDefinition
{
public:
	ActorParticleDefinition(SActorParticle* particle, SimpleModelDefinition* definition);
	ActorParticleDefinition(SActorParticle* particle, HierarchicalModelDefinition* definition);
	ActorParticleDefinition(const ActorParticleDefinition& other);

	int GetEmitterDefinitionID() const { return m_emitterDefinitionID; }
	int GetColdEmitterDefinitionID() const { return m_coldEmitterDefinitionID; }
	const std::string& GetPointName() const { return m_pointName; }
	int GetParticleType() const { return m_particleType; }
	int GetAnimationNumber() const { return m_animationNumber; }
	int GetAnimationVariation() const { return m_animationVariation; }
	int GetAnimationRandomVariation() const { return m_animationRandomVariation; }
	int GetStartTime() const { return m_startTime; }
	int GetPointIndex() const { return m_pointIndex; }
	int GetLifeSpan() const { return m_lifeSpan; }
	int GetGroundBased() const { return m_groundBased; }
	int GetPlayWithMat() const { return m_playWithMat; }
	int GetSporadic() const { return m_sporadic; }

private:
	void InitIndex(ParticlePointDefinitionManager* pPtMgr);

	int         m_emitterDefinitionID;
	int         m_coldEmitterDefinitionID;
	std::string m_pointName;
	int         m_particleType;
	int         m_animationNumber;
	int         m_animationVariation;
	int         m_animationRandomVariation;
	int         m_startTime;
	int         m_pointIndex;
	int         m_lifeSpan;
	int         m_groundBased;
	int         m_playWithMat;
	int         m_sporadic;
};

// Manages definitions of a particle emitter on a model
class ActorParticleDefinitionManager
{
public:
	ActorParticleDefinitionManager(uint32_t numParticles, SActorParticle* particles, SimpleModelDefinition* definition);
	ActorParticleDefinitionManager(uint32_t numParticles, SActorParticle* particles, HierarchicalModelDefinition* definition);
	~ActorParticleDefinitionManager();

	uint32_t GetNumParticles() const { return (uint32_t)m_particleDefinitions.size(); }
	ActorParticleDefinition* GetParticleDefinition(uint32_t index) const;

	void AddParticleDefinition(SActorParticle* particle, SimpleModelDefinition* definition);
	void AddParticleDefinition(SActorParticle* particle, HierarchicalModelDefinition* definition);
	void DeleteParticleDefinition(uint32_t index);

	std::vector<std::unique_ptr<ActorParticleDefinition>> m_particleDefinitions;
};

//-------------------------------------------------------------------------------------------------

class ActorParticle
{
public:
	ActorParticle(ActorParticleDefinition* definition, HierarchicalModel* model);
	ActorParticle(ActorParticleDefinition* definition, SimpleModel* model);
	~ActorParticle();

	ActorParticleDefinition* GetDefinition() const { return m_definition; }
	ParticlePoint* GetParticlePoint() const { return m_particlePoint; }

	void StartParticle(Actor* actor);
	void StopParticle();

	int GetEmitterDefID() const { return m_definition->GetEmitterDefinitionID(); }
	int GetColdEmitterDefID() const { return m_definition->GetColdEmitterDefinitionID(); }
	const std::string& GetPointName() const { return m_definition->GetPointName(); }
	int GetParticleType() const { return m_definition->GetParticleType(); }
	int GetAnimationNumber() const { return m_definition->GetAnimationNumber(); }
	int GetAnimationVariation() const { return m_definition->GetAnimationVariation(); }
	int GetAnimationRandomVariation() const { return m_definition->GetAnimationRandomVariation(); }
	int GetStartTime() const { return m_definition->GetStartTime(); }
	int GetLifeSpan() const { return m_definition->GetLifeSpan(); }
	int GetGroundBased() const { return m_definition->GetGroundBased(); }
	int GetPlayWithMat() const { return m_definition->GetPlayWithMat(); }
	int GetSporadic() const { return m_definition->GetSporadic(); }

private:
	ActorParticleDefinition* m_definition;
	ParticlePoint*           m_particlePoint = nullptr;
	ParticleEmitter*         m_emitter = nullptr;
};

class ActorParticleManager
{
public:
	ActorParticleManager(ActorParticleDefinitionManager* definition, HierarchicalModel* model);
	ActorParticleManager(ActorParticleDefinitionManager* definition, SimpleModel* model);
	~ActorParticleManager();

	uint32_t GetNumParticles() const;
	ActorParticle* GetParticle(uint32_t index) const;

	void CreateParticle(SActorParticle* particle, HierarchicalModel* model);
	void CreateParticle(SActorParticle* particle, SimpleModel* model);

	void InitParticles(int mat);
	//void SetupAnimationParticles(...)
	//void ClearAnimationtParticles(...)

private:
	int m_currentMat = 0;
	std::vector<std::unique_ptr<ActorParticle>> m_particles;
	std::vector<std::unique_ptr<ActorParticleDefinition>> m_particleDefinitions;
};

//-------------------------------------------------------------------------------------------------

} // namespace eqg
