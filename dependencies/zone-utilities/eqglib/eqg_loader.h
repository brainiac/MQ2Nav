
#pragma once

#include "eqg_geometry.h"
#include "eqg_terrain.h"
#include "light.h"
#include "placeable.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace eqg {

class Archive;
class BufferReader;
class Light;
class LODList;
class Material;
class Placeable;
class ResourceManager;
class TerrainSystem;
class TerrainArea;
struct SEQZoneParameters;
struct SZONInstanceNew;
struct SMaterialFaceVertexData;

class EQGLoader
{
public:
	EQGLoader(ResourceManager* resourceMgr);
	~EQGLoader();

	bool Load(Archive* archive, int loadFlags = 0);

	std::vector<std::shared_ptr<Geometry>> models;
	std::vector<std::shared_ptr<Placeable>> placeables;
	std::vector<std::shared_ptr<TerrainArea>> areas;
	std::vector<std::shared_ptr<Light>> lights;
	std::shared_ptr<TerrainSystem> terrain;
	std::shared_ptr<Geometry> terrain_model;

	std::vector<std::string> mesh_names;
	std::vector<std::string> actor_tags;

	std::map<std::string, std::shared_ptr<MaterialPalette>> material_palettes;
	std::map<std::string, std::shared_ptr<Material>> materials;

	Archive* GetArchive() const { return m_archive; }

private:
	bool ParseFile(const std::string& fileName);

	bool ParseZone(const std::vector<char>& buffer, const std::string& tag);
	bool ParseZoneV2(const std::vector<char>& buffer, const std::string& tag);
	bool ParseModel(const std::vector<char>& buffer, const std::string& fileName, const std::string& tag);
	bool ParseTerrain(const std::vector<char>& buffer, const std::string& fileName, const std::string& tag);
	bool ParseLOD(const std::vector<char>& buffer, const std::string& tag);
	bool ParseTerrainProject(const std::vector<char>& buffer);
	bool ParseMaterialsFacesAndVertices(BufferReader& reader, const char* stringPool, const std::string& tag,
		uint32_t num_faces, uint32_t num_vertices, uint32_t num_materials, bool new_format, SMaterialFaceVertexData* out_data);

	Archive*                 m_archive = nullptr;
	ResourceManager*         m_resourceMgr = nullptr;
	std::string              m_fileName;
	int                      m_loadFlags = 0;
};

struct LODListElement
{
	LODListElement(std::string_view data);

	std::string definition;

	enum LODElementType
	{
		Unknown = 0,
		LOD,
		Collision
	};
	LODElementType type = Unknown;
	float max_distance = 10000.0f;
};
using LODListElementPtr = std::shared_ptr<LODListElement>;

class LODList : public eqg::Resource
{
public:
	LODList(std::string_view tag);
	bool Init(const std::vector<char>& buffer);

	static ResourceType GetStaticResourceType() { return ResourceType::LODList; }

	std::string_view GetTag() const override { return m_tag; }
	const std::vector<LODListElementPtr>& GetElements() const { return m_elements; }
	const LODListElementPtr& GetCollision() const { return m_collision; }

private:
	std::string m_tag;
	std::vector<LODListElementPtr> m_elements;
	LODListElementPtr m_collision;
};
using LODListPtr = std::shared_ptr<LODList>;

} // namespace eqg

using PlaceablePtr = std::shared_ptr<eqg::Placeable>;
using EQGGeometryPtr = std::shared_ptr<eqg::Geometry>;
using TerrainPtr = std::shared_ptr<eqg::TerrainSystem>;
using TerrainAreaPtr = std::shared_ptr<eqg::TerrainArea>;
