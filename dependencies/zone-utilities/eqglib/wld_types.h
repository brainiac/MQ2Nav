
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace eqg {

class ActorInstance;
class ActorDefinition;
class Bitmap;
class HierarchicalModelDefinition;
class Material;
class MaterialPalette;
class ParticleCloudDefinition;
class SimpleModelDefinition;
class WLDLoader;
struct STrack;

using ActorDefinitionPtr = std::shared_ptr<ActorDefinition>;

enum WLDObjectType : uint32_t
{
	WLD_NONE                                  = 0,
	WLD_OBJ_DEFAULTPALETTEFILE_TYPE           = 1,
	WLD_OBJ_WORLD_USERDATA_TYPE               = 2,
	WLD_OBJ_BMINFO_TYPE                       = 3,
	WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE       = 4,
	WLD_OBJ_SIMPLESPRITEINSTANCE_TYPE         = 5,

	WLD_OBJ_HIERARCHICALSPRITEDEFINITION_TYPE = 16, // (0x10)
	WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE   = 17, // (0x11)
	WLD_OBJ_TRACKDEFINITION_TYPE              = 18, // (0x12)
	WLD_OBJ_TRACKINSTANCE_TYPE                = 19, // (0x13)
	WLD_OBJ_ACTORDEFINITION_TYPE              = 20, // (0x14)
	WLD_OBJ_ACTORINSTANCE_TYPE                = 21, // (0x15)

	WLD_OBJ_LIGHTDEFINITION_TYPE              = 27, // (0x1B)
	WLD_OBJ_LIGHTINSTANCE_TYPE                = 28, // (0x1C)

	WLD_OBJ_WORLDTREE_TYPE                    = 33, // (0x21)
	WLD_OBJ_REGION_TYPE                       = 34, // (0x22)

	WLD_OBJ_BLITSPRITEDEFINITION_TYPE         = 38, // (0x26)
	WLD_OBJ_BLITSPRITEINSTANCE_TYPE           = 39, // (0x27)
	WLD_OBJ_POINTLIGHT_TYPE                   = 40, // (0x28)
	WLD_OBJ_ZONE_TYPE                         = 41, // (0x29)

	WLD_OBJ_DMSPRITEDEFINITION_TYPE           = 44, // (0x2C)
	WLD_OBJ_DMSPRITEINSTANCE_TYPE             = 45, // (0x2D)

	WLD_OBJ_MATERIALDEFINITION_TYPE           = 48, // (0x30)
	WLD_OBJ_MATERIALPALETTE_TYPE              = 49, // (0x31)

	WLD_OBJ_DMRGBTRACKDEFINITION_TYPE         = 50, // (0x32)
	WLD_OBJ_DMRGBTRACKINSTANCE_TYPE           = 51, // (0x33)
	WLD_OBJ_PCLOUDDEFINITION_TYPE             = 52, // (0x34)
	WLD_OBJ_CONSTANTAMBIENT_TYPE              = 53, // (0x35)
	WLD_OBJ_DMSPRITEDEFINITION2_TYPE          = 54, // (0x36)

	WLD_OBJ_LAST_TYPE                         = 56,
};

// flags for actor objects
enum WLDOBJ_ACTOROPT
{
	WLD_OBJ_ACTOROPT_HAVECURRENTACTION               = 0x0001,
	WLD_OBJ_ACTOROPT_HAVELOCATION                    = 0x0002,
	WLD_OBJ_ACTOROPT_HAVEBOUNDINGRADIUS              = 0x0004,
	WLD_OBJ_ACTOROPT_HAVESCALEFACTOR                 = 0x0008,
	WLD_OBJ_ACTOROPT_HAVESOUND                       = 0x0010,
	WLD_OBJ_ACTOROPT_ACTIVE                          = 0x0020,
	WLD_OBJ_ACTOROPT_ACTIVEGEOMETRY                  = 0x0040,
	WLD_OBJ_ACTOROPT_SPRITEVOLUMEONLY                = 0x0080,
	WLD_OBJ_ACTOROPT_HAVEDMRGBTRACK                  = 0x0100,
	WLD_OBJ_ACTOROPT_USEBOUNDINGBOX                  = 0x0200,
};

// flags for light objects
enum WLDOBJ_LIGHTOPT
{
	WLD_OBJ_LIGHTOPT_HAVECURRENTFRAME                = 0x0001,
	WLD_OBJ_LIGHTOPT_HAVESLEEP                       = 0x0002,
	WLD_OBJ_LIGHTOPT_HAVESKIPFRAMES                  = 0x0004,
	WLD_OBJ_LIGHTOPT_SKIPFRAMES                      = 0x0008,
	WLD_OBJ_LIGHTOPT_HAVECOLORS                      = 0x0010,
};

// flags for material objects
enum WLDOBJ_MATOPT
{
	WLD_OBJ_MATOPT_TWOSIDED                          = 0x0001,
	WLD_OBJ_MATOPT_HASUVSHIFTPERMS                   = 0x0002,
};

// flags for particle cloud emitters
enum WLDOBJ_PCLOUDOPT
{
	WLD_OBJ_PCLOUDOPT_HASSPAWNBOX                    = 0x0001,
	WLD_OBJ_PCLOUDOPT_HASBBOX                        = 0x0002,
	WLD_OBJ_PCLOUDOPT_HASSPRITEDEF                   = 0x0004,
};

// flags for region objects
enum WLDOBJ_REGOPT
{
	WLD_OBJ_REGOPT_HAVESPHERE                        = 0x0001,
	WLD_OBJ_REGOPT_HAVEREVERBVOLUME                  = 0x0002,
	WLD_OBJ_REGOPT_HAVEREVERBOFFSET                  = 0x0004,
	WLD_OBJ_REGOPT_REGIONFOG                         = 0x0008,
	WLD_OBJ_REGOPT_ENABLE_GOURAUD2                   = 0x0010,
	WLD_OBJ_REGOPT_ENCODEDVISIBILITY                 = 0x0020,
	WLD_OBJ_REGOPT_HAVEREGIONDMSPRITE                = 0x0040,
	WLD_OBJ_REGOPT_ENCODEDVISIBILITY2                = 0x0080,
	WLD_OBJ_REGOPT_HAVEREGIONDMSPRITEDEF             = 0x0100,
};

// flags for sprite objects
enum WLDOBJ_SPROPT
{
	WLD_OBJ_SPROPT_HAVECENTEROFFSET                  = 0x0001,
	WLD_OBJ_SPROPT_HAVEBOUNDINGRADIUS                = 0x0002,
	WLD_OBJ_SPROPT_HAVECURRENTFRAME                  = 0x0004,
	WLD_OBJ_SPROPT_HAVESLEEP                         = 0x0008,
	WLD_OBJ_SPROPT_HAVESKIPFRAMES                    = 0x0010,

	WLD_OBJ_SPROPT_SKIPFRAMES                        = 0x0040,

	WLD_OBJ_SPROPT_HAVEATTACHEDSKINS                 = 0x0200,

	WLD_OBJ_SPROPT_SPRITEDEFPOLYHEDRON               = 0x10000,
	WLD_OBJ_SPROPT_DAGCOLLISIONS                     = 0x20000,
};

// flags for track instances
enum WLDOBJ_TRKOPT
{
	WLD_OBJ_TRKOPT_HAVESLEEP                         = 0x0001,
	WLD_OBJ_TRKOPT_REVERSE                           = 0x0002,
	WLD_OBJ_TRKOPT_INTERPOLATE                       = 0x0004,
};

enum S3D_FACEFLAGS
{
	S3D_FACEFLAG_PASSABLE = 0x10
};

struct SLocation
{
	float x, y, z;
	float heading;
	float pitch;
	float roll;
	uint32_t region;
};

struct SWLDHeader
{
	uint32_t magic;
	uint32_t version;
	uint32_t fragments;
	uint32_t unk1;
	uint32_t unk2;
	uint32_t hash_length;
	uint32_t unk3;
};

struct SPlanarEquation
{
	glm::vec3 normal;
	float dist;
};

struct STextureDataDefinition
{
	uint32_t columns = 0;
	uint32_t rows = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t numFrames = 0;
	uint32_t currentFrame = 0;
	uint32_t updateInterval = 1;
	uint32_t renderMethod = 0;
	bool valid = false;
	bool skipFrames = false;

	std::vector<std::shared_ptr<Bitmap>> sourceTextures;
};

// WLD_OBJ_BMINFO_TYPE (0x3)
struct WLD_OBJ_BMINFO 
{
	int tag;
	uint32_t num_mip_levels;
	//uint16_t filepath_length;
};

struct WLD_OBJ_BLITSPRITEDEFINITION
{
	int tag;
	uint32_t flags;
	int simple_sprite_id;
	uint32_t render_method;
};

struct WLD_OBJ_BLITSPRITEINSTANCE
{
	int tag;
	uint32_t definition_id;
	uint32_t flags;
};

// WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE (0x4)
struct WLD_OBJ_SIMPLESPRITEDEFINITION 
{
	int tag;
	uint32_t flags;
	uint32_t num_frames;
};

// WLD_OBJ_SIMPLESPRITEINSTANCE_TYPE (0x5)
struct WLD_OBJ_SIMPLESPRITEINSTANCE
{
	int tag;
	int definition_id;
	uint32_t flags;
};

struct WLD_OBJ_HIERARCHICALSPRITEDEFINITION
{
	int tag;
	uint32_t flags;
	uint32_t num_dags;
	uint32_t collision_volume_id;
};

struct WLD_OBJ_HIERARCHICALSPRITEINSTANCE
{
	int tag;
	uint32_t definition_id;
	uint32_t flags;
};

struct WLDDATA_DAG
{
	int tag;
	uint32_t flags;
	int track_id; // id of track
	int sprite_id; // id of sprite instance
	uint32_t num_sub_dags;
};

struct WLD_OBJ_TRACKDEFINITION
{
	int tag;
	uint32_t flags;
	uint32_t num_frames;
};

struct WLD_OBJ_TRACKINSTANCE
{
	int tag;
	uint32_t track_id;
	uint32_t flags;
};

struct WLD_OBJ_DMTRACKDEFINITION2
{
	int tag;
	uint32_t flags;
	uint16_t num_vertices;
	uint16_t num_frames;
	uint16_t sleep;
	uint16_t current_frame;
	uint16_t scale;
};

struct EQG_S3D_PFRAMETRANSFORM
{
	int16_t rot_q0;
	int16_t rot_q1;
	int16_t rot_q2;
	int16_t rot_q3;

	int16_t pivot_x;
	int16_t pivot_y;
	int16_t pivot_z;

	int16_t scale;
};

struct WLD_OBJ_ACTORDEFINITION
{
	int tag;
	uint32_t flags;
	int callback_id;
	uint32_t num_actions;
	uint32_t num_sprites;
	int collision_volume_id;
};

struct WLD_OBJ_ACTORINSTANCE
{
	int tag;
	int actor_def_id;
	uint32_t flags;
	int collision_volume_id;
};

struct WLD_OBJ_LIGHTDEFINITION
{
	int tag;
	uint32_t flags;
	uint32_t num_frames;
};

struct WLD_OBJ_LIGHTINSTANCE
{
	int tag;
	uint32_t definition_id;
	uint32_t flags;
};

struct WLD_OBJ_WORLDTREE
{
	int tag;
	uint32_t num_world_nodes;
};

struct WLD_OBJ_WORLDTREE_NODE
{
	SPlanarEquation plane;
	uint32_t region;
	uint32_t node[2];
};

struct WLD_OBJ_REGION
{
	int tag;
	uint32_t flags;
	int ambient_light_id;
	uint32_t num_vertices;
	uint32_t num_proximal_regions;
	uint32_t num_render_vertices;
	uint32_t num_walls;
	uint32_t num_obstacles;
	uint32_t num_cutting_obstacles;
	uint32_t num_vis_nodes;
	uint32_t num_vis_lists;
};

struct WLD_OBJ_POINTLIGHT
{
	int tag;
	int light_id;
	uint32_t flags;
	glm::vec3 pos;
	float radius;
};

struct WLD_OBJ_ZONE
{
	int tag;
	uint32_t flags;
	uint32_t num_regions;
};

struct WLD_OBJ_DMRGBTRACKINSTANCE
{
	int tag;
	int definition_id;
	uint32_t flags;
};

struct WLD_OBJ_DMRGBTRACKDEFINITION
{
	int tag;
	uint32_t flags;
	uint32_t num_vertices;
	uint32_t num_frames;
	uint32_t sleep;
	uint32_t current_frame;
};

struct WLD_OBJ_DMSPRITEINSTANCE
{
	int tag;
	int definition_id;
	uint32_t flags;
};

struct WLD_OBJ_MATERIALDEFINITION
{
	int tag;
	uint32_t flags;
	uint32_t render_method;
	uint32_t color;
	float brightness;
	float scaled_ambient;
	int sprite_or_bminfo;
};

struct WLD_OBJ_MATERIALPALETTE
{
	int tag;
	uint32_t flags;
	uint32_t num_entries;
};

struct WLD_OBJ_PCLOUDDEFINITION
{
	int tag;
	uint32_t flags;
	uint32_t particle_type;
	uint32_t spawn_type;
	uint32_t pcloud_flags;
	uint32_t size;
	float gravity_multiplier;
	glm::vec3 gravity;
	uint32_t duration;
	float spawn_radius;
	float spawn_angle;
	uint32_t lifespan;
	float spawn_velocity_multiplier;
	glm::vec3 spawn_velocity;
	uint32_t spawn_rate;
	float spawn_scale;
	uint32_t tint;
};

struct BOUNDINGBOX
{
	glm::vec3 min;
	glm::vec3 max;
};

struct WLD_OBJ_DMSPRITEDEFINITION2
{
	int tag;
	uint32_t flags;
	uint32_t material_palette_id;
	uint32_t dm_track_id;
	uint32_t dmrgb_track_id;
	uint32_t collision_volume_id;
	glm::vec3 center_offset;
	glm::vec3 hotspot;
	float bounding_radius;
	BOUNDINGBOX bounding_box;
	uint16_t num_vertices;
	uint16_t num_uvs;
	uint16_t num_vertex_normals;
	uint16_t num_rgb_colors;
	uint16_t num_faces;
	uint16_t num_skin_groups;
	uint16_t num_fmaterial_groups;
	uint16_t num_vmaterial_groups;
	uint16_t num_mesh_ops;
	int16_t scale;
};

struct WLD_PVERTEX
{
	int16_t x;
	int16_t y;
	int16_t z;
};

struct WLD_PUV
{
	float u;
	float v;
};

struct WLD_PUVOLDFORM
{
	uint16_t u;
	uint16_t v;
};

struct WLD_PNORMAL
{
	uint8_t x;
	uint8_t y;
	uint8_t z;
};

struct WLD_DMFACE2
{
	uint16_t flags;
	glm::u16vec3 indices;
};

struct WLD_SKINGROUP
{
	uint16_t group_size;
	uint16_t dag_node_index;
};

struct WLD_MATERIALGROUP
{
	uint16_t group_size;
	uint16_t material_index;
};

struct SDMRGBTrackWLDData
{
	uint32_t numRGBs;
	uint32_t* RGBs;
	bool hasAlphas;
};

struct SDMSpriteDef2WLDData
{
	std::string_view tag;
	float vertexScaleFactor;

	uint32_t numVertices;
	glm::i16vec3* vertices;

	uint32_t numUVs;
	bool uvsUsingOldForm;
	glm::vec2* uvs;
	glm::u16vec2* uvsOldForm;

	uint32_t numVertexNormals;
	glm::u8vec3* vertexNormals;

	uint32_t numRGBs;
	uint32_t* rgbData;

	uint32_t numFaces;
	WLD_DMFACE2* faces;

	uint32_t numSkinGroups;
	WLD_SKINGROUP* skinGroups;

	uint32_t numFaceMaterialGroups;
	WLD_MATERIALGROUP* faceMaterialGroups;

	uint32_t numVertexMaterialGroups;
	WLD_MATERIALGROUP* vertexMaterialGroups;

	std::shared_ptr<MaterialPalette> materialPalette;

	WLD_OBJ_TRACKINSTANCE* trackInstance;
	WLD_OBJ_DMTRACKDEFINITION2* trackDefinition;

	glm::vec3 centerOffset;
	float boundingRadius;
	bool noCollision;
};

struct SDagWLDData
{
	std::string_view tag;

	ActorDefinitionPtr attachedActor;
	std::shared_ptr<STrack> track;
	std::vector<SDagWLDData*> subDags; // TODO: Just store as index
};

struct SHSpriteDefWLDData
{
	std::string_view tag;
	glm::vec3 centerOffset = glm::vec3(0.0f);
	float boundingRadius = 1.0f;

	std::vector<SDagWLDData> dags;
	std::shared_ptr<MaterialPalette> materialPalette;

	uint32_t activeSkin = 0;
	uint32_t numAttachedSkins = 0;
	std::vector<std::unique_ptr<SDMSpriteDef2WLDData>> attachedSkins;
	int* skeletonDagIndices = nullptr;
};

struct SAreaWLDData
{
	uint32_t numRegions;
	uint32_t* regions;
	std::string_view tag;
	std::string_view userData;
	uint32_t areaNum;
};

struct SRegionWLDData
{
	std::string_view tag;
	uint32_t ambientLightIndex = 0;
	uint32_t visibilityType = 0;
	uint32_t range = 0;
	uint8_t* encodedVisibility = nullptr;
	glm::vec3 sphereCenter = glm::vec3{ 0.0f };
	float sphereRadius = 0;
	float reverbVolume = 0;
	uint32_t reverbOffset = 0;
	uint32_t regionSpriteIndex = (uint32_t)-1;
	bool regionSpriteDef = false;
	std::unique_ptr<SDMSpriteDef2WLDData> regionSprite;
};

struct SWorldTreeNodeWLDData
{
	SPlanarEquation plane;
	uint32_t region;
	uint32_t front;
	uint32_t back;
};

struct SWorldTreeWLDData
{
	std::string_view tag;
	std::vector<SWorldTreeNodeWLDData> nodes;
};

struct STerrainWLDData
{
	std::vector<SRegionWLDData> regions;
	std::vector<SAreaWLDData> areas;
	std::shared_ptr<SWorldTreeWLDData> worldTree;
	uint32_t constantAmbientColor;
};

enum EPCloudFlags
{
	PCLOUD_FLAG_FREE                  = 0x00000001,
	PCLOUD_FLAG_COLLISION             = 0x00000002,
	PCLOUD_FLAG_RESPAWN               = 0x00000004,
	PCLOUD_FLAG_VIEWRELX              = 0x00000008,
	PCLOUD_FLAG_VIEWRELY              = 0x00000010,
	PCLOUD_FLAG_VIEWRELZ              = 0x00000020,
	PCLOUD_FLAG_VIEWWARP              = 0x00000040,
	PCLOUD_FLAG_BROWNIAN              = 0x00000080,
	PCLOUD_FLAG_FADE                  = 0x00000100,
	PCLOUD_FLAG_BOUNDINGBOX           = 0x00000200,
	PCLOUD_FLAG_UPDATE_BBOX           = 0x00000400,
	PCLOUD_FLAG_POINTGRAVITY          = 0x00000800,
	PCLOUD_FLAG_GRAVITY               = 0x00001000,
	PCLOUD_FLAG_FREEDEF               = 0x00002000,
	PCLOUD_FLAG_OBJECTRELATIVE        = 0x00004000,
	PCLOUD_FLAG_PARENTOBJRELATIVE     = 0x00008000,
	PCLOUD_FLAG_SPAWNSCALERELATIVE    = 0x00010000,
	PCLOUD_FLAG_HIDEWITHSPAWNOBJECT   = 0x00020000,
};

enum EPCloudSpawnShape
{
	PCLOUD_SPAWN_SHAPE_BOX = 0,
	PCLOUD_SPAWN_SHAPE_SPHERE,
	PCLOUD_SPAWN_SHAPE_RING,
	PCLOUD_SPAWN_SHAPE_SPRAY,
	PCLOUD_SPAWN_SHAPE_DISK,
	PCLOUD_SPAWN_SHAPE_RING2,
};

struct SParticleCloudDefData
{
	uint32_t type;
	uint32_t flags; // EPCloudFlags
	uint32_t size;
	float scalarGravity;
	glm::vec3 gravity;
	glm::vec3 bbMin;
	glm::vec3 bbMax;
	uint32_t spawnType; // EPCloudSpawnShape
	uint32_t spawnTime;
	glm::vec3 spawnMin;
	glm::vec3 spawnMax;
	glm::vec3 spawnNormal;
	float spawnRadius;
	float spawnAngle;
	uint32_t spawnLifespan;
	float spawnVelocity;
	uint32_t spawnRate;
	float spawnScale;
	float dagScale;
	uint32_t color;

	std::shared_ptr<STextureDataDefinition> spriteDef;
};

class ParsedObject
{
public:
	ParsedObject() {}
	virtual ~ParsedObject() {}
};

struct WLDFileObject
{
	uint32_t size;
	WLDObjectType type;
	std::string_view tag;
	uint8_t* data;
	ParsedObject* parsed_data = nullptr;
};

// WLD_OBJ_BMINFO_TYPE (3)
struct ParsedBMInfo : ParsedObject
{
	std::vector<std::shared_ptr<Bitmap>> bitmaps;
};

// WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE (4)
struct ParsedSimpleSpriteDef : ParsedObject
{
	WLD_OBJ_SIMPLESPRITEDEFINITION* definition = nullptr;
	std::vector<std::shared_ptr<ParsedBMInfo>> parsedBitmaps;
	std::shared_ptr<ParsedBMInfo> pParsedBMInfoForPalette;
};

struct ParsedHierarchicalModelDef : ParsedObject
{
	std::shared_ptr<HierarchicalModelDefinition> hierarchicalModelDef;
};

// WLD_OBJ_TRACKDEFINITION_TYPE (18)
struct ParsedTrackInstance : ParsedObject
{
	std::shared_ptr<STrack> track;
};

// WLD_OBJ_TRACKINSTANCE_TYPE (19)
struct ParsedTrackDefinition : ParsedObject
{
	std::shared_ptr<STrack> track;
};

// WLD_OBJ_ACTORDEFINITION_TYPE (20)
struct ParsedActorDefinition : ParsedObject
{
	std::shared_ptr<ActorDefinition> actorDefinition;
};

// WLD_OBJ_ACTORINSTANCE_TYPE (21)
struct ParsedActorInstance : ParsedObject
{
	std::shared_ptr<ActorInstance> actorInstance;
};


// WLD_OBJ_MATERIALDEFINITION_TYPE (48)
struct ParsedMaterialPalette : ParsedObject
{
	WLD_OBJ_MATERIALPALETTE* matPalette = nullptr;
	std::vector<std::shared_ptr<Material>> materials;

	std::shared_ptr<MaterialPalette> palette;
};

struct ParsedPCloudDefinition : ParsedObject
{
	std::shared_ptr<ParticleCloudDefinition> particleCloudDef;
};

// WLD_OBJ_DMSPRITEDEFINITION2_TYPE (54)
struct ParsedDMSpriteDefinition2 : ParsedObject
{
	std::shared_ptr<SimpleModelDefinition> simpleModelDefinition;
};

struct ParsedRegion : ParsedObject
{
	std::shared_ptr<SRegionWLDData> region;
};


} // namespace eqg
