
#include "pch.h"

#include "buffer_reader.h"
#include "log_internal.h"
#include "wld_fragment.h"
#include "wld_loader.h"
#include "wld_structs.h"

namespace eqg {

constexpr float EQ_TO_DEG = 360.0f / 512.0f;

WLDFragment1B::WLDFragment1B(WLDLoader* loader, WLDFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_LIGHTDEFINITION>();

	light = std::make_shared<Light>();
	light->tag = obj->tag;
	light->pos = { 0.0f, 0.0f, 0.0f };

	if (header->flags & WLD_OBJ_LIGHTOPT_SKIPFRAMES)
	{
		light->skip_frames = true;
	}

	if (header->flags & WLD_OBJ_LIGHTOPT_HAVECURRENTFRAME)
	{
		reader.read(light->current_frame);
	}

	if (header->flags & WLD_OBJ_LIGHTOPT_HAVESLEEP)
	{
		reader.read(light->update_interval);
	}

	float* intensity_list = reader.read_array<float>(header->num_frames);
	light->intensity.reserve(header->num_frames);

	for (float* intensity = intensity_list; intensity < intensity_list + header->num_frames; ++intensity)
	{
		light->intensity.push_back(*intensity);
	}

	if (header->flags & WLD_OBJ_LIGHTOPT_HAVECOLORS)
	{
		COLOR* color_list = reader.read_array<COLOR>(header->num_frames);
		light->color.reserve(header->num_frames);

		for (COLOR* color = color_list; color < color_list + header->num_frames; ++color)
		{
			light->color.emplace_back(color->red, color->green, color->blue);
		}
	}
	else
	{
		light->color.emplace_back(1.0f, 1.0f, 1.0f);
	}
}

WLDFragment1C::WLDFragment1C(WLDLoader* loader, WLDFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_LIGHTINSTANCE>();

	light_id = header->definition_id;
}

WLDFragment21::WLDFragment21(WLDLoader* loader, WLDFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_WORLDTREE>();

	tree = std::make_shared<BSPTree>();
	auto& nodes = tree->GetNodes();
	nodes.resize(header->num_world_nodes);

	for (uint32_t i = 0; i < header->num_world_nodes; ++i)
	{
		WLD_OBJ_WORLDTREE_NODE* data = reader.read_ptr<WLD_OBJ_WORLDTREE_NODE>();

		auto& node = nodes[i];
		node.plane = data->plane;
		node.region = data->region;
		node.front = data->node[0];
		node.back = data->node[1];
	}
}

WLDFragment22::WLDFragment22(WLDLoader* loader, WLDFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_REGION>();

	ambient_light_index = loader->GetObjectIndexFromID(header->ambient_light_id);

	if (header->num_vis_nodes > 0)
	{
		reader.skip<WLD_OBJ_XYZ>(header->num_vis_nodes);
		reader.skip<uint32_t>(header->num_vis_nodes * 4);
	}

	if (header->flags & WLD_OBJ_REGOPT_ENCODEDVISIBILITY)
	{
		encoded_visibility_type = 1;
	}
	else if (header->flags & WLD_OBJ_REGOPT_ENCODEDVISIBILITY2)
	{
		encoded_visibility_type = 2;
	}
	else
	{
		encoded_visibility_type = 0;
	}

	range = reader.read<uint16_t>();

	if (encoded_visibility_type != 2)
	{
		reader.skip<uint16_t>(range);
	}
	else
	{
		reader.skip<uint8_t>(range);
	}

	if (header->flags & WLD_OBJ_REGOPT_HAVESPHERE)
	{
		reader.read(sphere_center);
		reader.read(sphere_radius);
	}

	if (header->flags & WLD_OBJ_REGOPT_HAVEREVERBVOLUME)
	{
		reader.skip<uint32_t>();
	}

	if (header->flags & WLD_OBJ_REGOPT_HAVEREVERBOFFSET)
	{
		reader.skip<uint32_t>();
	}

	uint32_t udSize = reader.read<uint32_t>();
	if (udSize > 0)
	{
		reader.skip<uint8_t>(udSize);
	}

	if (header->flags & WLD_OBJ_REGOPT_HAVEREGIONDMSPRITE)
	{
		region_sprite_index = (int)loader->GetObjectIndexFromID(reader.read<uint32_t>());
	}

	if (header->flags & WLD_OBJ_REGOPT_HAVEREGIONDMSPRITEDEF)
	{
		region_sprite_index = (int)loader->GetObjectIndexFromID(reader.read<uint32_t>());
		region_sprite_is_def = true;
	}
}

WLDFragment28::WLDFragment28(WLDLoader* loader, WLDFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_POINTLIGHT>();

	// Get LIGHTINSTANCE
	WLDFileObject& light_obj = loader->GetObjectFromID(header->light_id);
	if (light_obj.type == WLD_OBJ_LIGHTINSTANCE_TYPE)
	{
		WLDFragment1C* light_inst = static_cast<WLDFragment1C*>(light_obj.parsed_data);

		// Get LIGHTDEFINITION
		WLDFileObject& light_def_obj = loader->GetObjectFromID(light_inst->light_id);
		if (light_def_obj.type == WLD_OBJ_LIGHTDEFINITION_TYPE)
		{
			WLDFragment1B* light_def = static_cast<WLDFragment1B*>(light_def_obj.parsed_data);

			light_def->light->pos = { header->pos.x, header->pos.y, header->pos.z };
			light_def->light->radius = header->radius;
		}
	}
}

WLDFragment29::WLDFragment29(WLDLoader* loader, WLDFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_ZONE>();

	region = std::make_shared<BSPRegion>();
	region->tag = obj->tag;

	for (uint32_t i = 0; i < header->num_regions; ++i)
	{
		region->regions.push_back(reader.read<uint32_t>());
	}

	uint32_t str_length = reader.read<uint32_t>();
	if (str_length > 0)
	{
		char* str = reader.read_array<char>(str_length);
		decode_s3d_string(str, str_length);

		region->old_style_tag = str;
	}
}

} // namespace eqg::s3d
