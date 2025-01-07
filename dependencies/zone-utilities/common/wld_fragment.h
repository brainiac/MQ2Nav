
#pragma once

#include "light.h"
#include "placeable.h"
#include "s3d_types.h"
#include "wld_structs.h"

#include <cstdint>
#include <memory>

namespace EQEmu::S3D {

class WLDLoader;

enum S3DObjectType : uint32_t
{
	WLD_NONE                                  = 0,
	WLD_OBJ_DEFAULTPALETTEFILE_TYPE           = 1,
	WLD_OBJ_WORLD_USERDATA_TYPE               = 2,
	WLD_OBJ_BMINFO_TYPE                       = 3,  // (0x03)
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

	WLD_OBJ_POINTLIGHT_TYPE                   = 40, // (0x28)
	WLD_OBJ_ZONE_TYPE                         = 41, // (0x29)

	WLD_OBJ_DMSPRITEDEFINITION_TYPE           = 44, // (0x2C)
	WLD_OBJ_DMSPRITEINSTANCE_TYPE             = 45, // (0x2D)

	WLD_OBJ_MATERIALDEFINITION_TYPE           = 48, // (0x30) TextureBrush
	WLD_OBJ_MATERIALPALETTE_TYPE              = 49, // (0x31) TextureBrushSet

	WLD_OBJ_DMRGBTRACKDEFINITION_TYPE         = 50, // (0x32)
	WLD_OBJ_DMRGBTRACKINSTANCE_TYPE           = 51, // (0x33)
	WLD_OBJ_PCLOUDDEFINITION_TYPE             = 52, // (0x34)
	WLD_OBJ_CONSTANTAMBIENT_TYPE              = 53, // (0x35)
	WLD_OBJ_DMSPRITEDEFINITION2_TYPE          = 54, // (0x36)
};

class WLDFragment;

struct S3DFileObject
{
	uint32_t size;
	S3DObjectType type;
	std::string_view tag;
	uint8_t* data;
	WLDFragment* parsed_data;
};

class WLDFragment
{
public:
	WLDFragment(S3DFileObject* obj_)
		: obj(obj_)
	{
	}

	virtual ~WLDFragment() = default;

	S3DFileObject* obj;
};

// WLD_OBJ_BMINFO_TYPE
class WLDFragment03 : public WLDFragment
{
public:
	WLDFragment03(WLDLoader* loader, S3DFileObject* obj);

	std::shared_ptr<Texture> texture;
};

// WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE
class WLDFragment04 : public WLDFragment
{
public:
	WLDFragment04(WLDLoader* loader, S3DFileObject* obj);

	std::shared_ptr<TextureBrush> brush;
};

// WLD_OBJ_SIMPLESPRITEINSTANCE_TYPE
class WLDFragment05 : public WLDFragment
{
public:
	WLDFragment05(WLDLoader* loader, S3DFileObject* obj);

	uint32_t def_id = 0;
};

// WLD_OBJ_HIERARCHICALSPRITEDEFINITION_TYPE
class WLDFragment10 : public WLDFragment
{
public:
	WLDFragment10(WLDLoader* loader, S3DFileObject* obj);

	std::shared_ptr<SkeletonTrack> track;
};

// WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE
class WLDFragment11 : public WLDFragment
{
public:
	WLDFragment11(WLDLoader* loader, S3DFileObject* obj);

	uint32_t def_id = 0;
};

// WLD_OBJ_TRACKDEFINITION_TYPE
class WLDFragment12 : public WLDFragment
{
public:
	WLDFragment12(WLDLoader* loader, S3DFileObject* obj);

	using TransformList = std::vector<SkeletonTrack::FrameTransform>;
	TransformList transforms;
};

// WLD_OBJ_TRACKINSTANCE_TYPE
class WLDFragment13 : public WLDFragment
{
public:
	WLDFragment13(WLDLoader* loader, S3DFileObject* obj);

	uint32_t def_id = 0;
};

// WLD_OBJ_ACTORDEFINITION_TYPE
class WLDFragment14 : public WLDFragment
{
public:
	WLDFragment14(WLDLoader* loader, S3DFileObject* obj);

	int sprite_id = 0;
};

// WLD_OBJ_ACTORINSTANCE_TYPE
class WLDFragment15 : public WLDFragment
{
public:
	WLDFragment15(WLDLoader* loader, S3DFileObject* obj);

	// TODO: Fixme
	std::shared_ptr<Placeable> placeable;
	int actor_def_id = 0;
	int collision_volume_id = 0;
	int color_track_id = 0;
	float bounding_radius = 0.0f;
	float scale_factor = 1.0f;
};

// WLD_OBJ_LIGHTDEFINITION_TYPE
class WLDFragment1B : public WLDFragment
{
public:
	WLDFragment1B(WLDLoader* loader, S3DFileObject* obj);

	std::shared_ptr<Light> light;
};

// WLD_OBJ_LIGHTINSTANCE_TYPE
class WLDFragment1C : public WLDFragment
{
public:
	WLDFragment1C(WLDLoader* loader, S3DFileObject* obj);

	int light_id = 0;
};

// WLD_OBJ_WORLDTREE_TYPE
class WLDFragment21 : public WLDFragment
{
public:
	WLDFragment21(WLDLoader* loader, S3DFileObject* obj);

	std::shared_ptr<BSPTree> tree;
};

// WLD_OBJ_REGION_TYPE
class WLDFragment22 : public WLDFragment
{
public:
	WLDFragment22(WLDLoader* loader, S3DFileObject* obj);

	int ambient_light_index = 0;
	uint32_t encoded_visibility_type = 0;
	uint32_t range = 0;

	glm::vec3 sphere_center = { 0.0f, 0.0f, 0.0f };
	float sphere_radius = 0.0f;

	int region_sprite_index = -1;
	bool region_sprite_is_def = false;
};

class WLDFragment28 : public WLDFragment
{
public:
	WLDFragment28(WLDLoader* loader, S3DFileObject* obj);

};

// WLD_OBJ_ZONE_TYPE
class WLDFragment29 : public WLDFragment
{
public:
	WLDFragment29(WLDLoader* loader, S3DFileObject* obj);

	std::shared_ptr<BSPRegion> region;
};

// WLD_OBJ_DMSPRITEINSTANCE_TYPE
class WLDFragment2D : public WLDFragment
{
public:
	WLDFragment2D(WLDLoader* loader, S3DFileObject* obj);

	uint32_t sprite_id = 0;
};

// WLD_OBJ_MATERIALDEFINITION_TYPE
class WLDFragment30 : public WLDFragment
{
public:
	WLDFragment30(WLDLoader* loader, S3DFileObject* obj);

	std::shared_ptr<TextureBrush> texture_brush;
};

// WLD_OBJ_MATERIALPALETTE_TYPE
class WLDFragment31 : public WLDFragment
{
public:
	WLDFragment31(WLDLoader* loader, S3DFileObject* obj);

	std::shared_ptr<TextureBrushSet> texture_brush_set;
};

// WLD_OBJ_DMSPRITEDEFINITION2_TYPE
class WLDFragment36 : public WLDFragment
{
public:
	WLDFragment36(WLDLoader* loader, S3DFileObject* obj);
	~WLDFragment36() {}

	std::shared_ptr<Geometry> geometry;
};

} // namespace EQEmu::S3D
