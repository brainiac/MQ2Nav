
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace eqg {

class Bitmap;
class MaterialPalette;

class ActorDefinition;
using ActorDefinitionPtr = std::shared_ptr<ActorDefinition>;

struct STrack;

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

// flags for region objects
enum WLDOBJ_REGOPT
{
	WLD_OBJ_REGOPT_HAVESPHERE                        = 0x0001,
	WLD_OBJ_REGOPT_HAVEREVERBVOLUME                  = 0x0002,
	WLD_OBJ_REGOPT_HAVEREVERBOFFSET                  = 0x0004,
	WLD_OBJ_REGOPT_REGIONFOG                         = 0x0008,
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
	WLD_OBJ_SPROPT_DAGCOLLISIONS                     = 0x20000, // ???
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

struct COLOR
{
	float red;
	float green;
	float blue;
};

struct wld_header
{
	uint32_t magic;
	uint32_t version;
	uint32_t fragments;
	uint32_t unk1;
	uint32_t unk2;
	uint32_t hash_length;
	uint32_t unk3;
};

struct wld_fragment_header
{
	uint32_t size;
	uint32_t id;
	uint32_t name_ref;
};

struct wld_fragment_reference
{
	int32_t id;
};


struct SPlanarEquation
{
	glm::vec3 normal;
	float dist;
};

class BSPRegion
{
public:
	uint32_t flags;
	std::vector<uint32_t> regions;
	std::string tag;
	std::string old_style_tag;
};

class BSPTree // SWorldTreeWLDData
{
public:
	struct BSPNode // SAreaBSPTree
	{
		SPlanarEquation plane;
		uint32_t region;
		uint32_t front;
		uint32_t back;
	};

	std::vector<BSPNode>& GetNodes() { return nodes; }

	std::vector<BSPNode> nodes;
};


struct WLD_OBJ_XYZ
{
	float x;
	float y;
	float z;
};

struct WLD_OBJ_BMINFO // WLD_OBJ_BMINFO_TYPE (0x3)
{
	int tag;
	uint32_t num_mip_levels;
	//uint16_t filepath_length;
};

struct WLD_OBJ_SIMPLESPRITEDEFINITION // WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE (0x4)
{
	int tag;
	uint32_t flags;
	uint32_t num_frames;
};

struct WLD_OBJ_SIMPLESPRITEINSTANCE // WLD_OBJ_SIMPLESPRITEINSTANCE_TYPE (0x5)
{
	int tag;
	int definition_id; // ID of SIMPLESPRITEDEFINITION
	uint32_t flags;
};

struct WLD_OBJ_HIERARCHICALSPRITEDEFINITION
{
	int tag;
	uint32_t flags;
	uint32_t num_dags; // track_ref_count
	uint32_t collision_volume_id; // polygon_anim_frag
};

struct WLD_OBJ_HIERARCHICALSPRITEINSTANCE
{
	int tag;
	uint32_t definition_id;
	uint32_t flags;
};

struct WLDDATA_DAG
{
	int tag; // name_ref;
	uint32_t flags;
	int track_id; // id of track //frag_ref;
	int sprite_id; // id of sprite instance //frag_ref2;
	uint32_t num_sub_dags; //tree_piece_count;
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
	uint32_t nuim_vis_lists;
};

struct WLD_OBJ_POINTLIGHT
{
	int tag;
	int light_id;
	uint32_t flags;
	WLD_OBJ_XYZ pos;
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
	uint32_t render_method; // params1
	uint32_t color; // params2
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

struct BOUNDINGBOX
{
	WLD_OBJ_XYZ min;
	WLD_OBJ_XYZ max;
};

struct WLD_OBJ_DMSPRITEDEFINITION2
{
	int tag;
	uint32_t flags;
	uint32_t material_palette_id; // frag1;
	uint32_t dm_track_id; // frag2;
	uint32_t dmrgb_track_id; // frag3;
	uint32_t collision_volume_id; // frag4;
	WLD_OBJ_XYZ center_offset; // center_x, center_y, center_z;
	WLD_OBJ_XYZ hotspot; // uint32_t params2[3];
	float bounding_radius; // max_dist;
	BOUNDINGBOX bounding_box; // min_x -> max_z
	uint16_t num_vertices; // vertex_count;
	uint16_t num_uvs; // tex_coord_count;
	uint16_t num_vertex_normals; // normal_count;
	uint16_t num_rgb_colors; // color_count;
	uint16_t num_faces; // polygon_count;
	uint16_t num_skin_groups; // size6;
	uint16_t num_fmaterial_groups; // polygon_tex_count;
	uint16_t num_vmaterial_groups; // vertex_tex_count;
	uint16_t num_mesh_ops; // size9;
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


} // namespace eqg
