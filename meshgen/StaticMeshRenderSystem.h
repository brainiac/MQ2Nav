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

namespace eqg
{
	class SimpleModelDefinition;
}

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
	bgfx::UniformHandle m_uniformHasTexture = BGFX_INVALID_HANDLE;
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

