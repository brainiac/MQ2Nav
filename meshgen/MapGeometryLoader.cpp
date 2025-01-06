//
// MapGeometryLoader.cpp
//

#include "meshgen/MapGeometryLoader.h"

#include "common/ZoneData.h"
#include "common/NavMeshData.h"
#include "zone-utilities/log/log_macros.h"
#include "Recast.h"

#include <rapidjson/document.h>

#include <filesystem>
#include <sstream>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>


namespace fs = std::filesystem;

constexpr float ROT_RATIO = 3.14159f / 180.0f;

static void RotateVertex(glm::vec3& v, float rx, float ry, float rz)
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

//============================================================================================================

ZoneCollisionMesh::ZoneCollisionMesh()
{
}

ZoneCollisionMesh::~ZoneCollisionMesh()
{
	clear();
}

void ZoneCollisionMesh::clear()
{
	delete[] m_verts;
	delete[] m_normals;
	delete[] m_tris;
}

void ZoneCollisionMesh::setMaxExtents(const std::pair<glm::vec3, glm::vec3>& maxExtents)
{
	m_maxExtents.first = maxExtents.first.zxy;
	m_maxExtents.second = maxExtents.second.zxy;

	m_maxExtentsSet = true;
}

void ZoneCollisionMesh::addVertex(float x, float y, float z)
{
	if (m_vertCount + 1 > vcap)
	{
		vcap = !vcap ? 8 : vcap * 2;
		float* nv = new float[vcap * 3];
		if (m_vertCount)
			memcpy(nv, m_verts, m_vertCount * 3 * sizeof(float));
		delete[] m_verts;
		m_verts = nv;
	}
	float* dst = &m_verts[m_vertCount * 3];
	*dst++ = x * m_scale;
	*dst++ = y * m_scale;
	*dst++ = z * m_scale;
	m_vertCount++;
}

void ZoneCollisionMesh::addTriangle(int a, int b, int c)
{
	if (m_triCount + 1 > tcap)
	{
		tcap = !tcap ? 8 : tcap * 2;
		int* nv = new int[tcap * 3];
		if (m_triCount)
			memcpy(nv, m_tris, m_triCount * 3 * sizeof(int));
		delete[] m_tris;
		m_tris = nv;
	}
	int* dst = &m_tris[m_triCount * 3];
	*dst++ = a;
	*dst++ = b;
	*dst++ = c;
	m_triCount++;
}

void ZoneCollisionMesh::addTriangle(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3)
{
	int index = m_vertCount;

	addVertex(v1.y, v1.z, v1.x);
	addVertex(v2.y, v2.z, v2.x);
	addVertex(v3.y, v3.z, v3.x);

	addTriangle(index, index + 1, index + 2);
}

void ZoneCollisionMesh::addTerrain(const TerrainPtr& terrain)
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

			if (ArePointsOutsideExtents(glm::vec3{ y, x, z }))
				continue;

			int index = m_vertCount;

			addVertex(x, z, y);
			addVertex(x + dt, z, y);
			addVertex(x + dt, z, y + dt);
			addVertex(x, z, y + dt);

			addTriangle(index + 0, index + 2, index + 1);
			addTriangle(index + 2, index + 0, index + 3);
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

				if (ArePointsOutsideExtents(glm::vec3{ _y, _x, z1 }))
					continue;

				int index = m_vertCount;

				addVertex(_x, z1, _y);
				addVertex(_x + dt, z2, _y);
				addVertex(_x + dt, z3, _y + dt);
				addVertex(_x, z4, _y + dt);

				addTriangle(index + 0, index + 2, index + 1);
				addTriangle(index + 2, index + 0, index + 3);
			}
		}
	}
}

void ZoneCollisionMesh::addPolys(const std::vector<glm::vec3>& verts, const std::vector<uint32_t>& indices)
{
	for (uint32_t index = 0; index < indices.size(); index += 3)
	{
		const glm::vec3& vert1 = verts[indices[index]];
		const glm::vec3& vert2 = verts[indices[index + 2]];
		const glm::vec3& vert3 = verts[indices[index + 1]];

		if (ArePointsOutsideExtents(vert1.yxz, vert2.yxz, vert3.yxz))
			continue;

		int current_index = m_vertCount;

		addVertex(vert1.x, vert1.z, vert1.y);
		addVertex(vert2.x, vert2.z, vert2.y);
		addVertex(vert3.x, vert3.z, vert3.y);

		addTriangle(current_index, current_index + 2, current_index + 1);
	}
}

// 0x10 = invisible
// 0x01 = no collision
auto isVisible = [](int flags)
	{
		bool invisible = (flags & 0x01) || (flags & 0x10);

		return !invisible;
	};

// or?
// bool visible = (iter->flags & 0x11) == 0;

void ZoneCollisionMesh::addModel(std::string_view name, const S3DGeometryPtr& model)
{
	std::shared_ptr<ModelEntry> entry = std::make_shared<ModelEntry>();

	for (const auto& vert : model->GetVertices())
	{
		entry->verts.push_back(vert.pos);
	}

	for (const auto& poly : model->GetPolygons())
	{
		bool visible = isVisible(poly.flags);

		entry->polys.emplace_back(
			ModelEntry::Poly{
				.indices=glm::ivec3{poly.verts[0], poly.verts[1], poly.verts[2]},
				.vis=visible,
				.flags=poly.flags
			});
	}

	m_models.emplace(name, std::move(entry));
}

void ZoneCollisionMesh::addModel(std::string_view name, const EQGGeometryPtr& model)
{
	std::shared_ptr<ModelEntry> entry = std::make_shared<ModelEntry>();

	for (const auto& vert : model->GetVertices())
	{
		entry->verts.push_back(vert.pos);
	}

	for (const auto& poly : model->GetPolygons())
	{
		bool visible = isVisible(poly.flags);

		entry->polys.emplace_back(
			ModelEntry::Poly{
				.indices=glm::ivec3{poly.verts[0], poly.verts[1], poly.verts[2]},
				.vis=visible,
				.flags=poly.flags
			});
	}

	m_models.emplace(name, std::move(entry));
}

void ZoneCollisionMesh::addModelInstance(const PlaceablePtr& obj, const glm::mat4x4& transform)
{
	const std::string& name = obj->GetFileName();

	auto modelIter = m_models.find(name);
	if (modelIter == m_models.end())
	{
		return;
	}

	// some objects have a really wild position, just ignore them.
	if (obj->GetZ() < -30000 || obj->GetX() > 15000 || obj->GetY() > 15000 || obj->GetZ() > 15000)
		return;

	// Get model transform
	auto mtx = transform * obj->GetTransform();

	if (IsPointOutsideExtents(glm::vec3{ mtx * glm::vec4{ 0., 0., 0., 1. } }))
	{
		eqLogMessage(LogWarn, "Ignoring placement of '%s' at { %.2f %.2f %.2f } due to being outside of max extents",
			obj->GetFileName().c_str(), obj->GetX(), obj->GetY(), obj->GetZ());
		return;
	}

	const auto& model = modelIter->second;
	for (const auto& poly : model->polys)
	{
		if (!poly.vis)
			continue;

		addTriangle(
			glm::vec3(mtx * glm::vec4(model->verts[poly.indices[0]], 1)),
			glm::vec3(mtx * glm::vec4(model->verts[poly.indices[1]], 1)),
			glm::vec3(mtx * glm::vec4(model->verts[poly.indices[2]], 1))
		);
	}
}

void ZoneCollisionMesh::addZoneGeometry(const S3DGeometryPtr& model)
{
	auto& mod_polys = model->GetPolygons();
	auto& mod_verts = model->GetVertices();

	for (uint32_t j = 0; j < mod_polys.size(); ++j)
	{
		auto& current_poly = mod_polys[j];
		auto v1 = mod_verts[current_poly.verts[0]];
		auto v2 = mod_verts[current_poly.verts[1]];
		auto v3 = mod_verts[current_poly.verts[2]];

		if ((current_poly.flags & EQEmu::S3D::S3D_FACEFLAG_PASSABLE) == 0)
		{
			addTriangle(v1.pos, v2.pos, v3.pos);
		}
	}
}

void ZoneCollisionMesh::finalize()
{
	m_normals = new float[m_triCount * 3];

	for (int i = 0; i < m_triCount * 3; i += 3)
	{
		const float* v0 = &m_verts[m_tris[i] * 3];
		const float* v1 = &m_verts[m_tris[i + 1] * 3];
		const float* v2 = &m_verts[m_tris[i + 2] * 3];
	
		float e0[3], e1[3];
		for (int j = 0; j < 3; ++j)
		{
			e0[j] = v1[j] - v0[j];
			e1[j] = v2[j] - v0[j];
		}

		float* n = &m_normals[i];
		n[0] = e0[1] * e1[2] - e0[2] * e1[1];
		n[1] = e0[2] * e1[0] - e0[0] * e1[2];
		n[2] = e0[0] * e1[1] - e0[1] * e1[0];

		float d = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
		if (d > 0)
		{
			d = 1.0f / d;
			n[0] *= d;
			n[1] *= d;
			n[2] *= d;
		}
	}

	rcCalcBounds(getVerts(), getVertCount(), glm::value_ptr(m_boundsMin), glm::value_ptr(m_boundsMax));
}

//============================================================================================================

MapGeometryLoader::MapGeometryLoader(const std::string& zoneShortName,
	const std::string& everquest_path, const std::string& mesh_path)
	: m_zoneName(zoneShortName)
	, m_eqPath(everquest_path)
	, m_meshPath(mesh_path)
{
}

MapGeometryLoader::~MapGeometryLoader()
{
}

bool MapGeometryLoader::Load()
{
	Clear();

	if (!LoadZone())
	{
		Clear();

		return false;
	}

	BuildCollisionMesh();

	m_loaded = true;
	return true;
}

void MapGeometryLoader::BuildCollisionMesh()
{
	m_collisionMesh.clear();

	// load terrain geometry
	if (terrain)
	{
		m_collisionMesh.addTerrain(terrain);
	}

	// load s3d zone geometry
	for (auto& model : map_s3d_geometry)
	{
		m_collisionMesh.addZoneGeometry(model);
	}

	m_collisionMesh.addPolys(collide_verts, collide_indices);

	// Load models
	for (auto& [name, model] : map_models)
	{
		m_collisionMesh.addModel(name, model);
	}

	for (auto& [name, model] : map_eqg_models)
	{
		m_collisionMesh.addModel(name, model);
	}

	// Add model instances

	glm::mat4x4 identity_mat = glm::identity<glm::mat4x4>();

	// Model instances with identity transform
	for (const auto& obj : map_placeables)
	{
		m_collisionMesh.addModelInstance(obj, identity_mat);
	}

	// Model instances from a model group with group's transform.
	for (const auto& group : map_group_placeables)
	{
		glm::mat4x4 groupTransform = group->GetTransform();

		for (const auto& obj : group->GetPlaceables())
		{
			m_collisionMesh.addModelInstance(obj, groupTransform);
		}
	}

	// Add dynamic object instances
	for (const auto& dynObj : dynamic_map_objects)
	{
		m_collisionMesh.addModelInstance(dynObj, identity_mat);
	}

	m_collisionMesh.finalize();
}

struct DoorParams
{
	uint32_t id;
	std::string name;
	uint16_t type;
	float scale;
	glm::mat4x4 transform;
};

bool IsSwitchStationary(DWORD type)
{
	return (type != 53 && type >= 50 && type < 59)
		|| (type >= 153 && type <= 155);
}


void MapGeometryLoader::LoadDoors()
{
	//
	// Load the door data
	//
	std::string filename = m_meshPath + "\\" + m_zoneName + "_doors.json";

	std::error_code ec;
	if (!fs::is_regular_file(filename, ec))
		return;

	std::stringstream ss;
	std::ifstream ifs;

	ifs.open(filename.c_str());
	ss << ifs.rdbuf();
	ifs.close();

	rapidjson::Document document;

	if (document.Parse<0>(ss.str().c_str()).HasParseError())
		return;
	if (!document.IsArray())
		return;

	std::vector<DoorParams> doors;
	m_hasDynamicObjects = true;

	for (auto iter = document.Begin(); iter != document.End(); ++iter)
	{
		if (!iter->IsObject())
			return;

		DoorParams params;
		params.id = (*iter)["ID"].GetInt();
		params.name = (*iter)["Name"].GetString();
		params.type = (*iter)["Type"].GetInt();
		params.scale = (float)(*iter)["Scale"].GetDouble();

		// only add stationary objects
		if (!IsSwitchStationary(params.type))
			continue;

		const auto& t = (*iter)["Transform"];
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				params.transform[i][j] = (float)t[i][j].GetDouble();
			}
		}

		doors.push_back(params);
	}

	if (doors.empty())
	{
		m_doorsLoaded = true;
		return;
	}

	for (DoorParams& params : doors)
	{
		std::shared_ptr<EQEmu::Placeable> gen_plac = std::make_shared<EQEmu::Placeable>();
		gen_plac->SetFileName(params.name);

		glm::vec3 scale;
		glm::quat rotation;
		glm::vec3 translation;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(params.transform, scale, rotation, translation, skew, perspective);

		gen_plac->SetPosition(translation);
		gen_plac->SetRotation(glm::eulerAngles(rotation));
		gen_plac->SetScale(params.scale * scale);
		gen_plac->SetName(params.name + "_ACTORDEF");

		dynamic_map_objects.push_back(gen_plac);

		glm::vec3 pos = glm::vec3(params.transform[3]);

		eqLogMessage(LogTrace, "Adding placeable from dynamic objects %s at (%f, %f, %f)", params.name.c_str(), pos.x, pos.y, pos.z);
		++m_dynamicObjects;
	}
}

//============================================================================

void MapGeometryLoader::Clear()
{
	m_collisionMesh.clear();
	m_doorsLoaded = false;
	m_loaded = false;

	collide_verts.clear();
	collide_indices.clear();
	non_collide_verts.clear();
	non_collide_indices.clear();
	current_collide_index = 0;
	current_non_collide_index = 0;
	collide_vert_to_index.clear();
	non_collide_vert_to_index.clear();
	terrain.reset();
	map_models.clear();
	map_anim_models.clear();
	map_eqg_models.clear();

	map_s3d_model_instances.clear();
	map_placeables.clear();
	map_group_placeables.clear();
	map_s3d_geometry.clear();
	dynamic_map_objects.clear();
	m_dynamicObjects = 0;
	m_hasDynamicObjects = false;

	m_wldLoaders.clear();
	m_archives.clear();
}

bool MapGeometryLoader::LoadZone()
{

	// Load <zonename>_EnvironmentEmitters.txt

	// Check if .eqg file exists
	std::string filePath = fmt::format("{}\\{}.eqg", m_eqPath, m_zoneName);
	bool usingEQG = false;

	std::error_code ec;
	if (fs::exists(filePath, ec))
	{
		usingEQG = true;
		if (!TryLoadEQG())
		{
			return false;
		}

		// Successful EQG Load

		LoadDoors();
	}
	else
	{
		if (!TryLoadS3D())
		{
			return false;
		}
	}

	return true;
}

bool MapGeometryLoader::TryLoadS3D()
{
	std::string filePath = m_eqPath + "\\" + m_zoneName;

	eqLogMessage(LogTrace, "Attempting to load %s.s3d as a s3d.", m_zoneName.c_str());

	EQEmu::PFS::Archive* zone_archive = LoadArchive(filePath + ".s3d");
	if (!zone_archive)
	{
		return false; // Zone doesn't exist
	}

	// Load zone data
	EQEmu::S3D::WLDLoader* zone_loader = LoadWLD(zone_archive, m_zoneName + ".wld");
	if (!zone_loader)
	{
		return false;
	}

	// if poknowledge, load poknowledge_obj3.eqg
	if (m_zoneName == "poknowledge")
	{
		
	}

	// Load object definitions
	if (EQEmu::PFS::Archive* object_archive = LoadArchive(filePath + "_obj2.s3d"))
	{
		LoadWLD(object_archive, m_zoneName + "_obj2.wld");
	}

	if (EQEmu::PFS::Archive* object_archive = LoadArchive(filePath + "_obj.s3d"))
	{
		LoadWLD(object_archive, m_zoneName + "_obj.wld");
	}

	if (EQEmu::PFS::Archive* object_archive = LoadArchive(filePath + "_2_obj.s3d"))
	{
		LoadWLD(object_archive, m_zoneName + "_2_obj.wld");
	}

	// If this is LDON zone, load obequip_lexit.s3d

	// Load <zonename>_chr.txt
	// Load <zonename>_assets.txt
	LoadAssetsFile();

	// Load object placements
	LoadWLD(zone_archive, "objects.wld");
	LoadWLD(zone_archive, "lights.wld");

	LoadDoors();

	return CompileS3D(zone_loader);
}

bool MapGeometryLoader::TryLoadEQG()
{
	eqLogMessage(LogTrace, "Attempting to load %s.eqg as a standard eqg.", m_zoneName.c_str());

	std::string filePath = m_eqPath + "\\" + m_zoneName;

	EQEmu::PFS::Archive* archive = LoadArchive(filePath + ".eqg");
	if (!archive)
	{
		eqLogMessage(LogTrace, "Failed to open %s.eqg as a standard eqg file because the file does not exist.", filePath.c_str());
		return false;
	}

	EQEmu::EQGLoader eqgLoader;

	eqLogMessage(LogTrace, "Attempting to load %s.eqg as a eqg.", m_zoneName.c_str());

	if (eqgLoader.Load(archive, filePath))
	{
		eqLogMessage(LogInfo, "Loaded %s.eqg as an eqg", m_zoneName.c_str());

		return CompileEQG(eqgLoader);
	}

	eqLogMessage(LogTrace, "Attempting to load %s.eqg as a v4 eqg.", m_zoneName.c_str());
	EQEmu::EQG4Loader eqg4;
	if (eqg4.Load(filePath, terrain))
	{
		eqLogMessage(LogInfo, "Loaded %s.eqg as a v4 eqg", m_zoneName.c_str());
		return CompileEQGv4();
	}

	return false;
}

EQEmu::PFS::Archive* MapGeometryLoader::LoadArchive(const std::string& path)
{
	for (const auto& archive : m_archives)
	{
		if (path == archive->GetFileName())
			return archive.get();
	}

	auto archive = std::make_unique<EQEmu::PFS::Archive>();
	if (archive->Open(path))
	{
		m_archives.push_back(std::move(archive));
		eqLogMessage(LogInfo, "Loaded archive: %s", fs::path(path).filename().c_str());

		return m_archives.back().get();
	}

	return nullptr;
}

EQEmu::S3D::WLDLoader* MapGeometryLoader::LoadWLD(EQEmu::PFS::Archive* archive, const std::string& fileName)
{
	for (const auto& loader : m_wldLoaders)
	{
		if (fileName == loader->GetFileName())
			return loader.get();
	}

	auto loader = std::make_unique<EQEmu::S3D::WLDLoader>();
	if (!loader->Init(archive, fileName))
		return nullptr;

	// Load objects from the .wld
	auto& objectList = loader->GetObjectList();
	for (auto& obj : objectList)
	{
		if (obj.type == EQEmu::S3D::WLD_OBJ_ZONE_TYPE)
		{
			EQEmu::S3D::WLDFragment29* frag = static_cast<EQEmu::S3D::WLDFragment29*>(obj.parsed_data);
			auto region = frag->region;

			eqLogMessage(LogTrace, "Processing zone region '%s' '%s' for s3d.", region->tag.c_str(), region->old_style_tag.c_str());
		}
		else if (obj.type == EQEmu::S3D::WLD_OBJ_REGION_TYPE)
		{
			// Regions with DMSPRITEDEF2 make up the geometry of the zone
			EQEmu::S3D::WLDFragment22* region_frag = static_cast<EQEmu::S3D::WLDFragment22*>(obj.parsed_data);

			if (region_frag->region_sprite_index != -1 && region_frag->region_sprite_is_def)
			{
				// Get the geometry from this sprite index
				auto& sprite_obj = loader->GetObject(region_frag->region_sprite_index);
				if (sprite_obj.type == EQEmu::S3D::WLD_OBJ_DMSPRITEDEFINITION2_TYPE)
				{
					auto model = static_cast<EQEmu::S3D::WLDFragment36*>(sprite_obj.parsed_data)->geometry;

					map_s3d_geometry.push_back(model);
				}
			}
		}
		else if (obj.type == EQEmu::S3D::WLD_OBJ_ACTORINSTANCE_TYPE)
		{
			EQEmu::S3D::WLDFragment15* frag = static_cast<EQEmu::S3D::WLDFragment15*>(obj.parsed_data);
			if (!frag->placeable)
			{
				eqLogMessage(LogWarn, "Placeable entry was not found for actor tag '%s'.", obj.tag);
				continue;
			}

			map_s3d_model_instances.push_back(frag->placeable);
		}
		else if (obj.type == EQEmu::S3D::WLD_OBJ_ACTORDEFINITION_TYPE)
		{
			std::string_view tag = obj.tag;
			if (tag.empty())
				continue;

			EQEmu::S3D::WLDFragment14* obj_frag = static_cast<EQEmu::S3D::WLDFragment14*>(obj.parsed_data);
			int sprite_id = obj_frag->sprite_id;

			auto& sprite_obj = loader->GetObjectFromID(sprite_id);

			if (sprite_obj.type == EQEmu::S3D::WLD_OBJ_DMSPRITEINSTANCE_TYPE)
			{
				EQEmu::S3D::WLDFragment2D* r_frag = static_cast<EQEmu::S3D::WLDFragment2D*>(sprite_obj.parsed_data);
				auto m_ref = r_frag->sprite_id;

				EQEmu::S3D::WLDFragment36* mod_frag = static_cast<EQEmu::S3D::WLDFragment36*>(loader->GetObjectFromID(m_ref).parsed_data);
				auto mod = mod_frag->geometry;

				map_models.emplace(tag, mod);
			}
			else if (sprite_obj.type == EQEmu::S3D::WLD_OBJ_HIERARCHICALSPRITEINSTANCE_TYPE)
			{
				EQEmu::S3D::WLDFragment11* r_frag = static_cast<EQEmu::S3D::WLDFragment11*>(sprite_obj.parsed_data);
				auto s_ref = r_frag->def_id;

				EQEmu::S3D::WLDFragment10* skeleton_frag = static_cast<EQEmu::S3D::WLDFragment10*>(loader->GetObjectFromID(s_ref).parsed_data);
				auto skele = skeleton_frag->track;

				map_anim_models.emplace(tag, skele);
			}
		}
	}
	
	m_wldLoaders.push_back(std::move(loader));
	eqLogMessage(LogInfo, "Loaded wld: %s", fileName.c_str());

	return m_wldLoaders.back().get();
}

void MapGeometryLoader::LoadAssetsFile()
{
	// to try to read an _assets file and load more eqg based data.
	std::string assets_file = fmt::format("{}\\{}_assets.txt", m_eqPath, m_zoneName);
	std::vector<std::string> filenames;

	std::error_code ec;
	if (fs::exists(assets_file, ec))
	{
		std::ifstream assets(assets_file.c_str());

		if (assets.is_open())
		{
			std::copy(std::istream_iterator<std::string>(assets),
				std::istream_iterator<std::string>(),
				std::back_inserter(filenames));
		}
	}

	for (const std::string& filename : filenames)
	{
		std::string asset_file = fmt::format("{}\\{}", m_eqPath, filename);
		bool loadEQG = true;

		if (!asset_file.ends_with(".eqg"))
		{
			loadEQG = false;
			asset_file.append(".s3d");
		}

		if (EQEmu::PFS::Archive* archive = LoadArchive(asset_file))
		{
			if (loadEQG)
			{
				std::vector<std::string> models;

				if (archive->GetFilenames("mod", models))
				{
					for (auto& modelName : models)
					{
						EQGGeometryPtr model;

						EQEmu::LoadEQGModel(*archive, modelName, model);
						if (model)
						{
							model->SetName(modelName);
							map_eqg_models.emplace(modelName, model);
						}
					}
				}
			}
			else
			{
				LoadWLD(archive, filename + ".wld");
			}
		}
	}
}

void MapGeometryLoader::TraverseBone(
	std::shared_ptr<EQEmu::S3D::SkeletonTrack::Bone> bone,
	glm::vec3 parent_trans,
	glm::vec3 parent_rot,
	glm::vec3 parent_scale)
{
	glm::vec3 pos = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 rot = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 scale = parent_scale;

	if (!bone->transforms.empty())
	{
		auto& transform = bone->transforms[0];

		if (transform.scale != 0)
		{
			pos = transform.pivot * transform.scale;
		}

		if (transform.rotation.w != 0)
		{
			rot.x = transform.rotation.x / transform.rotation.w;
			rot.y = transform.rotation.y / transform.rotation.w;
			rot.z = transform.rotation.z / transform.rotation.w;

			rot = rot * ROT_RATIO;
		}
	}

	RotateVertex(pos, parent_rot.x, parent_rot.y, parent_rot.z);
	pos += parent_trans;
	rot += parent_rot;

	if (bone->model)
	{
		if (!map_models.contains(bone->model->GetName()))
		{
			map_models[bone->model->GetName()] = bone->model;
		}
		
		std::shared_ptr<EQEmu::Placeable> gen_plac = std::make_shared<EQEmu::Placeable>();
		gen_plac->SetFileName(bone->model->GetName());
		gen_plac->SetPosition(pos);
		gen_plac->SetRotation(rot);
		gen_plac->SetScale(scale);
		map_placeables.push_back(gen_plac);

		eqLogMessage(LogTrace, "Adding placeable from bones %s at (%f, %f, %f)", bone->model->GetName().c_str(), pos.x, pos.y, pos.z);
	}

	for (size_t i = 0; i < bone->children.size(); ++i)
	{
		TraverseBone(bone->children[i], pos, rot, parent_scale);
	}
}

bool MapGeometryLoader::LoadModelInst(const PlaceablePtr& inst)
{
	if (map_models.contains(inst->tag))
	{
		auto model = map_models[inst->tag];

		if (!map_models.contains(model->GetName()))
		{
			map_models[model->GetName()] = model;
		}

		// TODO: Push these changes up to the loader. We're making a copy and modifying the data to fix some quirks.
		std::shared_ptr<EQEmu::Placeable> gen_plac = std::make_shared<EQEmu::Placeable>();
		gen_plac->SetFileName(model->GetName());
		gen_plac->SetPosition(inst->GetPosition());

		// y rotation seems backwards on s3ds, probably cause of the weird coord system they used back then
		// x rotation might be too but there are literally 0 x rotated placeables in all the s3ds so who knows
		gen_plac->SetRotation(inst->GetRotation() * ROT_RATIO);
		gen_plac->rotate.y = -gen_plac->rotate.y;
		gen_plac->SetScale(inst->GetScale());

		map_placeables.push_back(gen_plac);

		eqLogMessage(LogTrace, "Adding model instance '%s' at (%f, %f, %f)", model->GetName().c_str(),
			gen_plac->pos.x, gen_plac->pos.y, gen_plac->pos.z);
	}
	else if (map_anim_models.contains(inst->tag))
	{
		auto skel = map_anim_models[inst->tag];

		auto& bones = skel->GetBones();

		if (!bones.empty())
		{
			TraverseBone(bones[0], inst->GetPosition(), inst->GetRotation() * ROT_RATIO, inst->GetScale());
		}
	}
	else
	{
		eqLogMessage(LogWarn, "Could not find model for '%s'", inst->tag.c_str());
	}

	return true;
}


bool MapGeometryLoader::CompileS3D(EQEmu::S3D::WLDLoader* zone_loader)
{
	eqLogMessage(LogTrace, "Processing zone placeable fragments.");

	// OK now put them all together.

	for (auto& inst : map_s3d_model_instances)
	{
		LoadModelInst(inst);
	}

	for (auto& inst : dynamic_map_objects)
	{
		LoadModelInst(inst);
	}

	return true;
}

bool MapGeometryLoader::CompileEQG(EQEmu::EQGLoader& eqgLoader)
{
	std::vector<std::shared_ptr<EQEmu::EQG::Geometry>>& models = eqgLoader.models;
	std::vector<std::shared_ptr<EQEmu::Placeable>>& placeables = eqgLoader.placeables;
	std::vector<std::shared_ptr<EQEmu::EQG::Region>>& regions = eqgLoader.regions;
	std::vector<std::shared_ptr<EQEmu::Light>>& lights = eqgLoader.lights;

	for (uint32_t i = 0; i < placeables.size(); ++i)
	{
		PlaceablePtr& plac = placeables[i];
		EQGGeometryPtr model;
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

		float rot_x = plac->GetRotateX() * ROT_RATIO;
		float rot_y = plac->GetRotateY() * ROT_RATIO;
		float rot_z = plac->GetRotateZ() * ROT_RATIO;

		float scale_x = plac->GetScaleX();
		float scale_y = plac->GetScaleY();
		float scale_z = plac->GetScaleZ();

		if (!is_ter)
		{
			// TODO: come up with external list of models to ignore.
			if (model->GetName() == "OBJ_BlockerA.MOD")
				continue;

			if (map_eqg_models.count(model->GetName()) == 0)
			{
				map_eqg_models[model->GetName()] = model;
			}

			std::shared_ptr<EQEmu::Placeable> gen_plac(new EQEmu::Placeable());
			gen_plac->SetFileName(model->GetName());
			gen_plac->SetName(model->GetName());
			gen_plac->SetPosition(offset_x, offset_y, offset_z);
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
	if (!terrain)
	{
		return false;
	}

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

void MapGeometryLoader::AddFace(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, bool collidable)
{
	if (!collidable)
	{
		auto InsertVertex = [this](const glm::vec3& vec)
		{
			auto iter = non_collide_vert_to_index.find(vec);
			if (iter == non_collide_vert_to_index.end())
			{
				non_collide_vert_to_index[vec] = current_non_collide_index;
				non_collide_verts.push_back(vec);
				non_collide_indices.push_back(current_non_collide_index);

				++current_non_collide_index;
			}
			else
			{
				uint32_t t_idx = iter->second;
				non_collide_indices.push_back(t_idx);
			}
		};

		InsertVertex(v1);
		InsertVertex(v2);
		InsertVertex(v3);
	}
	else
	{
		auto InsertVertex = [this](const glm::vec3& vec)
		{
			auto iter = collide_vert_to_index.find(vec);
			if (iter == collide_vert_to_index.end())
			{
				collide_vert_to_index[vec] = current_collide_index;
				collide_verts.push_back(vec);
				collide_indices.push_back(current_collide_index);

				++current_collide_index;
			}
			else
			{
				uint32_t t_idx = iter->second;
				collide_indices.push_back(t_idx);
			}
		};

		InsertVertex(v1);
		InsertVertex(v2);
		InsertVertex(v3);
	}
}
