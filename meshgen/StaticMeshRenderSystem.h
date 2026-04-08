//
// StaticMeshRenderSystem.h
//

#pragma once

#include "bgfx/bgfx.h"
#include "entt/entity/registry.hpp"
#include "glm/glm.hpp"

#include <unordered_map>
#include <vector>

class MGSimpleModel;
class MGTerrain;
class MGTerrainTile;
class ZoneRenderManager;

struct MaterialBatch;

namespace eqg
{
	class Material;
	class SimpleModelDefinition;
}

// Material batch for rendering - groups faces by material for texture binding
struct MaterialBatch
{
	eqg::Material* material = nullptr;  // Material for this batch
	uint32_t startIndex = 0;            // Start index in index buffer
	uint32_t indexCount = 0;            // Number of indices in this batch

	bool isAlphaBlend = false;
};

// Vertex format for static mesh rendering
// Structured for future texture support
struct StaticMeshVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;
	uint32_t  colorDiffuse;  // ABGR

	static void Init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();
	}

	inline static bgfx::VertexLayout ms_layout;
};

// Instance data for instanced rendering
struct MeshInstanceData
{
	glm::vec4 row0;  // Transform matrix row 0
	glm::vec4 row1;  // Transform matrix row 1
	glm::vec4 row2;  // Transform matrix row 2
	glm::vec4 row3;  // Transform matrix row 3

	MeshInstanceData() = default;

	explicit MeshInstanceData(const glm::mat4& transform)
		: row0(transform[0])
		, row1(transform[1])
		, row2(transform[2])
		, row3(transform[3])
	{
	}

	static void Init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
			.end();
	}

	inline static bgfx::VertexLayout ms_layout;
};

// Render system for static mesh geometry (SimpleModel and Terrain)
class StaticMeshRenderSystem
{
public:
	StaticMeshRenderSystem();
	~StaticMeshRenderSystem();

	void Init(ZoneRenderManager* renderManager);
	void Shutdown();

	void SetRegistry(entt::registry* registry);
	void SetDirty() { m_dirty = true; }

	bool GetUseVertexColors() const { return m_useVertexColors; }
	void SetUseVertexColors(bool use) { m_useVertexColors = use; }

	void Update();
	void Render();

private:
	void RebuildRenderData();
	void RenderMaterialBatch(const glm::mat4& worldMtx, const MaterialBatch& batch,
		bgfx::VertexBufferHandle vertexBuffer, bgfx::IndexBufferHandle indexBuffer);

	void OnStaticMeshConstruct(entt::registry& registry, entt::entity entity);
	void OnStaticMeshDestroy(entt::registry& registry, entt::entity entity);
	void OnTerrainConstruct(entt::registry& registry, entt::entity entity);
	void OnTerrainDestroy(entt::registry& registry, entt::entity entity);
	void OnTerrainTileConstruct(entt::registry& registry, entt::entity entity);
	void OnTerrainTileDestroy(entt::registry& registry, entt::entity entity);
	void OnHiddenConstruct(entt::registry& registry, entt::entity entity);
	void OnHiddenDestroy(entt::registry& registry, entt::entity entity);

private:
	entt::registry* m_registry = nullptr;
	ZoneRenderManager* m_renderManager = nullptr;
	bool m_useVertexColors = true;

	bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uniformUseVertexColors = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_texColorSampler = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uniformTextureFlags = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uniformGlobalAmbient = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle m_whiteTexture = BGFX_INVALID_HANDLE;

	struct RenderBatch
	{
		eqg::SimpleModelDefinition* definition = nullptr;
		std::vector<glm::mat4> transforms;
		std::vector<MGSimpleModel*> models;
	};
	std::unordered_map<eqg::SimpleModelDefinition*, RenderBatch> m_batches;

	entt::entity m_terrainEntity = entt::null;
	MGTerrain* m_terrain = nullptr;

	std::vector<MGTerrainTile*> m_terrainTiles;

	bool m_dirty = true;

	entt::connection m_staticMeshConstructConnection;
	entt::connection m_staticMeshDestroyConnection;
	entt::connection m_terrainConstructConnection;
	entt::connection m_terrainDestroyConnection;
	entt::connection m_terrainTileConstructConnection;
	entt::connection m_terrainTileDestroyConnection;
	entt::connection m_hiddenConstructConnection;
	entt::connection m_hiddenDestroyConnection;
};

