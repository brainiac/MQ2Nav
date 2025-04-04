
#pragma once

#include "eqg_animation.h"
#include "eqg_resource.h"
#include "eqg_particles.h"
#include "eqg_structs.h"
#include "eqg_types_fwd.h"
#include "wld_types.h"

#include <glm/glm.hpp>
#include <chrono>
#include <vector>

namespace eqg {

// ModelDefinition, Model, ActorDefinition, ActorInstance, all very confusing objects
// in the EQG engine.
//
// Class hiearchies look something like this (some details ommitted):
// 
// Model definitions:
// * CSimpleModelDefinition -> TSimpleModelDefinitionBase -> CSimpleModelDefinitionBase
// * CActorDefinition -> CActorDefinitionInterface
//       CActorDefinitionDataClient -> CActorDefinitionData -> CActorDefinitionDataBase
//
// Model instances:
// * CSimpleModel -> CSimpleModelBase 
// * CHierarchicalModel -> CHierarchicalModelBase
// 
// Actors:
// * CSimpleActor -> CActor -> CActorInterface -> CActorInterfaceBase
//       CSimpleActorDataClient -> CSimpleActorData -> CActorData -> CActorDataBase
// * CHierarchicalActor -> CActor -> CActorInterface -> CActorInterfaceBase
//       CHierarchicalActorDataClient -> CHierarchicalActorData -> CActorData -> CActorDataBase
//
// Basically, you can think of in simpler terms:
//
// - CSimpleModel is an instance of CSimpleModelDefinition
// - CActorDefinition is an abstraction around CSimpleModelDefinition and CHierarchicalModelDefinition
// - CActor is an instance of CActorDefinition. A CActor can be a CSimpleActor or a CHierarchicalActor,
//   depending on the type of model (which is abstracted away). CSimpleActor owns the model.
//
// there might also be other types of actors, but I did not list them here.
//
// In the EQG engine, Actor has a copy of some information, like the object to world transform. When an actor
// moves it will update both at the same time.

using std::chrono::steady_clock;

struct SDMSpriteDef2WLDData;
struct SHSpriteDefWLDData;

class MaterialPalette;
using MaterialPalettePtr = std::shared_ptr<MaterialPalette>;

class ActorDefinition;
using ActorDefinitionPtr = std::shared_ptr<ActorDefinition>;

class Actor;
using ActorPtr = std::shared_ptr<Actor>;

class ParticleCloudDefinition;
using ParticleCloudDefinitionPtr = std::shared_ptr<ParticleCloudDefinition>;

class ParticlePointDefinitionManager;

class ResourceManager;

struct SActorParticle;
struct SParticlePoint;

enum ECollisionVolumeType
{
	eCollisionVolumeNone = 0,
	eCollisionVolumeModel,
	eCollisionVolumeSphere,
	eCollisionVolumeDag,
	eCollisionVolumeBox,
};


struct SFace
{
	glm::u32vec3  indices;
	uint16_t      flags; // EQG_FACEFLAGS
	int16_t       materialIndex;

	SFace() = default; // default uninit

	bool IsCollidable() const
	{
		return (flags & EQG_FACEFLAG_COLLISION_REQUIRED) != 0
			|| (flags & EQG_FACEFLAG_PASSABLE) == 0;
	}

	bool IsPassable() const
	{
		return (flags & EQG_FACEFLAG_PASSABLE) != 0;
	}
	
	bool IsTransparent() const
	{
		return (flags & EQG_FACEFLAG_TRANSPARENT) != 0;
	}
};

struct SMaterialGroup
{
	uint32_t vertexStart;
	uint32_t vertexCount;
	uint32_t faceStart;
	uint32_t faceCount;
};

class Geometry
{
public:
	std::string tag;

	std::vector<SEQMVertex> vertices;
	std::vector<SFace> faces;

	MaterialPalettePtr material_palette;
};


class SimpleModelDefinition : public Resource
{
public:
	SimpleModelDefinition();
	~SimpleModelDefinition() override;

	static ResourceType GetStaticResourceType() { return ResourceType::SimpleModelDefinition; }

	std::string_view GetTag() const override { return m_tag; }

	MaterialPalettePtr GetMaterialPalette() const { return m_materialPalette; }

	float GetDefaultBoundingRadius() const { return m_boundingRadius; }

	bool InitFromWLDData(std::string_view tag, SDMSpriteDef2WLDData* pWldData);
	bool InitFromEQMData(std::string_view tag,
		uint32_t numVertices, SEQMVertex* vertices,
		uint32_t numFaces, SEQMFace* faces,
		uint32_t numPoints, SParticlePoint* points,
		uint32_t numParticles, SActorParticle* particles,
		const MaterialPalettePtr& materialPalette);
	virtual bool InitStaticData(); // Initializes materials, batches, etc

	ParticlePointDefinitionManager* GetPointManager() const { return m_pointManager.get(); }
	ActorParticleDefinitionManager* GetParticleManager() const { return m_particleManager.get(); }

protected:
	void InitVerticesFromEQMData(uint32_t numVertices, SEQMVertex* vertices);
	void InitFacesFromEQMData(uint32_t numFaces, SEQMFace* faces);
	void InitCollisionData(bool forceModel);

public:
	std::string                   m_tag;

	uint32_t                      m_numVertices = 0;
	uint32_t                      m_numFaces = 0;
	uint32_t                      m_numFrames = 1;
	std::vector<glm::vec3>        m_vertices;
	std::vector<glm::vec2>        m_uvs;
	std::vector<glm::vec2>        m_uv2s;
	std::vector<glm::vec3>        m_normals;
	std::vector<uint32_t>         m_colors;
	std::vector<uint32_t>         m_colorTint;
	std::vector<SFace>            m_faces;
	std::vector<glm::vec3>        m_faceNormals;
	std::vector<uint32_t>         m_tangents;
	std::vector<uint32_t>         m_binormals;

	glm::vec3                     m_aabbMin = glm::vec3(0.0f);
	glm::vec3                     m_aabbMax = glm::vec3(0.0f);
	glm::vec3                     m_centerOffset = glm::vec3(0.0f);
	float                         m_boundingRadius = 0.0f;

	std::unique_ptr<ParticlePointDefinitionManager> m_pointManager;
	std::unique_ptr<ActorParticleDefinitionManager> m_particleManager;

	bool                          m_hasCollision = false;
	bool                          m_useLitBatches = false;
	ECollisionVolumeType          m_defaultCollisionType = eCollisionVolumeNone;

	steady_clock::time_point      m_lastUpdate = steady_clock::now();
	uint32_t                      m_updateInterval = 0;
	MaterialPalettePtr            m_materialPalette;
	std::vector<SMaterialGroup>   m_materialGroups;
};
using SimpleModelDefinitionPtr = std::shared_ptr<SimpleModelDefinition>;

struct BoneDefinition
{
	explicit BoneDefinition(const SDagWLDData& wldData);

	void AddSubBone(BoneDefinition* subBone);

	std::string_view GetTag() const { return m_tag; }

	std::string                   m_tag;

	glm::mat4x4                   m_mtx;
	glm::mat4x4                   m_defaultPoseMtx;
	std::vector<BoneDefinition*>  m_subBones;          // pointer into container owned by the hierarchical model def.

	ActorDefinitionPtr            m_attachedActor;
};

class HierarchicalModelDefinition : public Resource
{
public:
	HierarchicalModelDefinition();
	~HierarchicalModelDefinition() override;

	static ResourceType GetStaticResourceType() { return ResourceType::HierarchicalModelDefinition; }

	std::string_view GetTag() const override { return m_tag; }

	uint32_t GetNumBones() const { return m_numBones; }
	const BoneDefinition* GetBoneDefinition(uint32_t boneIndex) const;

	float GetDefaultBoundingRadius() const { return m_boundingRadius; }

	bool InitFromWLDData(std::string_view tag, SHSpriteDefWLDData* pWldData);

	ParticlePointDefinitionManager* GetPointManager() const { return nullptr; }

protected:
	std::string                   m_tag;

	glm::vec3                     m_aabbMin = glm::vec3(0.0f);
	glm::vec3                     m_aabbMax = glm::vec3(0.0f);
	glm::vec3                     m_centerOffset = glm::vec3(0.0f);
	float                         m_boundingRadius = 0.0f;

	uint32_t                      m_numBones = 0;
	uint32_t                      m_numSubBones = 0;
	std::vector<BoneDefinition>   m_bones;

	bool                          m_hasCollision = false;
	ECollisionVolumeType          m_defaultCollisionType = eCollisionVolumeNone;

	MaterialPalettePtr            m_materialPalette;
};
using HierarchicalModelDefinitionPtr = std::shared_ptr<HierarchicalModelDefinition>;

enum CallbackTagType
{
	UnusedCallback,
	SpriteCallback,
	SpriteCallback2,
	LadderCallback,
	TreeCallback,
};

// An actor definition is either a SimpleModelDefinition or a HierarchicalModelDefinition.
class ActorDefinition : public Resource
{
public:
	ActorDefinition(std::string_view tag, const SimpleModelDefinitionPtr& simpleModel);
	ActorDefinition(std::string_view tag, const HierarchicalModelDefinitionPtr& hierchicalModel);
	ActorDefinition(std::string_view tag, const ParticleCloudDefinitionPtr& particleCloudDef);

	~ActorDefinition() override;

	static ResourceType GetStaticResourceType() { return ResourceType::ActorDefinition; }

	std::string_view GetTag() const override { return m_tag; }

	const SimpleModelDefinitionPtr& GetSimpleModelDefinition() const { return m_simpleModelDefinition; }
	const HierarchicalModelDefinitionPtr& GetHierarchicalModelDefinition() const { return m_hierarchicalModelDefinition; }
	const ParticleCloudDefinitionPtr& GetParticleCloudDefinition() const { return m_particleCloudDefinition; }

	void SetCallbackTag(std::string_view tag); // S3D uses this
	void SetCallbackType(CallbackTagType type); // EQG uses this
	const std::string& GetCallbackTag() const { return m_callbackTag; }
	CallbackTagType GetCallbackType() const { return m_callbackType; }

	void SetCollisionVolumeType(ECollisionVolumeType type) { m_collisionVolumeType = type; }
	ECollisionVolumeType GetCollisionVolumeType() const { return m_collisionVolumeType; }

	float CalculateBoundingRadius();

protected:
	std::string                    m_tag;

	std::string                    m_callbackTag;
	CallbackTagType                m_callbackType = UnusedCallback;
	ECollisionVolumeType           m_collisionVolumeType = eCollisionVolumeNone;
	float                          m_boundingRadius = 0.0f;

	SimpleModelDefinitionPtr       m_simpleModelDefinition;
	HierarchicalModelDefinitionPtr m_hierarchicalModelDefinition;
	ParticleCloudDefinitionPtr     m_particleCloudDefinition;
};

//=================================================================================================
//=================================================================================================

// Per-instance model data for a simple model
class SimpleModel
{
public:
	SimpleModel();
	virtual ~SimpleModel();

	virtual bool Init(const SimpleModelDefinitionPtr& definition);
	virtual bool InitBatchInstances() { return true; } // TODO: Implement me

	virtual void SetActor(Actor* actor) { m_actor = actor; }
	virtual Actor* GetActor() const { return m_actor; }

	SimpleModelDefinitionPtr GetDefinition() const { return m_definition; }

	bool SetRGBs(SDMRGBTrackWLDData* pDMRGBTrackWLDData);
	bool SetRGBs(uint32_t* pRGBs, uint32_t numRGBs);

	SimpleModelDefinitionPtr      m_definition;
	Actor*                        m_actor = nullptr; // owning actor
	glm::mat4x4                   m_worldTransform;  // object to world transform matrix

	uint32_t                      m_currentFrame;
	steady_clock::time_point      m_lastUpdate;
	steady_clock::time_point      m_nextUpdate;
	float                         m_shadeFactor; // attenuation on global directional light (sunlight)
	std::vector<uint32_t>         m_bakedDiffuseLighting;  // used for diffuse lighting colors per polygon

	MaterialPalettePtr            m_materialPalette; // copy of the material palette from the definition
	
	// generated batches also go here
};
using SimpleModelPtr = std::shared_ptr<SimpleModel>;

// Per-instance model data for hierarchical models
class HierarchicalModel
{
public:
	HierarchicalModel();
	virtual ~HierarchicalModel();

	virtual void Init(const HierarchicalModelDefinitionPtr& definition);
	virtual bool InitBatchInstances() { return true; }

	HierarchicalModelDefinitionPtr GetDefinition() const { return m_definition; }

	HierarchicalModelDefinitionPtr m_definition;
	Actor*                m_actor; // owning actor
	glm::mat4x4                   m_worldTransform;  // object to world transform matrix

	MaterialPalettePtr            m_materialPalette; // copy of the material palette from the definition
};
using HierarchicalModelPtr = std::shared_ptr<HierarchicalModel>;

enum EActorType
{
	eActorTypeUndefined      = 0,
	eActorTypePlayer         = 1,
	eActorTypeCorpse         = 2,
	eActorTypeSwitch         = 3,
	eActorTypeMissile        = 4,
	eActorTypeObject         = 5,
	eActorTypeLadder         = 6,
	eActorTypeTree           = 7,
	eActorTypeLargeObject    = 8,
	eActorTypePlacedItem     = 9,
};

class Actor
{
public:
	Actor(ResourceManager* resourceMgr);
	virtual ~Actor();

	const std::string& GetTag() const { return m_tag; }
	const std::string& GetActorName() const { return m_actorName; }

	ActorDefinitionPtr GetDefinition() const { return m_definition; }

	virtual SimpleModelPtr GetSimpleModel() const { return nullptr; }
	virtual SimpleModelDefinitionPtr GetSimpleModelDefinition() const { return nullptr; }

	virtual HierarchicalModelPtr GetHierarchicalModel() const { return nullptr; }
	virtual HierarchicalModelDefinitionPtr GetHierarchicalModelDefinition() const { return nullptr; }

	void SetPosition(const glm::vec3& pos) { m_position = pos; }
	const glm::vec3& GetPosition() const { return m_position; }

	void SetOrientation(const glm::vec3& orientation) { m_orientation = orientation; }
	const glm::vec3& GetOrientation() const { return m_orientation; }

	void SetScale(float scale) { m_scale = scale; }
	float GetScale() const { return m_scale; }

	void SetBoundingRadius(float radius, bool adjustScale = false);
	float GetBoundingRadius() const { return m_boundingRadius * m_scale; }

	void SetCollisionVolumeType(ECollisionVolumeType collisionVolume) { m_collisionVolumeType = collisionVolume; }
	ECollisionVolumeType GetCollisionVolumeType() const { return m_collisionVolumeType; }

	void SetActorType(EActorType type) { m_actorType = type; }
	EActorType GetActorType() const { return m_actorType; }

	// TODO: Disabled flag

	ResourceManager*               m_resourceMgr;

	std::string                    m_tag;
	std::string                    m_actorName;

	ActorDefinitionPtr             m_definition;
	glm::mat4x4                    m_transform = glm::mat4x4(1.0f);

	glm::vec3                      m_position = glm::vec3(0.0f);
	glm::vec3                      m_orientation = glm::vec3(0.0f);
	float                          m_scale = 1.0f;

	int                            m_actorIndex = 0;
	ECollisionVolumeType           m_collisionVolumeType = eCollisionVolumeNone;
	float                          m_boundingRadius = 0.0f;
	EActorType                     m_actorType = eActorTypeUndefined;

protected:
	void DoInitCallback();
};

class SimpleActor : public Actor
{
public:
	SimpleActor(
		ResourceManager* resourceMgr,
		std::string_view actorTag,
		const ActorDefinitionPtr& actorDef,
		const glm::vec3& position,
		const glm::vec3& orientation,
		float scalFactor,
		float boundingRadius,
		ECollisionVolumeType collisionVolumeType,
		int actorIndex,
		SDMRGBTrackWLDData* DMRGBTrackWLDData = nullptr,
		uint32_t* RGBs = nullptr,
		uint32_t numRGBs = 0,
		std::string_view actorName = ""
	);
	~SimpleActor() override;

	SimpleModelPtr GetSimpleModel() const override { return m_model; }
	SimpleModelDefinitionPtr GetSimpleModelDefinition() const override { return m_model->GetDefinition(); }

	SimpleModelPtr m_model;
	SimpleModelPtr m_collisionModel;

	struct LODModel
	{
		SimpleModelPtr model;
		float distance;
	};
	std::vector<LODModel> m_lodModels;
};

class HierarchicalActor : public Actor
{
public:
	HierarchicalActor(
		ResourceManager* resourceMgr,
		std::string_view actorTag,
		ActorDefinitionPtr actorDef,
		const glm::vec3& position,
		const glm::vec3& orientation,
		float scaleFactor,
		float boundingRadius,
		ECollisionVolumeType collisionVolumeType,
		int actorIndex,
		SDMRGBTrackWLDData* DMRGBTrackWLDData = nullptr,
		uint32_t* RGBs = nullptr,
		uint32_t numRGBs = 0,
		std::string_view actorName = ""
	);
	~HierarchicalActor() override;

	HierarchicalModelPtr GetHierarchicalModel() const override { return m_model; }
	HierarchicalModelDefinitionPtr GetHierarchicalModelDefinition() const override { return m_model->GetDefinition(); }

	HierarchicalModelPtr m_model;
};


} // namespace eqg
