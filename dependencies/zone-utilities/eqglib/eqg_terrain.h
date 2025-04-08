#pragma once

#include "eqg_structs.h"
#include "eqg_loader.h"
#include "eqg_types_fwd.h"
#include "placeable.h"

#include <memory>
#include <map>

namespace eqg {

class BufferReader;
class Light;
class TerrainSystem;
class TerrainTile;
struct TerrainObjectGroup;

//------------------------------------------------------------------------------
// Invisible Walls

class InvisibleWall
{
public:
	explicit InvisibleWall(std::string_view name_)
		: m_name(name_) {}

	const std::string& GetName() const { return m_name; }
	float GetWallHeight() const { return m_wallTopHeight; }
	const std::vector<glm::vec3>& GetVertices() const { return m_vertices; }

	bool Load(BufferReader& reader);

private:
	std::string              m_name;
	float                    m_wallTopHeight = 100.0f;
	std::vector<glm::vec3>   m_vertices;
};
using InvisibleWallPtr = std::shared_ptr<InvisibleWall>;

//------------------------------------------------------------------------------
// WaterSheet

class WaterSheetData
{
public:
	WaterSheetData(uint32_t index = 0);

	bool Init(const std::vector<std::string>& tokens, size_t& k);
	bool ParseToken(const std::string& token, const std::vector<std::string>& tokens, size_t& k);

	MaterialPtr CreateMaterial(ResourceManager* resourceMgr);

	uint32_t GetIndex() const { return m_index; }

private:
	uint32_t                 m_index = 0;
	float                    m_fresnelBias = 0.25f;
	float                    m_fresnelPower = 8.0f;
	float                    m_reflectionAmount = 0.7f;
	float                    m_uvScale = 1.0f;
	glm::vec4                m_reflectionColor = { 0.7f, 1.0f, 1.0f, 1.0f };
	glm::vec4                m_waterColor1 = { 0.0f, 0.04f, 0.11f, 1.0f };
	glm::vec4                m_waterColor2 = { 0.0f, 0.23f, 0.17f, 1.0f };
	std::string              m_normalMap = "Resources\\WaterSwap\\water_n.dds";
	std::string              m_environmentMap = "Resources\\WaterSwap\\water_e.dds";

	MaterialPtr              m_material;
	static inline uint32_t   s_oldWaterDataIndex = 10000;
};
using WaterSheetDataPtr = std::shared_ptr<WaterSheetData>;

class WaterSheet
{
public:
	WaterSheet(TerrainSystem* terrain, const std::string& name, const std::shared_ptr<WaterSheetData>& data = nullptr);

	bool Init(const std::vector<std::string>& tokens, size_t& k);
	bool ParseToken(const std::string& token, const std::vector<std::string>& tokens, size_t& k);

	ActorDefinitionPtr CreateActorDefinition();

	std::string              m_name;
	WaterSheetDataPtr        m_data;
	std::string              m_definitionName;
	float                    m_minX = 0.0f;
	float                    m_minY = 0.0f;
	float                    m_maxX = 0.0f;
	float                    m_maxY = 0.0f;
	float                    m_zHeight = 0.0f;
	std::vector<SEQMVertex>  m_vertices;
	std::vector<SEQMFace>    m_faces;

	TerrainSystem*           m_terrain = nullptr;
	ResourceManager*         m_resourceMgr = nullptr;
	SimpleActorPtr           m_actor;
};

//------------------------------------------------------------------------------
// Terrain Object Group Definitions

struct TerrainObjectGroupDefinitionObjectElement
{
	std::string name;
	glm::vec3 position;
	glm::vec3 orientation;
	float scale;
	int object_id = 0;

	glm::mat4x4 transform;

	struct ElementFile
	{
		std::string tag;
		std::string fileName;
	};

	std::vector<ElementFile> files;

	bool Load(const std::vector<std::string>& tokens, size_t& k);
};
using TerrainObjectGroupDefinitionObjectElementPtr = std::shared_ptr<TerrainObjectGroupDefinitionObjectElement>;

struct TerrainObjectGroupDefinitionAreaElement
{
	std::string name;
	glm::vec3 position;
	glm::vec3 orientation;
	glm::vec3 extents;

	glm::mat4x4 transform;

	bool Load(const std::vector<std::string>& tokens, size_t& k);
};
using TerrainObjectGroupDefinitionAreaElementPtr = std::shared_ptr<TerrainObjectGroupDefinitionAreaElement>;

struct TerrainObjectGroupDefinition
{
	std::string name;
	std::vector<TerrainObjectGroupDefinitionObjectElementPtr> objects;
	std::vector<TerrainObjectGroupDefinitionAreaElementPtr> areas;

	bool Load(Archive* archive, const std::string& group_name);
};
using TerrainObjectGroupDefinitionPtr = std::shared_ptr<TerrainObjectGroupDefinition>;

//------------------------------------------------------------------------------
// Terrain Areas

class TerrainArea
{
public:
	std::string    name;
	std::string    shape;
	uint32_t       type = 0;

	glm::vec3      position;
	glm::vec3      orientation;
	glm::vec3      extents;

	glm::mat4x4    transform;
	glm::vec3      scale; // should probably just remove

	TerrainObjectGroup* group = nullptr;
};
using TerrainAreaPtr = std::shared_ptr<TerrainArea>;

//-------------------------------------------------------------------------------
// Terrain Objects

struct TerrainObject
{
	int            objectId = -1;
	std::string    name;
	std::string    ecosystem;
	uint8_t        shadeFactor = 255;

	glm::vec3      position;
	glm::vec3      orientation;
	glm::vec3      scale;

	glm::mat4x4    transform;

	uint32_t*      rgbLitData = nullptr;

	TerrainObjectGroup* group = nullptr;
};
using TerrainObjectPtr = std::shared_ptr<TerrainObject>;

struct TerrainObjectGroup
{
	std::string    name;

	glm::vec3      position;
	glm::vec3      orientation;
	glm::vec3      scale;

	glm::mat4x4    transform;

	std::vector<TerrainObjectPtr> objects;
	std::vector<TerrainAreaPtr> areas;

	void Initialize(TerrainObjectGroupDefinition* definition);

	TerrainObjectGroupDefinition* m_definition = nullptr;
};
using TerrainObjectGroupPtr = std::shared_ptr<TerrainObjectGroup>;

//-------------------------------------------------------------------------------
// Terrain Lights

struct TerrainLightFrame
{
	uint32_t color;
	float intensity;
};

class TerrainLightDefinition
{
public:
	TerrainLightDefinition(std::string_view tag);

	bool Init(ResourceManager* resourceMgr);
	bool Load(Archive* archive, ResourceManager* resourceMgr);

	const std::string& GetTag() const { return m_tag; }

	void AddFrame(float intensity, uint32_t color);

	const LightDefinitionPtr& GetDefinition() { return m_definition; }

private:
	void UpdateLightDefinition();

	std::string        m_tag;
	std::vector<TerrainLightFrame> m_frames;
	int                m_currentFrame = 0;
	int                m_updateInterval = 100;
	bool               m_skipFrames = false;

	LightDefinitionPtr m_definition;
};
using TerrainLightDefinitionPtr = std::shared_ptr<TerrainLightDefinition>;

class TerrainLight
{
public:
	TerrainLight(ResourceManager* resourceMgr, const std::string& name,
		const TerrainLightDefinitionPtr& definition)
		: m_name(name), m_definition(definition)
	{
		UpdateLightInstance(resourceMgr);
	}

	const std::string& GetName() const { return m_name; }

	void SetRadius(float radius) { m_radius = radius; }
	float GetRadius() const { return m_radius; }

	void SetPosition(const glm::vec3& position) { m_position = position; }
	const glm::vec3& GetPosition() const { return m_position; }

	void SetOrientation(const glm::vec3& orientation) { m_orientation = orientation; }
	const glm::vec3& GetOrientation() const { return m_orientation; }

	void SetScale(const glm::vec3& scale) { m_scale = scale; }
	const glm::vec3& GetScale() const { return m_scale; }

	void SetStatic(bool isStatic) { m_static = isStatic; }
	bool IsStatic() const { return m_static; }

	void UpdateLightInstance(ResourceManager* resourceMgr);

	std::string        m_name;
	float              m_radius = 100.0f;
	glm::vec3          m_position{ 0.0f };
	glm::vec3          m_orientation{ 0.0f };
	glm::vec3          m_scale{ 0.0f };
	bool               m_static = false;
	
	TerrainLightDefinitionPtr m_definition;
	PointLightPtr      m_light;
};
using TerrainLightPtr = std::shared_ptr<TerrainLight>;

//-------------------------------------------------------------------------------
// Terrain Tile

class TerrainTile
{
	friend class TerrainSystem;

public:
	TerrainTile(TerrainSystem* parent);
	~TerrainTile();

	bool Load(BufferReader& reader, int version);

	glm::vec3 GetPosInTile(const glm::vec3& pos) const;
	int GetTileLocX() const { return m_tileLoc.x; }
	int GetTileLocY() const { return m_tileLoc.y; }
	
	glm::ivec2               m_tileLoc; // Tile coordinates in tile grid
	glm::vec3                m_tilePos; // Actual world coordinates of tile.
	uint32_t                 m_floraSeed = 0;
	bool                     m_flat = false;
	glm::mat4x4              m_tileTransform;

	std::vector<float>       m_heightField;
	std::vector<uint32_t>    m_vertexColor;
	std::vector<uint32_t>    m_bakedLighting;
	std::vector<uint8_t>     m_quadFlags;

	// WaterSheet parameters
	float                    m_baseWaterLevel = -1000.0f;
	int                      m_waterDataIndex = 1;
	bool                     m_hasWaterSheet = false;
	float                    m_waterSheetMinX = 0.0f;
	float                    m_waterSheetMaxX = 0.0f;
	float                    m_waterSheetMinY = 0.0f;
	float                    m_waterSheetMaxY = 0.0f;
	float                    m_lavaLevel = -1000.0f;
	WaterSheet*              m_waterSheet = nullptr;

	std::vector<TerrainAreaPtr> m_areas;
	std::vector<TerrainObjectPtr> m_objects;
	std::vector<TerrainObjectGroupPtr> m_groups;
	std::vector<TerrainLightPtr> m_lights;

	TerrainSystem*           m_terrain = nullptr;
};
using TerrainTilePtr = std::shared_ptr<TerrainTile>;

//-------------------------------------------------------------------------------
// Terrain System

class TerrainSystem
{
	friend class TerrainTile;

public:
	TerrainSystem(Archive* archive, ResourceManager* resourceMgr);
	~TerrainSystem();

	bool Load(const char* zonBuffer, size_t size);
	bool Load(const SEQZoneParameters& params);

	std::vector<TerrainTilePtr>& GetTiles() { return m_tiles; }
	uint32_t GetQuadsPerTile() { return m_params.quads_per_tile; }
	float GetUnitsPerVertex() { return m_params.units_per_vert; }

	std::vector<WaterSheetPtr>& GetWaterSheets() { return m_waterSheets; }
	std::vector<InvisibleWallPtr>& GetInvisWalls() { return m_invisWalls; }

	static void LoadZoneParameters(const char* buffer, size_t size, SEQZoneParameters& params);
	const SEQZoneParameters& GetParams() const { return m_params; }

	TerrainObjectGroupDefinition* GetObjectGroupDefinition(const std::string& name);

	WaterSheetDataPtr GetWaterSheetData(uint32_t index) const;

	TerrainLightDefinitionPtr GetLightDefinition(std::string_view tag);


	ResourceManager* GetResourceManager() const { return m_resourceMgr; }

private:
	bool LoadTiles();
	bool LoadWaterSheets();
	bool LoadInvisibleWalls();

	SEQZoneParameters     m_params;
	Archive*              m_archive = nullptr;
	ResourceManager*      m_resourceMgr = nullptr;

public:
	uint32_t version = 0;
	float zone_min_x = 0.0f;
	float zone_min_y = 0.0f;
	uint32_t quad_count = 0;
	uint32_t vert_count = 0;

	std::vector<TerrainTilePtr>    m_tiles;

	std::vector<WaterSheetPtr>     m_waterSheets;
	std::vector<WaterSheetDataPtr> m_waterSheetData;
	std::vector<InvisibleWallPtr>  m_invisWalls;
	std::vector<TerrainObjectGroupDefinitionPtr> m_groupDefinitions;
	std::vector<TerrainLightDefinitionPtr> m_lightDefinitions;
	TerrainLightDefinitionPtr      m_defaultLightDefinition;

	std::vector<TerrainLightPtr>   m_lights;
	std::vector<TerrainAreaPtr>    m_areas;


	std::vector<std::shared_ptr<Placeable>> objects;
};

//=================================================================================================

struct STerrainWLDData;

// Environment Types parsed from WLD and EQG areas
struct AreaEnvironment
{
	// Type of environment (water, lava, etc.)
	enum Type : uint8_t
	{
		Type_None,
		UnderWater,
		UnderSlime,
		UnderLava,
		UnderIceWater,
		UnderWater2,
		UnderWater3,
	};

	// Flags on the environment
	enum Flags : uint8_t
	{
		Flags_None         = 0,
		Slippery           = 0x04,
		Kill               = 0x10,
		Teleport           = 0x20,
		TeleportIndex      = 0x40,
	};

	Type      type;
	Flags     flags;
	int16_t   teleportIndex;

	AreaEnvironment(std::string_view areaTag);
	AreaEnvironment() : type(Type_None), flags(Flags_None), teleportIndex(0) {}
	AreaEnvironment(Type env) : type(env), flags(Flags_None), teleportIndex(0) {}
	AreaEnvironment(Flags flags) : type(Type_None), flags(flags), teleportIndex(0) {}
	AreaEnvironment(Flags flags, uint16_t teleportIndex) : type(Type_None), flags(flags), teleportIndex(teleportIndex) {}
	AreaEnvironment(Type env, Flags flags) : type(env), flags(flags), teleportIndex(0) {}
	AreaEnvironment(Type env, Flags flags, uint16_t teleportIndex) : type(env), flags(flags), teleportIndex(teleportIndex) {}

	// Combines another environment with this one to produce a new environment with flags mixed
	AreaEnvironment operator|(AreaEnvironment other) const;
};
static_assert(sizeof(AreaEnvironment) == 4);

AreaEnvironment ParseAreaTag(std::string_view areaTag);

inline AreaEnvironment::AreaEnvironment(std::string_view areaTag)
{
	*this = ParseAreaTag(areaTag);
}

inline AreaEnvironment::Flags operator|(AreaEnvironment::Flags a, AreaEnvironment::Flags b) { return static_cast<AreaEnvironment::Flags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b)); }
inline AreaEnvironment::Flags operator|=(AreaEnvironment::Flags& a, AreaEnvironment::Flags b) { return a = a | b; }
inline AreaEnvironment::Flags operator&(AreaEnvironment::Flags a, AreaEnvironment::Flags b) { return static_cast<AreaEnvironment::Flags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b)); }
inline AreaEnvironment::Flags operator&=(AreaEnvironment::Flags& a, AreaEnvironment::Flags b) { return a = a & b; }

inline AreaEnvironment AreaEnvironment::operator|(AreaEnvironment other) const
{
	// Other environment type replacees, but flags mix.
	return AreaEnvironment(other.type, flags | other.flags, other.teleportIndex ? other.teleportIndex : 0);
}

struct AreaTeleport
{
	std::string tag;
	int teleportIndex;

	glm::vec3 position;
	float heading;
	int zoneId;
};
bool ParseAreaTeleportTag(std::string_view areaTag, AreaTeleport& teleport);

struct SArea
{
	std::string tag;
	std::string userData;
	std::vector<uint32_t> regionNumbers;
	std::vector<glm::vec3> centers;
};

class Terrain
{
public:
	Terrain();
	virtual ~Terrain();

	bool InitFromWLDData(const STerrainWLDData& wldData);

private:
	std::vector<glm::vec3> m_vertices;
	std::vector<glm::vec2> m_uvs;
	std::vector<glm::vec3> m_normals;
	std::vector<uint32_t>  m_rgbColors;
	std::vector<uint32_t>  m_rgbTints;
	std::vector<SFace>     m_faces;
	glm::vec3              m_bbMin;
	glm::vec3              m_bbMax;
	std::shared_ptr<MaterialPalette> m_materialPalette;
	uint32_t               m_constantAmbientColor = 0;

	// WLD/S3D data
	uint32_t               m_numWLDRegions = 0;
	std::vector<SArea>     m_wldAreas;
	std::vector<uint32_t>  m_wldAreaIndices;
	std::vector<AreaEnvironment> m_wldAreaEnvironments;
	std::shared_ptr<SWorldTreeWLDData> m_wldBspTree;

	// Teleports
	std::vector<AreaTeleport> m_teleports;
};


} // namespace eqg
