//
// SkeletalMeshRenderSystem.h
//

#pragma once

#include "meshgen/MaterialBatchRenderer.h"

#include "bgfx/bgfx.h"
#include "entt/entity/registry.hpp"
#include "glm/glm.hpp"

#include <unordered_map>
#include <vector>

class MGHierarchicalModel;
class ZoneRenderManager;

struct MaterialBatch;

namespace eqg
{
	class HierarchicalModelDefinition;
}

// Render system for skeletal mesh geometry (HierarchicalModel)
class SkeletalMeshRenderSystem
{
public:
	SkeletalMeshRenderSystem();
	~SkeletalMeshRenderSystem();

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

	void OnSkeletalMeshConstruct(entt::registry& registry, entt::entity entity);
	void OnSkeletalMeshDestroy(entt::registry& registry, entt::entity entity);
	void OnHiddenConstruct(entt::registry& registry, entt::entity entity);
	void OnHiddenDestroy(entt::registry& registry, entt::entity entity);

private:
	entt::registry* m_registry = nullptr;
	ZoneRenderManager* m_renderManager = nullptr;
	bool m_useVertexColors = true;

	MaterialBatchRenderer m_batchRenderer;

	struct RenderBatch
	{
		eqg::HierarchicalModelDefinition* definition = nullptr;
		std::vector<glm::mat4> transforms;
		std::vector<MGHierarchicalModel*> models;
	};
	std::unordered_map<eqg::HierarchicalModelDefinition*, RenderBatch> m_batches;

	bool m_dirty = true;

	entt::connection m_skeletalMeshConstructConnection;
	entt::connection m_skeletalMeshDestroyConnection;
	entt::connection m_hiddenConstructConnection;
	entt::connection m_hiddenDestroyConnection;
};
