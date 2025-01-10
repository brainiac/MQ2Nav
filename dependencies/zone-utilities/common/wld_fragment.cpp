
#include "buffer_reader.h"
#include "log_macros.h"
#include "s3d_types.h"
#include "wld_fragment.h"
#include "wld_loader.h"
#include "wld_structs.h"

namespace EQEmu::S3D {

constexpr float EQ_TO_DEG = 360.0f / 512.0f;

// WLD_BITMAP_INFO
WLDFragment03::WLDFragment03(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);

	WLD_OBJ_BMINFO* header = reader.read_ptr<WLD_OBJ_BMINFO>();
	uint32_t count = header->num_mip_levels;
	if (!count)
		count = 1;

	texture = std::make_shared<Texture>();
	texture->frames.reserve(count);

	for (uint32_t i = 0; i < count; ++i)
	{
		uint16_t name_len = reader.read<uint16_t>();

		char* texture_name = reader.read_array<char>(name_len);
		decode_s3d_string(texture_name, name_len);

		texture->frames.emplace_back(texture_name);
	}
}

WLDFragment04::WLDFragment04(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);

	WLD_OBJ_SIMPLESPRITEDEFINITION* def = reader.read_ptr<WLD_OBJ_SIMPLESPRITEDEFINITION>();

	brush = std::make_shared<TextureBrush>();

	if (def->flags & 0x4)
		reader.skip<uint32_t>();

	if (def->flags & 0x8)
		reader.skip<uint32_t>();

	uint32_t count = def->texture_count;
	if (!count)
		count = 1;

	for (uint32_t i = 0; i < count; ++i)
	{
		uint32_t tagId = reader.read<uint32_t>();

		WLDFragment03* bitmap = static_cast<WLDFragment03*>(loader->GetObjectFromID(tagId).parsed_data);
		if (!bitmap)
		{
			eqLogMessage(LogWarn, "missing bitmap!");
			continue;
		}
		brush->brush_textures.push_back(bitmap->texture);
	}
}

WLDFragment05::WLDFragment05(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);

	auto* inst = reader.read_ptr<WLD_OBJ_SIMPLESPRITEINSTANCE>();

	def_id = loader->GetObjectIndexFromID(inst->definition_id);
}

WLDFragment10::WLDFragment10(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);

	auto* header = reader.read_ptr<WLD_OBJ_HIERARCHICALSPRITEDEFINITION>();

	track = std::make_shared<SkeletonTrack>();
	track->name = obj->tag;

	if (header->flags & WLD_OBJ_SPROPT_HAVECENTEROFFSET)
	{
		reader.read(track->center_offset);
	}

	if (header->flags & WLD_OBJ_SPROPT_HAVEBOUNDINGRADIUS)
	{
		reader.read(track->bounding_radius);
	}

	std::vector<std::pair<int, int>> tree;
	std::vector<std::shared_ptr<SkeletonTrack::Bone>> bones;

	for (uint32_t i = 0; i < header->num_dags; ++i)
	{
		WLDDATA_DAG* ent = reader.read_ptr<WLDDATA_DAG>();
		auto bone = std::make_shared<SkeletonTrack::Bone>();

		S3DFileObject& sprite_obj = loader->GetObjectFromID(ent->sprite_id);
		if (sprite_obj.type == WLD_OBJ_DMSPRITEINSTANCE_TYPE)
		{
			WLDFragment2D* sprite_inst = static_cast<WLDFragment2D*>(sprite_obj.parsed_data);
			WLDFragment36* sprite_def = static_cast<WLDFragment36*>(loader->GetObjectFromID(sprite_inst->sprite_id).parsed_data);
			bone->model = sprite_def->geometry;

			S3DFileObject& track_obj = loader->GetObjectFromID(ent->track_id);
			if (track_obj.type == WLD_OBJ_TRACKINSTANCE_TYPE)
			{
				WLDFragment13* track_inst = static_cast<WLDFragment13*>(track_obj.parsed_data);
				WLDFragment12* track_def = static_cast<WLDFragment12*>(loader->GetObjectFromID(track_inst->def_id).parsed_data);
				bone->transforms = track_def->transforms; // FIXME
			}
		}

		bones.push_back(bone);

		for (uint32_t j = 0; j < ent->num_sub_dags; ++j)
		{
			tree.emplace_back(i, reader.read<int32_t>());
		}
	}

	for (auto& [bone_src, bone_dst] : tree)
	{
		bones[bone_src]->children.push_back(bones[bone_dst]);
	}

	if (header->flags & WLD_OBJ_SPROPT_HAVEATTACHEDSKINS)
	{
		uint32_t numAttachedSkins = reader.read<uint32_t>();

		for (uint32_t i = 0; i < numAttachedSkins; ++i)
		{
			int ref = reader.read<int>();

			S3DFileObject& skinObj = loader->GetObjectFromID(ref);
			if (skinObj.type = WLD_OBJ_DMSPRITEINSTANCE_TYPE)
			{
				WLDFragment2D* inst = static_cast<WLDFragment2D*>(skinObj.parsed_data);

				S3DFileObject& defObj = loader->GetObjectFromID(inst->sprite_id);
				if (defObj.type == WLD_OBJ_DMSPRITEDEFINITION2_TYPE)
				{
					WLDFragment36* f = static_cast<WLDFragment36*>(defObj.parsed_data);
					auto mod = f->geometry;
				}
			}
		}

		track->attached_skeleton_dag_index.resize(numAttachedSkins);
		for (uint32_t i = 0; i < numAttachedSkins; ++i)
		{
			track->attached_skeleton_dag_index[i] = reader.read<int>();
		}
	}
}

WLDFragment11::WLDFragment11(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_HIERARCHICALSPRITEINSTANCE>();

	def_id = header->definition_id;
}

WLDFragment12::WLDFragment12(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_TRACKDEFINITION>();

	transforms.resize(header->num_frames);

	for (uint32_t i = 0; i < header->num_frames; ++i)
	{
		auto* s3d_transform = reader.read_ptr<EQG_S3D_PFRAMETRANSFORM>();
		auto& transform = transforms[i];

		constexpr float quat_scale = 1.0f / 16384.0f;
		constexpr float pivot_scale = 1.0f / 256.0f;

		transform.rotation = glm::quat(
			-static_cast<float>(s3d_transform->rot_q3 * quat_scale),
			static_cast<float>(s3d_transform->rot_q0 * quat_scale),
			static_cast<float>(s3d_transform->rot_q1 * quat_scale),
			static_cast<float>(s3d_transform->rot_q2 * quat_scale)
		);
		transform.pivot = glm::vec3(
			static_cast<float>(s3d_transform->pivot_x * pivot_scale),
			static_cast<float>(s3d_transform->pivot_y * pivot_scale),
			static_cast<float>(s3d_transform->pivot_z * pivot_scale)
		);
		transform.scale = static_cast<float>(s3d_transform->scale) * pivot_scale;
	}
}

WLDFragment13::WLDFragment13(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_TRACKINSTANCE>();

	def_id = header->track_id;
}

WLDFragment14::WLDFragment14(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_ACTORDEFINITION>();

	if (header->flags & WLD_OBJ_ACTOROPT_HAVECURRENTACTION)
		reader.skip<uint32_t>();
	if (header->flags & WLD_OBJ_ACTOROPT_HAVELOCATION)
		reader.skip(sizeof(uint32_t) * 7);

	std::string_view callback_name = loader->GetString(header->callback_id);

	// action data is unused.
	for (uint32_t i = 0; i < header->num_actions; ++i)
	{
		uint32_t size = reader.read<uint32_t>();

		for (uint32_t j = 0; j < size; ++j)
		{
			reader.skip<uint32_t>();
			reader.skip<float>();
		}
	}

	// Should only have one sprite id;
	for (uint32_t i = 0; i < header->num_sprites; ++i)
	{
		sprite_id = reader.read<uint32_t>();
	}

	// remaining data is "userdata" but is never set, so don't bother to read for it.
}

WLDFragment15::WLDFragment15(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_ACTORINSTANCE>();

	if (header->actor_def_id >= 0)
		return;

	if (header->flags & WLD_OBJ_ACTOROPT_HAVECURRENTACTION)
		reader.skip<uint32_t>();

	placeable = std::make_shared<Placeable>();
	placeable->tag = loader->GetString(header->actor_def_id);
	actor_def_id = header->actor_def_id;
	collision_volume_id = header->collision_volume_id;

	if (header->flags & WLD_OBJ_ACTOROPT_HAVELOCATION)
	{
		SLocation location;
		reader.read(location);

		placeable->pos.x = location.x;
		placeable->pos.y = location.y;
		placeable->pos.z = location.z;

		// y rotation seems backwards on s3ds, probably cause of the weird coord system they used back then
		// x rotation might be too but there are literally 0 x rotated placeables in all the s3ds so who knows
		// 
		// Convert 512 degrees -> 360 degrees -> radians
		placeable->rotate.x = glm::radians(location.roll * EQ_TO_DEG);
		placeable->rotate.y = -glm::radians(location.pitch * EQ_TO_DEG);
		placeable->rotate.z = glm::radians(location.heading * EQ_TO_DEG);
	}

	if (header->flags & WLD_OBJ_ACTOROPT_HAVEBOUNDINGRADIUS)
	{
		reader.read(bounding_radius);
	}

	if (header->flags & WLD_OBJ_ACTOROPT_HAVESCALEFACTOR)
	{
		reader.read(scale_factor);
	}

	placeable->scale = glm::vec3(scale_factor);

	if (header->flags & WLD_OBJ_ACTOROPT_HAVEDMRGBTRACK)
	{
		reader.read(color_track_id);
	}
}

WLDFragment1B::WLDFragment1B(WLDLoader* loader, S3DFileObject* obj)
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

WLDFragment1C::WLDFragment1C(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_LIGHTINSTANCE>();

	light_id = header->definition_id;
}

WLDFragment21::WLDFragment21(WLDLoader* loader, S3DFileObject* obj)
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

WLDFragment22::WLDFragment22(WLDLoader* loader, S3DFileObject* obj)
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

WLDFragment28::WLDFragment28(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_POINTLIGHT>();

	// Get LIGHTINSTANCE
	S3DFileObject& light_obj = loader->GetObjectFromID(header->light_id);
	if (light_obj.type == WLD_OBJ_LIGHTINSTANCE_TYPE)
	{
		WLDFragment1C* light_inst = static_cast<WLDFragment1C*>(light_obj.parsed_data);

		// Get LIGHTDEFINITION
		S3DFileObject& light_def_obj = loader->GetObjectFromID(light_inst->light_id);
		if (light_def_obj.type == WLD_OBJ_LIGHTDEFINITION_TYPE)
		{
			WLDFragment1B* light_def = static_cast<WLDFragment1B*>(light_def_obj.parsed_data);

			light_def->light->pos = { header->pos.x, header->pos.y, header->pos.z };
			light_def->light->radius = header->radius;
		}
	}
}

WLDFragment29::WLDFragment29(WLDLoader* loader, S3DFileObject* obj)
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

WLDFragment2D::WLDFragment2D(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_DMSPRITEINSTANCE>();

	sprite_id = header->definition_id;
}

// WLD_OBJ_MATERIALDEFINITION_TYPE
WLDFragment30::WLDFragment30(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_MATERIALDEFINITION>();

	if (!header->flags)
	{
		reader.skip(sizeof(uint32_t) * 2); // ???
	}

	if (!header->render_method || !header->sprite_or_bminfo)
	{
		texture_brush = std::make_shared<TextureBrush>();
		auto texture = std::make_shared<Texture>();
		texture->GetTextureFrames().push_back("collide.dds");
		
		texture_brush->brush_textures.push_back(texture);
		texture_brush->flags = 1;
		return;
	}

	S3DFileObject& sprite_obj = loader->GetObjectFromID(header->sprite_or_bminfo);
	uint32_t tex_ref = 0;
	if (sprite_obj.type == WLD_OBJ_SIMPLESPRITEINSTANCE_TYPE)
	{
		WLDFragment05* simple_sprite_inst = static_cast<WLDFragment05*>(sprite_obj.parsed_data);

		tex_ref = simple_sprite_inst->def_id;
	}

	S3DFileObject& sprite_def_obj = loader->GetObjectFromID(tex_ref);
	if (sprite_def_obj.type == WLD_OBJ_SIMPLESPRITEDEFINITION_TYPE)
	{
		WLDFragment04* simple_sprite_def = static_cast<WLDFragment04*>(sprite_def_obj.parsed_data);

		texture_brush = std::make_shared<TextureBrush>();
		*texture_brush = *simple_sprite_def->brush;
		 
		// ????

		if (header->render_method & (1 << 1) || header->render_method & (1 << 2) || header->render_method & (1 << 3) || header->render_method & (1 << 4))
			texture_brush->flags = 1;
		else
			texture_brush->flags = 0;
	}
}

// WLD_OBJ_MATERIALPALETTE_TYPE
WLDFragment31::WLDFragment31(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_MATERIALPALETTE>();

	texture_brush_set = std::make_shared<TextureBrushSet>();
	texture_brush_set->texture_sets.reserve(header->num_entries);

	for (uint32_t i = 0; i < header->num_entries; ++i)
	{
		int ref_id = reader.read<int>();

		S3DFileObject& frag = loader->GetObjectFromID(ref_id);
		if (frag.type == WLD_OBJ_MATERIALDEFINITION_TYPE)
		{
			WLDFragment30* parsed_data = static_cast<WLDFragment30*>(frag.parsed_data);

			texture_brush_set->texture_sets.push_back(parsed_data->texture_brush);
		}
	}
}

WLDFragment36::WLDFragment36(WLDLoader* loader, S3DFileObject* obj)
	: WLDFragment(obj)
{
	BufferReader reader(obj->data, obj->size);
	auto* header = reader.read_ptr<WLD_OBJ_DMSPRITEDEFINITION2>();

	constexpr float recip_255 = 1.0f / 256.0f;
	constexpr float recip_127 = 1.0f / 127.0f;

	float scale = 1.0f / (float)(1 << header->scale);

	std::shared_ptr<Geometry> model = std::make_shared<Geometry>();
	model->tag = obj->tag;

	S3DFileObject& frag = loader->GetObjectFromID(header->material_palette_id);
	if (frag.type == WLD_OBJ_MATERIALPALETTE_TYPE)
	{
		WLDFragment31* material_palette = static_cast<WLDFragment31*>(frag.parsed_data);

		model->SetTextureBrushSet(material_palette->texture_brush_set);
	}

	// Vertices
	model->verts.resize(header->num_vertices);
	WLD_PVERTEX* verts = reader.read_array<WLD_PVERTEX>(header->num_vertices);

	for (uint32_t i = 0; i < header->num_vertices; ++i)
	{
		auto& in = verts[i];

		model->verts[i].pos = {
			header->center_offset.x + in.x * scale,
			header->center_offset.y + in.y * scale,
			header->center_offset.z + in.z * scale
		};
	}

	// Texture coordinates
	uint16_t num_uvs = header->num_uvs;
	if (num_uvs > model->verts.size())
	{
		eqLogMessage(LogWarn, "num_uvs > model->verts.size()");
		num_uvs = (uint16_t)model->verts.size();
	}

	if (loader->IsOldVersion())
	{
		WLD_PUVOLDFORM* uvs = reader.read_array<WLD_PUVOLDFORM>(header->num_uvs);

		for (uint32_t i = 0; i < num_uvs; ++i)
		{
			auto& in = uvs[i];
			model->verts[i].tex = { in.u * recip_255, in.v * recip_255 };
		}
	}
	else
	{
		WLD_PUV* uvs = reader.read_array<WLD_PUV>(header->num_uvs);

		for (uint32_t i = 0; i < num_uvs; ++i)
		{
			auto& in = uvs[i];
			model->verts[i].tex = { in.u, in.v };
		}
	}

	// Normals
	uint16_t num_vertex_normals = header->num_vertex_normals;
	if (num_vertex_normals > model->verts.size())
	{
		// this check is here cause there's literally zones where there's normals than verts (ssratemple for ex). Stupid I know.
		eqLogMessage(LogWarn, "num_vertex_normals > model->verts.size()");
		num_vertex_normals = (uint16_t)model->verts.size();
	}

	WLD_PNORMAL* normals = reader.read_array<WLD_PNORMAL>(header->num_vertex_normals);
	for (uint32_t i = 0; i < num_vertex_normals; ++i)
	{
		auto& in = normals[i];
		model->verts[i].nor = { in.x * recip_127, in.y * recip_127, in.z * recip_127 };
	}

	// TODO: not handling colors yet
	reader.read_array<uint32_t>(header->num_rgb_colors);

	// Polygons
	model->polys.resize(header->num_faces);
	WLD_DMFACE2* faces = reader.read_array<WLD_DMFACE2>(header->num_faces);
	for (uint32_t i = 0; i < header->num_faces; ++i)
	{
		auto& in = faces[i];
		auto& p = model->polys[i];

		p.flags = in.flags;
		p.verts[0] = in.indices[2];
		p.verts[1] = in.indices[1];
		p.verts[2] = in.indices[0];
	}

	// TODO: not handling skin group
	reader.read_array<WLD_SKINGROUP>(header->num_skin_groups);

	// Materials
	WLD_MATERIALGROUP* poly_material_groups = reader.read_array<WLD_MATERIALGROUP>(header->num_fmaterial_groups);
	uint32_t pc = 0;

	for (uint32_t i = 0; i < header->num_fmaterial_groups; ++i)
	{
		auto& tm = poly_material_groups[i];

		for (uint32_t group_num = 0; group_num < tm.group_size; ++group_num)
		{
			if (pc >= model->polys.size())
			{
				eqLogMessage(LogWarn, "pc >= model->GetPolygons().size()");
				break;
			}

			model->polys[pc++].tex = tm.material_id;
		}
	}

	// TODO: not handling vertex material groups

	geometry = model;
}

} // namespace EQEmu::S3D
