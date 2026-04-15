//
// StaticMeshRenderSystem.h
//

#pragma once

#include "meshgen/RenderBatchManager.h"

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
	class Material;
	class SimpleModelDefinition;
}

// Render system for static mesh geometry (SimpleModel and Terrain)
class StaticMeshRenderSystem
{
public:
	StaticMeshRenderSystem(ZoneRenderManager* renderManager);
	~StaticMeshRenderSystem();

	void SetRegistry(entt::registry* registry);
	void SetDirty() { m_dirty = true; }

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
	void OnPointLightConstruct(entt::registry& registry, entt::entity entity);
	void OnPointLightDestroy(entt::registry& registry, entt::entity entity);

private:
	entt::registry* m_registry = nullptr;
	ZoneRenderManager* m_renderManager = nullptr;
	bool m_dirty = true;

	struct RenderBatch
	{
		eqg::SimpleModelDefinition* definition = nullptr;
		std::vector<glm::mat4> transforms;
		std::vector<MGSimpleModel*> models;

		// Cached point light assignments per model (parallel to transforms/models)
		std::vector<ActivePointLights> lightAssignments;
	};
	std::unordered_map<eqg::SimpleModelDefinition*, RenderBatch> m_batches;

	entt::entity m_terrainEntity = entt::null;
	MGTerrain* m_terrain = nullptr;

	std::vector<MGTerrainTile*> m_terrainTiles;

	entt::connection m_staticMeshConstructConnection;
	entt::connection m_staticMeshDestroyConnection;
	entt::connection m_terrainConstructConnection;
	entt::connection m_terrainDestroyConnection;
	entt::connection m_terrainTileConstructConnection;
	entt::connection m_terrainTileDestroyConnection;
	entt::connection m_hiddenConstructConnection;
	entt::connection m_hiddenDestroyConnection;
	entt::connection m_pointLightConstructConnection;
	entt::connection m_pointLightDestroyConnection;
};

