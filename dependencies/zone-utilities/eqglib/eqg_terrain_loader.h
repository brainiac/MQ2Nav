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
class TerrainObjectGroup;

//------------------------------------------------------------------------------

// Structure used repeatedly to represent positional information for objects in the terrain system
struct TerrainLoc
{
	glm::ivec2 tile;          // tile index in 2d tile grid
	glm::vec3  position;      // tile-space position (x, z) -- y is generally not used.
	glm::vec3  orientation;   // orientation in degrees
	glm::vec3  scale;         // scale

	// In many places, a height offset is used to adjust the position of the object from the tile height.

	TerrainLoc()
		: tile(0, 0)
		, position(0.f)
		, orientation(0.f)
		, scale(0.f)
	{
	}
};


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

	MaterialPtr CreateMaterial();

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
	SimpleActorPtr           m_actor;
};

//------------------------------------------------------------------------------
// Terrain Object Group Definitions

// Object element defined by a TerrainObjectGroup definition.
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
	std::vector<uint32_t> litData;

	bool Load(const std::vector<std::string>& tokens, size_t& k);
};
using TerrainObjectGroupDefinitionObjectElementPtr = std::shared_ptr<TerrainObjectGroupDefinitionObjectElement>;

// Area element defined by a TerrainObjectGroup definition.
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

// Definition for a TerrainObjectGroup.
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

class TerrainObject
{
public:
	TerrainObject(std::string_view name, const ActorPtr& pActor, int objectID);

	int GetObjectID() const { return m_objectID; }
	void SetObjectID(int objectID) { m_objectID = objectID; }

	void Update();

	std::string    m_name;
	int            m_objectID;
	ActorPtr       m_actor;

	std::string    ecosystem;
	uint8_t        shadeFactor = 255;

	glm::vec3      position{ 0 };
	glm::vec3      orientation{ 0 };
	glm::vec3      scale{ 0 };

	glm::mat4x4    transform{ 0 };

	uint32_t*      rgbLitData = nullptr;

	TerrainObjectGroup* group = nullptr;
};
using TerrainObjectPtr = std::shared_ptr<TerrainObject>;

class TerrainObjectGroup
{
public:
	TerrainObjectGroup(TerrainTile* terrain, const TerrainLoc& loc, float zOffset);

	void Initialize(TerrainObjectGroupDefinition* definition);

	glm::vec3      m_position;
	glm::vec3      m_orientation;
	glm::vec3      m_scale;
	glm::mat4x4    m_transform;

	TerrainTile*                  m_tile = nullptr;
	TerrainObjectGroupDefinition* m_definition = nullptr;
	std::vector<TerrainObjectPtr> m_objects;
	std::vector<TerrainAreaPtr>   m_areas;
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

	bool Init();
	bool Load(Archive* archive);

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
	TerrainLight(const std::string& name,
		const TerrainLightDefinitionPtr& definition)
		: m_name(name), m_definition(definition)
	{
		UpdateLightInstance();
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

	void UpdateLightInstance();

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
	TerrainSystem(Archive* archive);
	~TerrainSystem();

	bool Load(const char* zonBuffer, size_t size);
	bool Load(const SEQZoneParameters& params);

	uint32_t GetVersion() const { return m_version; }
	glm::vec2 GetZoneMin() const { return m_zoneMin; }

	std::vector<TerrainTilePtr>& GetTiles() { return m_tiles; }
	uint32_t GetQuadsPerTile() const { return m_params.quads_per_tile; }
	float GetUnitsPerVertex() const { return m_params.units_per_vert; }
	uint32_t GetQuadCount() const { return m_quadCount; }
	uint32_t GetVertCount() const { return m_vertCount; }

	std::vector<WaterSheetPtr>& GetWaterSheets() { return m_waterSheets; }
	std::vector<InvisibleWallPtr>& GetInvisWalls() { return m_invisWalls; }

	static void LoadZoneParameters(const char* buffer, size_t size, SEQZoneParameters& params);
	const SEQZoneParameters& GetParams() const { return m_params; }

	TerrainObjectGroupDefinition* GetObjectGroupDefinition(const std::string& name);
	WaterSheetDataPtr GetWaterSheetData(uint32_t index) const;
	TerrainLightDefinitionPtr GetLightDefinition(std::string_view tag);
	TerrainObjectPtr CreateTerrainObject(std::string_view name, int objectID);
	TerrainObjectPtr CreateTerrainObject(
		TerrainObjectGroupDefinitionObjectElement* groupElement,
		TerrainObjectGroup* group);

private:
	bool LoadTiles();
	bool LoadWaterSheets();
	bool LoadInvisibleWalls();

	int GetNextObjectID();

	SEQZoneParameters              m_params;
	Archive*                       m_archive = nullptr;

public:
	uint32_t                       m_version = 0;
	glm::vec2                      m_zoneMin{ 0.0f };
	uint32_t                       m_quadCount = 0;
	uint32_t                       m_vertCount = 0;
	int                            m_maxObjectID = 0;

	std::vector<TerrainTilePtr>    m_tiles;

	std::vector<WaterSheetPtr>     m_waterSheets;
	std::vector<WaterSheetDataPtr> m_waterSheetData;
	std::vector<InvisibleWallPtr>  m_invisWalls;
	std::vector<TerrainObjectGroupDefinitionPtr> m_groupDefinitions;
	std::vector<TerrainLightDefinitionPtr> m_lightDefinitions;
	TerrainLightDefinitionPtr      m_defaultLightDefinition;
	std::unordered_map<int, TerrainObjectPtr> m_objects;

	std::vector<TerrainLightPtr>   m_lights;
	std::vector<TerrainAreaPtr>    m_areas;
};

} // namespace eqg
