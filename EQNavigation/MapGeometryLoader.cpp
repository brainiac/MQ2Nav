
#include "pch.h"
#include "MapGeometryLoader.h"

#include "zone-utilities/log/log_macros.h"
#include "zone-utilities/common/compression.h"

#include <gtc/matrix_transform.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


MapGeometryLoader::MapGeometryLoader(const std::string& zoneShortName,
	const std::string& everquest_path)
	: m_zoneName(zoneShortName)
	, m_eqPath(everquest_path)
{
}

MapGeometryLoader::~MapGeometryLoader()
{
	delete [] m_verts;
	delete [] m_normals;
	delete [] m_tris;
}
		
void MapGeometryLoader::addVertex(float x, float y, float z)
{
	if (m_vertCount+1 > vcap)
	{
		vcap = !vcap ? 8 : vcap * 2;
		float* nv = new float[vcap * 3];
		if (m_vertCount)
			memcpy(nv, m_verts, m_vertCount*3*sizeof(float));
		delete [] m_verts;
		m_verts = nv;
	}
	float* dst = &m_verts[m_vertCount*3];
	*dst++ = x*m_scale;
	*dst++ = y*m_scale;
	*dst++ = z*m_scale;
	m_vertCount++;
}

void MapGeometryLoader::addTriangle(int a, int b, int c)
{
	if (m_triCount + 1 > tcap)
	{
		tcap = !tcap ? 8 : tcap * 2;
		int* nv = new int[tcap * 3];
		if (m_triCount)
			memcpy(nv, m_tris, m_triCount*3*sizeof(int));
		delete [] m_tris;
		m_tris = nv;
	}
	int* dst = &m_tris[m_triCount*3];
	*dst++ = a;
	*dst++ = b;
	*dst++ = c;
	m_triCount++;
}

template <typename T>
void MapGeometryLoader::AddMapModel(const std::shared_ptr<EQEmu::Placeable>& obj,
	T& model, uint32_t& counter)
{
	const auto& verts = model.GetVertices();
	const auto& polys = model.GetPolygons();

	for (const auto& poly : polys)
	{
		// 0x10 appeared on some non-collidable polygons
		if (poly.flags == 0x10)
			continue;

		for (int i = 0; i < 3; i++)
		{
			glm::vec3 vert = verts[poly.verts[i]].pos;
			RotateVertex(vert, obj->GetRotateX(), obj->GetRotateY(), obj->GetRotateZ());
			ScaleVertex(vert, obj->GetScaleX(), obj->GetScaleY(), obj->GetScaleZ());
			TranslateVertex(vert, obj->GetX(), obj->GetY(), obj->GetZ());

			addVertex(vert.y, vert.z, vert.x);
		}

		addTriangle(counter, counter + 1, counter + 2);
		counter += 3;
	}
}

bool MapGeometryLoader::load()
{
	uint32_t counter = 0;

	if (!Build())
	{
		return false;
	}
	else
	{
		// load terrain geometry
		if (terrain)
		{
			const auto& tiles = terrain->GetTiles();
			uint32_t quads_per_tile = terrain->GetQuadsPerTile();
			float units_per_vertex = terrain->GetUnitsPerVertex();
			uint32_t vert_count = ((quads_per_tile + 1) * (quads_per_tile + 1));
			uint32_t quad_count = (quads_per_tile * quads_per_tile);

			for (uint32_t i = 0; i < tiles.size(); ++i)
			{
				auto& tile = tiles[i];
				bool flat = tile->IsFlat();

				float x = tile->GetX();
				float y = tile->GetY();


				if (flat)
				{
					float z = tile->GetFloats()[0];

					// get x,y of corner point for this quad
					float dt = quads_per_tile * units_per_vertex;

					addVertex(x,      z, y);
					addVertex(x + dt, z, y);
					addVertex(x + dt, z, y + dt);
					addVertex(x,      z, y + dt);

					addTriangle(counter + 0, counter + 2, counter + 1);
					addTriangle(counter + 2, counter + 0, counter + 3);

					counter += 4;
				}
				else
				{
					auto& floats = tile->GetFloats();
					int row_number = -1;

					for (uint32_t quad = 0; quad < quad_count; ++quad)
					{
						if (quad % quads_per_tile == 0)
							++row_number;

						if (tile->GetFlags()[quad] & 0x01)
							continue;

						// get x,y of corner point for this quad
						float _x = x + (row_number * units_per_vertex);
						float _y = y + (quad % quads_per_tile) * units_per_vertex;
						float dt = units_per_vertex;

						float z1 = floats[quad + row_number];
						float z2 = floats[quad + row_number + quads_per_tile + 1];
						float z3 = floats[quad + row_number + quads_per_tile + 2];
						float z4 = floats[quad + row_number + 1];

						addVertex(_x,      z1, _y);
						addVertex(_x + dt, z2, _y);
						addVertex(_x + dt, z3, _y + dt);
						addVertex(_x,      z4, _y + dt);

						addTriangle(counter + 0, counter + 2, counter + 1);
						addTriangle(counter + 2, counter + 0, counter + 3);

						counter += 4;
					}
				}
			}
		}
		for (uint32_t index = 0; index < collide_indices.size(); index += 3, counter += 3)
		{
			uint32_t vert_index1 = collide_indices[index];
			const glm::vec3& vert1 = collide_verts[vert_index1];
			addVertex(vert1.x, vert1.z, vert1.y);

			uint32_t vert_index2 = collide_indices[index + 2];
			const glm::vec3& vert2 = collide_verts[vert_index2];
			addVertex(vert2.x, vert2.z, vert2.y);

			uint32_t vert_index3 = collide_indices[index + 1];
			const glm::vec3& vert3 = collide_verts[vert_index3];
			addVertex(vert3.x, vert3.z, vert3.y);

			addTriangle(counter, counter + 2, counter + 1);
		}

		for (auto& obj : map_placeables)
		{
			const std::string& name = obj->GetFileName();

			// check map models
			auto modelIter = map_models.find(name);
			if (modelIter != map_models.end())
			{
				const auto& model = modelIter->second;
				AddMapModel<EQEmu::S3D::Geometry>(obj, *model, counter);

				continue;
			}

			auto modelIter2 = map_eqg_models.find(name);
			if (modelIter2 != map_eqg_models.end())
			{
				const auto& model = modelIter2->second;
				AddMapModel<EQEmu::EQG::Geometry>(obj, *model, counter);

				continue;
			}
		}

		//const auto& non_collide_indices = map.GetNonCollideIndices();

		//for (uint32_t index = 0; index < non_collide_indices.size(); index += 3, counter += 3)
		//{
		//	uint32_t vert_index1 = non_collide_indices[index];
		//	const glm::vec3& vert1 = map.GetNonCollideVert(vert_index1);
		//	addVertex(vert1.x, vert1.z, vert1.y, vcap);

		//	uint32_t vert_index2 = non_collide_indices[index + 1];
		//	const glm::vec3& vert2 = map.GetNonCollideVert(vert_index2);
		//	addVertex(vert2.x, vert2.z, vert2.y, vcap);

		//	uint32_t vert_index3 = non_collide_indices[index + 2];
		//	const glm::vec3& vert3 = map.GetNonCollideVert(vert_index3);
		//	addVertex(vert3.x, vert3.z, vert3.y, vcap);

		//	addTriangle(counter, counter + 2, counter + 1, tcap);
		//}
	}
	
	//message = "Calculating Surface Normals...";
	m_normals = new float[m_triCount*3];
	for (int i = 0; i < m_triCount*3; i += 3)
	{
		const float* v0 = &m_verts[m_tris[i]*3];
		const float* v1 = &m_verts[m_tris[i+1]*3];
		const float* v2 = &m_verts[m_tris[i+2]*3];
		float e0[3], e1[3];
		for (int j = 0; j < 3; ++j)
		{
			e0[j] = v1[j] - v0[j];
			e1[j] = v2[j] - v0[j];
		}
		float* n = &m_normals[i];
		n[0] = e0[1]*e1[2] - e0[2]*e1[1];
		n[1] = e0[2]*e1[0] - e0[0]*e1[2];
		n[2] = e0[0]*e1[1] - e0[1]*e1[0];
		float d = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
		if (d > 0)
		{
			d = 1.0f/d;
			n[0] *= d;
			n[1] *= d;
			n[2] *= d;
		}
	}

	return true;
}

//============================================================================

bool MapGeometryLoader::Build()
{
	eqLogMessage(LogTrace, "Attempting to load %s.eqg as a standard eqg.", m_zoneName.c_str());

	std::string filePath = m_eqPath + "\\" + m_zoneName;

	EQEmu::EQGLoader eqg;
	std::vector<std::shared_ptr<EQEmu::EQG::Geometry>> eqg_models;
	std::vector<std::shared_ptr<EQEmu::Placeable>> eqg_placables;
	std::vector<std::shared_ptr<EQEmu::EQG::Region>> eqg_regions;
	std::vector<std::shared_ptr<EQEmu::Light>> eqg_lights;
	if (eqg.Load(filePath, eqg_models, eqg_placables, eqg_regions, eqg_lights))
	{
		return CompileEQG(eqg_models, eqg_placables, eqg_regions, eqg_lights);
	}

	eqLogMessage(LogTrace, "Attempting to load %s.eqg as a v4 eqg.", m_zoneName.c_str());
	EQEmu::EQG4Loader eqg4;
	if (eqg4.Load(filePath, terrain))
	{
		return CompileEQGv4();
	}

	eqLogMessage(LogTrace, "Attempting to load %s.s3d as a standard s3d.", m_zoneName.c_str());
	EQEmu::S3DLoader s3d;
	std::vector<EQEmu::S3D::WLDFragment> zone_frags;
	std::vector<EQEmu::S3D::WLDFragment> zone_object_frags;
	std::vector<EQEmu::S3D::WLDFragment> object_frags;
	if (!s3d.ParseWLDFile(filePath + ".s3d", m_zoneName + ".wld", zone_frags))
	{
		return false;
	}

	if (!s3d.ParseWLDFile(filePath + ".s3d", "objects.wld", zone_object_frags))
	{
		return false;
	}

	if (!s3d.ParseWLDFile(filePath + "_obj.s3d", m_zoneName + "_obj.wld", object_frags))
	{
		return false;
	}

	return CompileS3D(zone_frags, zone_object_frags, object_frags);
}

void MapGeometryLoader::TraverseBone(
	std::shared_ptr<EQEmu::S3D::SkeletonTrack::Bone> bone,
	glm::vec3 parent_trans,
	glm::vec3 parent_rot,
	glm::vec3 parent_scale)
{
	float offset_x = 0.0f;
	float offset_y = 0.0f;
	float offset_z = 0.0f;

	float rot_x = 0.0f;
	float rot_y = 0.0f;
	float rot_z = 0.0f;

	float scale_x = parent_scale.x;
	float scale_y = parent_scale.y;
	float scale_z = parent_scale.z;

	if (bone->orientation)
	{
		if (bone->orientation->shift_denom != 0)
		{
			offset_x = (float)bone->orientation->shift_x_num / (float)bone->orientation->shift_denom;
			offset_y = (float)bone->orientation->shift_y_num / (float)bone->orientation->shift_denom;
			offset_z = (float)bone->orientation->shift_z_num / (float)bone->orientation->shift_denom;
		}

		if (bone->orientation->rotate_denom != 0)
		{
			rot_x = (float)bone->orientation->rotate_x_num / (float)bone->orientation->rotate_denom;
			rot_y = (float)bone->orientation->rotate_y_num / (float)bone->orientation->rotate_denom;
			rot_z = (float)bone->orientation->rotate_z_num / (float)bone->orientation->rotate_denom;

			rot_x = rot_x * 3.14159f / 180.0f;
			rot_y = rot_y * 3.14159f / 180.0f;
			rot_z = rot_z * 3.14159f / 180.0f;
		}
	}

	glm::vec3 pos(offset_x, offset_y, offset_z);
	glm::vec3 rot(rot_x, rot_y, rot_z);

	RotateVertex(pos, parent_rot.x, parent_rot.y, parent_rot.z);
	pos += parent_trans;

	rot += parent_rot;

	if (bone->model)
	{
		auto& mod_polys = bone->model->GetPolygons();
		auto& mod_verts = bone->model->GetVertices();

		if (map_models.count(bone->model->GetName()) == 0)
		{
			map_models[bone->model->GetName()] = bone->model;
		}

		std::shared_ptr<EQEmu::Placeable> gen_plac(new EQEmu::Placeable());
		gen_plac->SetFileName(bone->model->GetName());
		gen_plac->SetLocation(pos.x, pos.y, pos.z);
		gen_plac->SetRotation(rot.x, rot.y, rot.z);
		gen_plac->SetScale(scale_x, scale_y, scale_z);
		map_placeables.push_back(gen_plac);

		eqLogMessage(LogTrace, "Adding placeable %s at (%f, %f, %f)", bone->model->GetName().c_str(), pos.x, pos.y, pos.z);
	}

	for (size_t i = 0; i < bone->children.size(); ++i)
	{
		TraverseBone(bone->children[i], pos, rot, parent_scale);
	}
}

bool MapGeometryLoader::CompileS3D(
	std::vector<EQEmu::S3D::WLDFragment>& zone_frags,
	std::vector<EQEmu::S3D::WLDFragment>& zone_object_frags,
	std::vector<EQEmu::S3D::WLDFragment>& object_frags)
{
	collide_verts.clear();
	collide_indices.clear();
	non_collide_verts.clear();
	non_collide_indices.clear();
	current_collide_index = 0;
	current_non_collide_index = 0;
	collide_vert_to_index.clear();
	non_collide_vert_to_index.clear();
	map_models.clear();
	map_eqg_models.clear();
	map_placeables.clear();

	eqLogMessage(LogTrace, "Processing s3d zone geometry fragments.");
	for (uint32_t i = 0; i < zone_frags.size(); ++i)
	{
		if (zone_frags[i].type == 0x36)
		{
			EQEmu::S3D::WLDFragment36& frag = reinterpret_cast<EQEmu::S3D::WLDFragment36&>(zone_frags[i]);
			auto model = frag.GetData();

			auto& mod_polys = model->GetPolygons();
			auto& mod_verts = model->GetVertices();

			for (uint32_t j = 0; j < mod_polys.size(); ++j)
			{
				auto& current_poly = mod_polys[j];
				auto v1 = mod_verts[current_poly.verts[0]];
				auto v2 = mod_verts[current_poly.verts[1]];
				auto v3 = mod_verts[current_poly.verts[2]];

				float t = v1.pos.x;
				v1.pos.x = v1.pos.y;
				v1.pos.y = t;

				t = v2.pos.x;
				v2.pos.x = v2.pos.y;
				v2.pos.y = t;

				t = v3.pos.x;
				v3.pos.x = v3.pos.y;
				v3.pos.y = t;

				if (current_poly.flags == 0x10)
					AddFace(v1.pos, v2.pos, v3.pos, false);
				else
					AddFace(v1.pos, v2.pos, v3.pos, true);
			}
		}
	}

	eqLogMessage(LogTrace, "Processing zone placeable fragments.");
	std::vector<std::pair<std::shared_ptr<EQEmu::Placeable>, std::shared_ptr<EQEmu::S3D::Geometry>>> placables;
	std::vector<std::pair<std::shared_ptr<EQEmu::Placeable>, std::shared_ptr<EQEmu::S3D::SkeletonTrack>>> placables_skeleton;
	for (uint32_t i = 0; i < zone_object_frags.size(); ++i)
	{
		if (zone_object_frags[i].type == 0x15)
		{
			EQEmu::S3D::WLDFragment15& frag = reinterpret_cast<EQEmu::S3D::WLDFragment15&>(zone_object_frags[i]);
			auto plac = frag.GetData();

			if (!plac)
			{
				eqLogMessage(LogWarn, "Placeable entry was not found.");
				continue;
			}

			bool found = false;
			for (uint32_t o = 0; o < object_frags.size(); ++o)
			{
				if (object_frags[o].type == 0x14) {
					EQEmu::S3D::WLDFragment14& obj_frag = reinterpret_cast<EQEmu::S3D::WLDFragment14&>(object_frags[o]);
					auto mod_ref = obj_frag.GetData();

					if (mod_ref->GetName().compare(plac->GetName()) == 0)
					{
						found = true;

						auto& frag_refs = mod_ref->GetFrags();
						for (uint32_t m = 0; m < frag_refs.size(); ++m)
						{
							if (object_frags[frag_refs[m] - 1].type == 0x2D)
							{
								EQEmu::S3D::WLDFragment2D& r_frag = reinterpret_cast<EQEmu::S3D::WLDFragment2D&>(object_frags[frag_refs[m] - 1]);
								auto m_ref = r_frag.GetData();

								EQEmu::S3D::WLDFragment36& mod_frag = reinterpret_cast<EQEmu::S3D::WLDFragment36&>(object_frags[m_ref]);
								auto mod = mod_frag.GetData();
								placables.push_back(std::make_pair(plac, mod));
							}
							else if (object_frags[frag_refs[m] - 1].type == 0x11)
							{
								EQEmu::S3D::WLDFragment11& r_frag = reinterpret_cast<EQEmu::S3D::WLDFragment11&>(object_frags[frag_refs[m] - 1]);
								auto s_ref = r_frag.GetData();

								EQEmu::S3D::WLDFragment10& skeleton_frag = reinterpret_cast<EQEmu::S3D::WLDFragment10&>(object_frags[s_ref]);
								auto skele = skeleton_frag.GetData();

								placables_skeleton.push_back(std::make_pair(plac, skele));
							}
						}

						break;
					}
				}
			}

			if (!found)
			{
				eqLogMessage(LogWarn, "Could not find model for placeable %s", plac->GetName().c_str());
			}
		}
	}

	eqLogMessage(LogTrace, "Processing s3d placeables.");
	size_t pl_sz = placables.size();
	for (size_t i = 0; i < pl_sz; ++i)
	{
		auto plac = placables[i].first;
		auto model = placables[i].second;

		auto& mod_polys = model->GetPolygons();
		auto& mod_verts = model->GetVertices();

		float offset_x = plac->GetX();
		float offset_y = plac->GetY();
		float offset_z = plac->GetZ();

		float rot_x = plac->GetRotateX() * 3.14159f / 180.0f;
		float rot_y = plac->GetRotateY() * 3.14159f / 180.0f;
		float rot_z = plac->GetRotateZ() * 3.14159f / 180.0f;

		float scale_x = plac->GetScaleX();
		float scale_y = plac->GetScaleY();
		float scale_z = plac->GetScaleZ();

		if (map_models.count(model->GetName()) == 0)
		{
			map_models[model->GetName()] = model;
		}
		std::shared_ptr<EQEmu::Placeable> gen_plac(new EQEmu::Placeable());
		gen_plac->SetFileName(model->GetName());
		gen_plac->SetLocation(offset_x, offset_y, offset_z);
		//y rotation seems backwards on s3ds, probably cause of the weird coord system they used back then
		//x rotation might be too but there are literally 0 x rotated placeables in all the s3ds so who knows
		gen_plac->SetRotation(rot_x, -rot_y, rot_z);
		gen_plac->SetScale(scale_x, scale_y, scale_z);
		map_placeables.push_back(gen_plac);

		eqLogMessage(LogTrace, "Adding placeable %s at (%f, %f, %f)", model->GetName().c_str(), offset_x, offset_y, offset_z);
	}

	eqLogMessage(LogTrace, "Processing s3d animated placeables.");
	pl_sz = placables_skeleton.size();
	for (size_t i = 0; i < pl_sz; ++i)
	{
		auto& plac = placables_skeleton[i].first;

		auto& bones = placables_skeleton[i].second->GetBones();

		if (bones.size() > 0)
		{
			float offset_x = plac->GetX();
			float offset_y = plac->GetY();
			float offset_z = plac->GetZ();

			float rot_x = plac->GetRotateX() * 3.14159f / 180.0f;
			float rot_y = plac->GetRotateY() * 3.14159f / 180.0f;
			float rot_z = plac->GetRotateZ() * 3.14159f / 180.0f;

			float scale_x = plac->GetScaleX();
			float scale_y = plac->GetScaleY();
			float scale_z = plac->GetScaleZ();
			TraverseBone(bones[0],
				glm::vec3(offset_x, offset_y, offset_z),
				glm::vec3(rot_x, rot_y, rot_z),
				glm::vec3(scale_x, scale_y, scale_z));
		}
	}

	return true;
}

bool MapGeometryLoader::CompileEQG(
	std::vector<std::shared_ptr<EQEmu::EQG::Geometry>>& models,
	std::vector<std::shared_ptr<EQEmu::Placeable>>& placeables,
	std::vector<std::shared_ptr<EQEmu::EQG::Region>>& regions,
	std::vector<std::shared_ptr<EQEmu::Light>>& lights)
{
	collide_verts.clear();
	collide_indices.clear();
	non_collide_verts.clear();
	non_collide_indices.clear();
	current_collide_index = 0;
	current_non_collide_index = 0;
	collide_vert_to_index.clear();
	non_collide_vert_to_index.clear();
	map_models.clear();
	map_eqg_models.clear();
	map_placeables.clear();

	for (uint32_t i = 0; i < placeables.size(); ++i)
	{
		std::shared_ptr<EQEmu::Placeable>& plac = placeables[i];
		std::shared_ptr<EQEmu::EQG::Geometry> model;
		bool is_ter = false;

		if (plac->GetName().substr(0, 3).compare("TER") == 0
			|| plac->GetFileName().substr(plac->GetFileName().length() - 4, 4).compare(".ter") == 0)
		{
			is_ter = true;
		}

		for (uint32_t j = 0; j < models.size(); ++j)
		{
			if (models[j]->GetName().compare(plac->GetFileName()) == 0)
			{
				model = models[j];
				break;
			}
		}

		if (!model)
		{
			eqLogMessage(LogWarn, "Could not find placeable %s.", plac->GetFileName().c_str());
			continue;
		}

		float offset_x = plac->GetX();
		float offset_y = plac->GetY();
		float offset_z = plac->GetZ();

		float rot_x = plac->GetRotateX() * 3.14159f / 180.0f;
		float rot_y = plac->GetRotateY() * 3.14159f / 180.0f;
		float rot_z = plac->GetRotateZ() * 3.14159f / 180.0f;

		float scale_x = plac->GetScaleX();
		float scale_y = plac->GetScaleY();
		float scale_z = plac->GetScaleZ();

		if (!is_ter)
		{
			if (map_eqg_models.count(model->GetName()) == 0)
			{
				map_eqg_models[model->GetName()] = model;
			}

			std::shared_ptr<EQEmu::Placeable> gen_plac(new EQEmu::Placeable());
			gen_plac->SetFileName(model->GetName());
			gen_plac->SetLocation(offset_x, offset_y, offset_z);
			gen_plac->SetRotation(rot_x, rot_y, rot_z);
			gen_plac->SetScale(scale_x, scale_y, scale_z);
			map_placeables.push_back(gen_plac);

			eqLogMessage(LogTrace, "Adding placeable %s at (%f, %f, %f)", model->GetName().c_str(), offset_x, offset_y, offset_z);
			continue;
		}

		auto& mod_polys = model->GetPolygons();
		auto& mod_verts = model->GetVertices();

		for (uint32_t j = 0; j < mod_polys.size(); ++j)
		{
			auto& current_poly = mod_polys[j];
			auto v1 = mod_verts[current_poly.verts[0]];
			auto v2 = mod_verts[current_poly.verts[1]];
			auto v3 = mod_verts[current_poly.verts[2]];


			float t = v1.pos.x;
			v1.pos.x = v1.pos.y;
			v1.pos.y = t;

			t = v2.pos.x;
			v2.pos.x = v2.pos.y;
			v2.pos.y = t;

			t = v3.pos.x;
			v3.pos.x = v3.pos.y;
			v3.pos.y = t;

			if (current_poly.flags & 0x01)
				AddFace(v1.pos, v2.pos, v3.pos, false);
			else
				AddFace(v1.pos, v2.pos, v3.pos, true);
		}
	}

	return true;
}

bool MapGeometryLoader::CompileEQGv4()
{
	collide_verts.clear();
	collide_indices.clear();
	non_collide_verts.clear();
	non_collide_indices.clear();
	current_collide_index = 0;
	current_non_collide_index = 0;
	collide_vert_to_index.clear();
	non_collide_vert_to_index.clear();
	map_models.clear();
	map_eqg_models.clear();
	map_placeables.clear();

	if (!terrain)
		return false;

	auto& water_sheets = terrain->GetWaterSheets();
	for (size_t i = 0; i < water_sheets.size(); ++i)
	{
		auto& sheet = water_sheets[i];

		if (sheet->GetTile())
		{
			auto& tiles = terrain->GetTiles();
			for (size_t j = 0; j < tiles.size(); ++j)
			{
				float x = tiles[j]->GetX();
				float y = tiles[j]->GetY();
				float z = tiles[j]->GetBaseWaterLevel();

				float QuadVertex1X = x;
				float QuadVertex1Y = y;
				float QuadVertex1Z = z;

				float QuadVertex2X = QuadVertex1X + (terrain->GetOpts().quads_per_tile * terrain->GetOpts().units_per_vert);
				float QuadVertex2Y = QuadVertex1Y;
				float QuadVertex2Z = QuadVertex1Z;

				float QuadVertex3X = QuadVertex2X;
				float QuadVertex3Y = QuadVertex1Y + (terrain->GetOpts().quads_per_tile * terrain->GetOpts().units_per_vert);
				float QuadVertex3Z = QuadVertex1Z;

				float QuadVertex4X = QuadVertex1X;
				float QuadVertex4Y = QuadVertex3Y;
				float QuadVertex4Z = QuadVertex1Z;

				uint32_t current_vert = (uint32_t)non_collide_verts.size() + 3;
				non_collide_verts.push_back(glm::vec3(QuadVertex1X, QuadVertex1Y, QuadVertex1Z));
				non_collide_verts.push_back(glm::vec3(QuadVertex2X, QuadVertex2Y, QuadVertex2Z));
				non_collide_verts.push_back(glm::vec3(QuadVertex3X, QuadVertex3Y, QuadVertex3Z));
				non_collide_verts.push_back(glm::vec3(QuadVertex4X, QuadVertex4Y, QuadVertex4Z));

				non_collide_indices.push_back(current_vert);
				non_collide_indices.push_back(current_vert - 2);
				non_collide_indices.push_back(current_vert - 1);

				non_collide_indices.push_back(current_vert);
				non_collide_indices.push_back(current_vert - 3);
				non_collide_indices.push_back(current_vert - 2);
			}
		}
		else
		{
			uint32_t id = (uint32_t)non_collide_verts.size();

			non_collide_verts.push_back(glm::vec3(sheet->GetMinY(), sheet->GetMinX(), sheet->GetZHeight()));
			non_collide_verts.push_back(glm::vec3(sheet->GetMinY(), sheet->GetMaxX(), sheet->GetZHeight()));
			non_collide_verts.push_back(glm::vec3(sheet->GetMaxY(), sheet->GetMinX(), sheet->GetZHeight()));
			non_collide_verts.push_back(glm::vec3(sheet->GetMaxY(), sheet->GetMaxX(), sheet->GetZHeight()));

			non_collide_indices.push_back(id);
			non_collide_indices.push_back(id + 1);
			non_collide_indices.push_back(id + 2);

			non_collide_indices.push_back(id + 1);
			non_collide_indices.push_back(id + 3);
			non_collide_indices.push_back(id + 2);
		}
	}

	auto& invis_walls = terrain->GetInvisWalls();
	for (size_t i = 0; i < invis_walls.size(); ++i)
	{
		auto& wall = invis_walls[i];
		auto& verts = wall->GetVerts();

		for (size_t j = 0; j < verts.size(); ++j)
		{
			if (j + 1 == verts.size())
				break;

			float t;
			auto v1 = verts[j];
			auto v2 = verts[j + 1];

			t = v1.x;
			v1.x = v1.y;
			v1.y = t;

			t = v2.x;
			v2.x = v2.y;
			v2.y = t;

			glm::vec3 v3 = v1;
			v3.z += 1000.0;

			glm::vec3 v4 = v2;
			v4.z += 1000.0;

			AddFace(v2, v1, v3, true);
			AddFace(v3, v4, v2, true);

			AddFace(v3, v1, v2, true);
			AddFace(v2, v4, v3, true);
		}
	}

	// map_eqg_models
	auto& models = terrain->GetModels();
	auto model_iter = models.begin();
	while (model_iter != models.end())
	{
		auto& model = model_iter->second;
		if (map_eqg_models.count(model->GetName()) == 0)
		{
			map_eqg_models[model->GetName()] = model;
		}
		++model_iter;
	}

	// map_placeables
	auto& pgs = terrain->GetPlaceableGroups();
	for (size_t i = 0; i < pgs.size(); ++i)
	{
		map_group_placeables.push_back(pgs[i]);
	}

	return true;
}

void MapGeometryLoader::AddFace(glm::vec3& v1, glm::vec3& v2, glm::vec3& v3, bool collidable)
{
	if (!collidable)
	{
		std::tuple<float, float, float> tt = std::make_tuple(v1.x, v1.y, v1.z);
		auto iter = non_collide_vert_to_index.find(tt);
		if (iter == non_collide_vert_to_index.end())
		{
			non_collide_vert_to_index[tt] = current_non_collide_index;
			non_collide_verts.push_back(v1);
			non_collide_indices.push_back(current_non_collide_index);

			++current_non_collide_index;
		}
		else
		{
			uint32_t t_idx = iter->second;
			non_collide_indices.push_back(t_idx);
		}

		tt = std::make_tuple(v2.x, v2.y, v2.z);
		iter = non_collide_vert_to_index.find(tt);
		if (iter == non_collide_vert_to_index.end())
		{
			non_collide_vert_to_index[tt] = current_non_collide_index;
			non_collide_verts.push_back(v2);
			non_collide_indices.push_back(current_non_collide_index);

			++current_non_collide_index;
		}
		else
		{
			uint32_t t_idx = iter->second;
			non_collide_indices.push_back(t_idx);
		}

		tt = std::make_tuple(v3.x, v3.y, v3.z);
		iter = non_collide_vert_to_index.find(tt);
		if (iter == non_collide_vert_to_index.end())
		{
			non_collide_vert_to_index[tt] = current_non_collide_index;
			non_collide_verts.push_back(v3);
			non_collide_indices.push_back(current_non_collide_index);

			++current_non_collide_index;
		}
		else
		{
			uint32_t t_idx = iter->second;
			non_collide_indices.push_back(t_idx);
		}
	}
	else
	{
		std::tuple<float, float, float> tt = std::make_tuple(v1.x, v1.y, v1.z);
		auto iter = collide_vert_to_index.find(tt);
		if (iter == collide_vert_to_index.end())
		{
			collide_vert_to_index[tt] = current_collide_index;
			collide_verts.push_back(v1);
			collide_indices.push_back(current_collide_index);

			++current_collide_index;
		}
		else
		{
			uint32_t t_idx = iter->second;
			collide_indices.push_back(t_idx);
		}

		tt = std::make_tuple(v2.x, v2.y, v2.z);
		iter = collide_vert_to_index.find(tt);
		if (iter == collide_vert_to_index.end()) {
			collide_vert_to_index[tt] = current_collide_index;
			collide_verts.push_back(v2);
			collide_indices.push_back(current_collide_index);

			++current_collide_index;
		}
		else
		{
			uint32_t t_idx = iter->second;
			collide_indices.push_back(t_idx);
		}

		tt = std::make_tuple(v3.x, v3.y, v3.z);
		iter = collide_vert_to_index.find(tt);
		if (iter == collide_vert_to_index.end())
		{
			collide_vert_to_index[tt] = current_collide_index;
			collide_verts.push_back(v3);
			collide_indices.push_back(current_collide_index);

			++current_collide_index;
		}
		else
		{
			uint32_t t_idx = iter->second;
			collide_indices.push_back(t_idx);
		}
	}
}

void MapGeometryLoader::RotateVertex(glm::vec3& v, float rx, float ry, float rz)
{
	glm::vec3 nv = v;

	nv.y = (cos(rx) * v.y) - (sin(rx) * v.z);
	nv.z = (sin(rx) * v.y) + (cos(rx) * v.z);

	v = nv;

	nv.x = (cos(ry) * v.x) + (sin(ry) * v.z);
	nv.z = -(sin(ry) * v.x) + (cos(ry) * v.z);

	v = nv;

	nv.x = (cos(rz) * v.x) - (sin(rz) * v.y);
	nv.y = (sin(rz) * v.x) + (cos(rz) * v.y);

	v = nv;
}

void MapGeometryLoader::ScaleVertex(glm::vec3& v, float sx, float sy, float sz)
{
	v.x = v.x * sx;
	v.y = v.y * sy;
	v.z = v.z * sz;
}

void MapGeometryLoader::TranslateVertex(glm::vec3& v, float tx, float ty, float tz)
{
	v.x = v.x + tx;
	v.y = v.y + ty;
	v.z = v.z + tz;
}
