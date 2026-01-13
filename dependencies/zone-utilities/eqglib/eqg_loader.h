
#pragma once

#include "eqg_geometry.h"
#include "eqg_terrain.h"
#include "eqg_types_fwd.h"

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

	Archive* GetArchive() const { return m_archive; }

	std::map<std::string, MaterialPalettePtr> material_palettes;
	std::map<std::string, MaterialPtr> materials;

private:
	bool ParseFile(const std::string& fileName);

	bool ParseZone(const std::vector<char>& buffer, const std::string& tag);
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

	// Temp pointer to .ZON data during load
	std::span<uint8_t>       m_zonData;
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

using TerrainSystemPtr = std::shared_ptr<eqg::TerrainSystem>;
using TerrainPtr = std::shared_ptr<eqg::Terrain>;
using TerrainAreaPtr = std::shared_ptr<eqg::TerrainArea>;
