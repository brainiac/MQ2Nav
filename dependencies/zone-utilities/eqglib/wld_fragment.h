
#pragma once

#include "eqg_material.h"
#include "light.h"
#include "wld_structs.h"

#include <cstdint>
#include <memory>

namespace eqg {

struct STrack;
class WLDLoader;
class HierarchicalModelDefinition;

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

	WLD_OBJ_WORLDTREE_TYPE                    = 33, // (0x21) BSPTree
	WLD_OBJ_REGION_TYPE                       = 34, // (0x22)

	WLD_OBJ_BLITSPRITEDEFINITION_TYPE         = 38,
	WLD_OBJ_BLITSPRITEINSTANCE_TYPE           = 39,
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

// WLD_OBJ_DMSPRITEDEFINITION2_TYPE (54)
struct ParsedDMSpriteDefinition2 : ParsedObject
{
	std::shared_ptr<SimpleModelDefinition> simpleModelDefinition;
};

class WLDFragment : public ParsedObject
{
public:
	WLDFragment(WLDFileObject* obj_)
		: obj(obj_)
	{
	}

	virtual ~WLDFragment() = default;

	WLDFileObject* obj;
};

// WLD_OBJ_LIGHTDEFINITION_TYPE
class WLDFragment1B : public WLDFragment
{
public:
	WLDFragment1B(WLDLoader* loader, WLDFileObject* obj);

	std::shared_ptr<Light> light;
};

// WLD_OBJ_LIGHTINSTANCE_TYPE
class WLDFragment1C : public WLDFragment
{
public:
	WLDFragment1C(WLDLoader* loader, WLDFileObject* obj);

	int light_id = 0;
};

// WLD_OBJ_WORLDTREE_TYPE
class WLDFragment21 : public WLDFragment
{
public:
	WLDFragment21(WLDLoader* loader, WLDFileObject* obj);

	std::shared_ptr<BSPTree> tree;
};

// WLD_OBJ_REGION_TYPE
class WLDFragment22 : public WLDFragment
{
public:
	WLDFragment22(WLDLoader* loader, WLDFileObject* obj);

	int ambient_light_index = 0;
	uint32_t encoded_visibility_type = 0;
	uint32_t range = 0;

	glm::vec3 sphere_center = { 0.0f, 0.0f, 0.0f };
	float sphere_radius = 0.0f;

	int region_sprite_index = -1;
	bool region_sprite_is_def = false;
};

// WLD_OBJ_POINTLIGHT_TYPE
class WLDFragment28 : public WLDFragment
{
public:
	WLDFragment28(WLDLoader* loader, WLDFileObject* obj);
};

// WLD_OBJ_ZONE_TYPE
class WLDFragment29 : public WLDFragment
{
public:
	WLDFragment29(WLDLoader* loader, WLDFileObject* obj);

	std::shared_ptr<BSPRegion> region;
};

} // namespace eqg
