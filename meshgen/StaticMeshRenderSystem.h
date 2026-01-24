//
// StaticMeshRenderSystem.h
//

#pragma once

#include <bgfx/bgfx.h>
#include <entt/entity/registry.hpp>
#include <glm/glm.hpp>

#include <unordered_map>
#include <vector>

class MGSimpleModel;
class MGTerrain;
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

	void Update();
	void Render();

private:
	void RebuildRenderData();

	void OnStaticMeshConstruct(entt::registry& registry, entt::entity entity);
	void OnStaticMeshDestroy(entt::registry& registry, entt::entity entity);
	void OnTerrainConstruct(entt::registry& registry, entt::entity entity);
	void OnTerrainDestroy(entt::registry& registry, entt::entity entity);
	void OnHiddenConstruct(entt::registry& registry, entt::entity entity);
	void OnHiddenDestroy(entt::registry& registry, entt::entity entity);

private:
	entt::registry* m_registry = nullptr;
	ZoneRenderManager* m_renderManager = nullptr;

	bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;

	// Per-definition render batches (for instancing in the future)
	struct RenderBatch
	{
		eqg::SimpleModelDefinition* definition = nullptr;
		std::vector<glm::mat4> transforms;
		std::vector<MGSimpleModel*> models;
	};
	std::unordered_map<eqg::SimpleModelDefinition*, RenderBatch> m_batches;

	// Terrain entity (singleton per zone)
	entt::entity m_terrainEntity = entt::null;
	MGTerrain* m_terrain = nullptr;

	bool m_dirty = true;

	entt::connection m_staticMeshConstructConnection;
	entt::connection m_staticMeshDestroyConnection;
	entt::connection m_terrainConstructConnection;
	entt::connection m_terrainDestroyConnection;
	entt::connection m_hiddenConstructConnection;
	entt::connection m_hiddenDestroyConnection;
};

