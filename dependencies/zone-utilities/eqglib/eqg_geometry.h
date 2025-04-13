
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

class Bone;
using BonePtr = std::shared_ptr<Bone>;

class BoneDefinition;
using BoneDefinitionPtr = std::shared_ptr<BoneDefinition>;

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

class SimpleModel;
using SimpleModelPtr = std::shared_ptr<SimpleModel>;

class ResourceManager;

struct SActorParticle;
struct SParticlePoint;

enum EBones
{
	eBoneNone = -1,
	eBoneHead = 0,
	eBoneHelm,
	eBoneGuild,
	eBonePrimary,
	eBoneSecondary,
	eBoneShield,
	eBoneBloodSpurt,
	eBoneTunic,
	eBoneHair,
	eBoneBeard,
	eBoneEyebrows,
	eBoneChest,
	eBoneLeftBracer,
	eBoneRightBracer,
	eBonePelvis,
	eBoneSpell,
	eBoneCamera,
	eBoneArms,
	eBoneLeftGlove,
	eBoneRightGlove,
	eBoneLegs,
	eBoneLeftBoot,
	eBoneRightBoot,
	eBoneTorch,
	eBoneFacialAttachment,
	eBoneTattoo,
	eBoneLeftShoulder,
	eBoneRightShoulder,
	eNumBones,
};

enum EBoneGroups
{
	eBoneGroupUpperBody = 0,
	eBoneGroupLowerBody,
	eBoneGroupHead,
	eNumBoneGroups,
};

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

//-------------------------------------------------------------------------------------------------

class BoneDefinition
{
public:
	explicit BoneDefinition(const SDagWLDData& wldData);
	explicit BoneDefinition(SEQMBone* boneData);

	const std::string& GetTag() const { return m_tag; }

	ActorDefinitionPtr GetAttachedActorDef() const { return m_attachedActorDef; }

	void AddSubBone(BoneDefinition* subBone);

	BoneDefinition* GetSubBone(uint32_t index) const
	{
		return m_subBones[index];
	}

	uint32_t GetNumSubBones() const
	{
		return static_cast<uint32_t>(m_subBones.size());
	}

	float GetBoneScale() const
	{
		return glm::length(glm::vec3(m_mtx[0]));
	}

	glm::mat4x4& GetMatrix() { return m_mtx; }
	const glm::mat4x4& GetMatrix() const { return m_mtx; }

	glm::mat4x4& GetDefaultPosMatrix() { return m_defaultPoseMtx; }
	const glm::mat4x4& GetDefaultPosMatrix() const { return m_defaultPoseMtx; }

private:
	std::string                   m_tag;

	glm::mat4x4                   m_mtx;
	glm::mat4x4                   m_defaultPoseMtx;
	std::vector<BoneDefinition*>  m_subBones;          // pointer into container owned by the hierarchical model def.

	ActorDefinitionPtr            m_attachedActorDef;
};
using BoneDefinitionPtr = std::shared_ptr<BoneDefinition>;

//-------------------------------------------------------------------------------------------------

class Bone
{
public:
	Bone(const BoneDefinition* boneDef);
	~Bone();

	const std::string& GetTag() const { return m_definition->GetTag(); }

	const glm::mat4x4& GetMatrix() const { return *m_currentMtx; }
	glm::mat4x4& GetMatrix() { return *m_currentMtx; }

	const glm::mat4x4& GetAttachmentMatrix() const { return m_attachmentMtx; }

	void SetAttachedActor(const ActorPtr& actor);
	Actor* GetAttachedActor() const { return m_attachedActor.get(); }
	void SetParentActor(Actor* actor);
	Actor* GetParentActor() const { return m_parentActor; }

	void AddSubBone(Bone* subBone, uint32_t index)
	{
		m_subBones[index] = subBone;
	}

	void AttachToBone(Bone* otherBone);
	void DetachBone();

	void UpdateActorToBoneWorldMatrices(bool updateParticleTtime);

	ActorPtr                      m_attachedActor;
	Actor*                        m_parentActor = nullptr;
	SimpleModelPtr                m_simpleAttachment;

	glm::mat4x4*                  m_currentMtx = nullptr;
	std::vector<Bone*>            m_subBones;
	Bone*                         m_boneAttachedTo = nullptr;
	const BoneDefinition*         m_definition = nullptr;

	glm::mat4x4                   m_positionMtx;
	glm::mat4x4                   m_boneToWorldMtx;
	glm::mat4x4                   m_attachmentMtx;

	enum
	{
		BoneFlag_RootBone                   = 0x01,
		BoneFlag_FirstSkinnedAttachmentBone = 0x02,
		BoneFlag_ApplySRT                   = 0x04
	};
	uint32_t                      m_boneFlags = 0;
};
using BonePtr = std::shared_ptr<Bone>;

class BoneGroup
{
public:
	BoneGroup(int maxBones, int maxAnimations);
	~BoneGroup();

	void AddBone(Bone* bone, bool newBoneNames);

private:
	int m_maxBones;
	int m_maxAnimations;
	std::vector<Bone*> m_bones;
};
using BoneGroupPtr = std::shared_ptr<BoneGroup>;

//-------------------------------------------------------------------------------------------------

class SimpleModelDefinition : public Resource
{
public:
	SimpleModelDefinition();
	~SimpleModelDefinition() override;

	static ResourceType GetStaticResourceType() { return ResourceType::SimpleModelDefinition; }

	std::string_view GetTag() const override { return m_tag; }

	MaterialPalettePtr GetMaterialPalette() const { return m_materialPalette; }

	float GetDefaultBoundingRadius() const { return m_boundingRadius; }
	ECollisionVolumeType GetDefaultCollisionType() const { return m_defaultCollisionType; }

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

// TODO: Create an abstract mesh object
struct SSkinMesh
{
	std::string tag;
	uint32_t attachPointBoneIndex;
	bool hasBlendWeights;
	bool hasBlendIndices;
	bool oldModel;

	struct VertexData
	{
		glm::vec3 vertex;
		glm::vec3 normal;
		glm::vec2 uv;
	};
	std::vector<VertexData> vertices;
	std::vector<uint16_t> indices;
	std::vector<uint32_t> materialIndexTable;
	std::vector<uint32_t> attributes;
};

class HierarchicalModelDefinition : public Resource
{
public:
	HierarchicalModelDefinition();
	~HierarchicalModelDefinition() override;

	static ResourceType GetStaticResourceType() { return ResourceType::HierarchicalModelDefinition; }

	std::string_view GetTag() const override { return m_tag; }

	uint32_t GetNumBones() const { return m_numBones; }
	uint32_t GetNumSubBones() const { return m_numSubBones; }
	const BoneDefinition* GetBoneDefinition(uint32_t boneIndex) const;
	std::vector<BoneDefinition>& GetBones() { return m_bones; }
	bool HasBoneNamed(std::string_view boneName) const;

	uint32_t GetNumAttachedSkins() const { return m_numAttachedSkins; }
	uint32_t GetFirstDefaultActiveSkin() const { return m_firstDefaultActiveSkin; }
	uint32_t GetNumDefaultActiveSkins() const { return m_numDefaultActiveSkins; }
	SSkinMesh* GetAttachedSkin(uint32_t skinIndex) const;
	AnimationPtr GetDefaultAnimation() const { return m_defaultAnim; }

	float GetDefaultBoundingRadius() const { return m_boundingRadius; }
	bool IsNewStyleModel() const { return m_isNewStyleModel; }

	bool InitFromWLDData(std::string_view tag, SHSpriteDefWLDData* pWldData);
	bool InitBonesFromWLDData(SHSpriteDefWLDData* pWldData);
	bool InitSkinsFromWLDData(SHSpriteDefWLDData* pWldData);

	MaterialPalettePtr GetMaterialPalette() const { return m_materialPalette; }

	ParticlePointDefinitionManager* GetPointManager() const { return m_pointManager.get(); }
	ActorParticleDefinitionManager* GetParticleManager() const { return m_particleManager.get(); }

protected:
	std::string                   m_tag;

	glm::vec3                     m_aabbMin = glm::vec3(0.0f);
	glm::vec3                     m_aabbMax = glm::vec3(0.0f);
	glm::vec3                     m_centerOffset = glm::vec3(0.0f);
	float                         m_boundingRadius = 0.0f;

	uint32_t                      m_numBones = 0;
	uint32_t                      m_numSubBones = 0;
	std::vector<BoneDefinition>   m_bones;
	uint32_t                      m_numAttachedSkins = 0;
	uint32_t                      m_firstDefaultActiveSkin = 0;
	uint32_t                      m_numDefaultActiveSkins = 0;
	std::vector<SSkinMesh>        m_attachedSkins;
	AnimationPtr                  m_defaultAnim;
	MaterialPalettePtr            m_materialPalette;

	bool                          m_isNewStyleModel = false;
	bool                          m_hasCollision = false;
	ECollisionVolumeType          m_defaultCollisionType = eCollisionVolumeNone;

	std::unique_ptr<ParticlePointDefinitionManager> m_pointManager;
	std::unique_ptr<ActorParticleDefinitionManager> m_particleManager;

	bool                          m_disableAttachments = false;
	bool                          m_disableShieldAttachments = false;
	bool                          m_disablePrimaryAttachments = false;
	bool                          m_disableSecondaryAttachments = false;
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

	const glm::mat4x4& GetObjectToWorldMatrix() const { return m_worldTransform; }

	ParticlePointManager* GetPointManager() const { return m_pointManager.get(); }
	ParticlePoint* GetParticlePoint(int index) const;
	int GetNumParticlePoints() const;
	ActorParticleManager* GetParticleManager() const { return m_particleManager.get(); }

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

	std::unique_ptr<ParticlePointManager> m_pointManager;
	std::unique_ptr<ActorParticleManager> m_particleManager;

	// generated batches also go here
};
using SimpleModelPtr = std::shared_ptr<SimpleModel>;

//-------------------------------------------------------------------------------------------------

// Per-instance model data for hierarchical models
class HierarchicalModel
{
public:
	using DefinitionType = HierarchicalModelDefinition;
	using DefinitionPtrType = HierarchicalModelDefinitionPtr;

	HierarchicalModel();
	virtual ~HierarchicalModel();

	virtual void Init(const DefinitionPtrType& definition);
	virtual bool InitBatchInstances() { return true; }

	virtual void SetActor(Actor* actor) { m_actor = actor; }
	virtual Actor* GetActor() const { return m_actor; }

	DefinitionPtrType GetDefinition() const { return m_definition; }

	const glm::mat4x4& GetObjectToWorldMatrix() const { return m_worldTransform; }

	ParticlePointManager* GetPointManager() const { return m_pointManager.get(); }
	ParticlePoint* GetParticlePoint(int index) const;
	int GetNumParticlePoints() const;
	ActorParticleManager* GetParticleManager() const { return m_particleManager.get(); }

	bool SetSkinMeshActiveState(uint32_t index, bool active);

	Bone* GetBone(std::string_view tag) const;
	Bone* GetBone(uint32_t index) const;
	void SetBoneParent(Actor* actor);

	void UpdateBoneToWorldMatrices(glm::mat4x4* parentMatrix = nullptr);
	void UpdateBoneToWorldMatrices(Bone* bone, glm::mat4x4* parentMatrix);

private:
	DefinitionPtrType             m_definition;
	Actor*                        m_actor = nullptr; // owning actor
	glm::mat4x4                   m_worldTransform;  // object to world transform matrix
	std::vector<BonePtr>          m_bones;
	uint32_t                      m_activeMeshes = 0;

	MaterialPalettePtr            m_materialPalette; // copy of the material palette from the definition

	std::unique_ptr<ParticlePointManager> m_pointManager;
	std::unique_ptr<ActorParticleManager> m_particleManager;
};
using HierarchicalModelPtr = std::shared_ptr<HierarchicalModel>;

//-------------------------------------------------------------------------------------------------

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

	Actor* GetTopLevelActor();

	void SetParentActor(Actor* actor) { m_parentActor = actor; }
	Actor* GetParentActor() const { return m_parentActor; }

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

	void SetTerrainObject(TerrainObject* terrainObject) { m_terrainObject = terrainObject; }
	TerrainObject* GetTerrainObject() const { return m_terrainObject; }

	void SetHasParentBone(bool hasParentBone) { m_hasParentBone = hasParentBone; }
	bool HasParentBone() const { return m_hasParentBone; }

	bool IsDisabled() const { return m_disabled; }
	void SetDisabled(bool disabled) { m_disabled = disabled; }

	bool IsInvisible() const { return m_invisible; }
	void SetInvisible(bool invisible) { m_invisible = invisible; }

	// Used when in first person camera mode to see your own particles
	bool ShouldShowParticlesWhenInvisible() const { return m_showParticlesWhenInvisible; }
	void SetShowParticlesWhenInvisible(bool show) { m_showParticlesWhenInvisible = show; }

protected:
	ResourceManager*               m_resourceMgr;

	std::string                    m_tag;
	std::string                    m_actorName;

	ActorDefinitionPtr             m_definition;
	Actor*                         m_parentActor = nullptr;
	glm::mat4x4                    m_transform = glm::mat4x4(1.0f);

	glm::vec3                      m_position = glm::vec3(0.0f);
	glm::vec3                      m_orientation = glm::vec3(0.0f);
	float                          m_scale = 1.0f;

	int                            m_actorIndex = 0;
	ECollisionVolumeType           m_collisionVolumeType = eCollisionVolumeNone;
	float                          m_boundingRadius = 0.0f;
	EActorType                     m_actorType = eActorTypeUndefined;
	TerrainObject*                 m_terrainObject = nullptr;
	bool                           m_hasParentBone = false;

	bool                           m_disabled = false;
	bool                           m_invisible = false;
	bool                           m_showParticlesWhenInvisible = false;

protected:
	void DoInitCallback();
};

//-------------------------------------------------------------------------------------------------

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
	SimpleActor(
		ResourceManager* resourceMgr,
		std::string_view actorTag,
		const ActorDefinitionPtr& actorDef,
		int actorIndex,
		bool useDefaultBoundingRadius = false,
		std::string_view actorName = "");
	~SimpleActor() override;

	SimpleModelPtr GetSimpleModel() const override { return m_model; }
	SimpleModelDefinitionPtr GetSimpleModelDefinition() const override { return m_model->GetDefinition(); }

	SimpleModelPtr GetCollisionModel() const { return m_collisionModel; }

private:
	void InitLOD();

	SimpleModelPtr m_model;
	SimpleModelPtr m_collisionModel;

	struct LODModel
	{
		SimpleModelPtr model;
		float distance;
	};
	std::vector<LODModel> m_lodModels;
};

//-------------------------------------------------------------------------------------------------

class HierarchicalActor : public Actor
{
public:
	HierarchicalActor(
		ResourceManager* resourceMgr,
		std::string_view actorTag,
		const ActorDefinitionPtr& actorDef,
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

	HierarchicalActor(
		ResourceManager* resourceMgr,
		std::string_view actorTag,
		const ActorDefinitionPtr& actorDef,
		int actorIndex,
		bool allSkinsActive = false,
		bool useDefaultBoundingRadius = false,
		bool sharedBoneGroups = true,
		Bone* pBone = nullptr,
		std::string_view actorName = "");

	~HierarchicalActor() override;

	HierarchicalModelPtr GetHierarchicalModel() const override { return m_model; }
	HierarchicalModelDefinitionPtr GetHierarchicalModelDefinition() const override { return m_model->GetDefinition(); }

	SimpleModelPtr GetCollisionModel() const { return m_collisionModel; }

private:
	void InitLOD();
	void SetAllSkinsActive();
	void PutAllBonesInBoneGroup(int groupIndex, int maxNumAnims, bool newBoneNames);
	void AttachLODModels(HierarchicalModel* parent, HierarchicalModel* child);

	HierarchicalModelPtr     m_model;
	SimpleModelPtr           m_collisionModel;

	struct LODModel
	{
		HierarchicalModelPtr model;
		float distance;
	};
	std::vector<LODModel>    m_lodModels;

	std::vector<Bone*>       m_bones;
	std::vector<BoneGroupPtr> m_boneGroups;

	uint32_t                 m_headSkins = 0;
	uint32_t                 m_bodySkins = 0;
};

//-------------------------------------------------------------------------------------------------

class EmitterInterface;

class ParticleActor : public Actor
{
public:
	ParticleActor(
		ResourceManager* resourceMgr,
		std::string_view actorTag,
		const ActorDefinitionPtr& actorDef,
		int actorIndex,
		bool allSkinsActive = false,
		Bone* bone = nullptr);

	virtual ~ParticleActor() override;

	EmitterInterface* m_emitter = nullptr;
};

} // namespace eqg
