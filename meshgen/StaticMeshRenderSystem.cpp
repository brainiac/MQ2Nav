//
// StaticMeshRenderSystem.cpp
//

#include "pch.h"
#include "meshgen/StaticMeshRenderSystem.h"

#include <entt/entity/handle.hpp>

#include "meshgen/Components.h"
#include "meshgen/EQComponents.h"
#include "meshgen/Entity.h"
#include "meshgen/MGSimpleModel.h"
#include "meshgen/MGTerrain.h"
#include "meshgen/ResourceManager.h"
#include "meshgen/ZoneRenderManager.h"

#include "eqglib/eqg_geometry.h"

#include "spdlog/spdlog.h"
#include "glm/gtc/type_ptr.hpp"

//============================================================================

StaticMeshRenderSystem::StaticMeshRenderSystem()
{
}

StaticMeshRenderSystem::~StaticMeshRenderSystem()
{
	Shutdown();
}

void StaticMeshRenderSystem::Init(ZoneRenderManager* renderManager)
{
	m_renderManager = renderManager;

	// Load shader program
	m_program = g_resourceMgr->GetProgramHandle("staticmesh");

	// Initialize vertex layouts
	StaticMeshVertex::Init();
}

void StaticMeshRenderSystem::Shutdown()
{
	if (m_registry)
	{
		m_staticMeshConstructConnection.release();
		m_staticMeshDestroyConnection.release();
		m_terrainConstructConnection.release();
		m_terrainDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();
	}

	m_batches.clear();
	m_terrain = nullptr;
	m_terrainEntity = entt::null;
	m_registry = nullptr;
	m_renderManager = nullptr;
}

void StaticMeshRenderSystem::SetRegistry(entt::registry* registry)
{
	// Disconnect from old registry if any
	if (m_registry)
	{
		m_staticMeshConstructConnection.release();
		m_staticMeshDestroyConnection.release();
		m_terrainConstructConnection.release();
		m_terrainDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();

		m_batches.clear();
		m_terrain = nullptr;
		m_terrainEntity = entt::null;
	}

	m_registry = registry;

	// Register component callbacks on new registry
	if (m_registry)
	{
		m_staticMeshConstructConnection = m_registry->on_construct<StaticMeshRenderComponent>()
			.connect<&StaticMeshRenderSystem::OnStaticMeshConstruct>(this);
		m_staticMeshDestroyConnection = m_registry->on_destroy<StaticMeshRenderComponent>()
			.connect<&StaticMeshRenderSystem::OnStaticMeshDestroy>(this);
		m_terrainConstructConnection = m_registry->on_construct<TerrainRenderComponent>()
			.connect<&StaticMeshRenderSystem::OnTerrainConstruct>(this);
		m_terrainDestroyConnection = m_registry->on_destroy<TerrainRenderComponent>()
			.connect<&StaticMeshRenderSystem::OnTerrainDestroy>(this);
		m_hiddenConstructConnection = m_registry->on_construct<HiddenComponent>()
			.connect<&StaticMeshRenderSystem::OnHiddenConstruct>(this);
		m_hiddenDestroyConnection = m_registry->on_destroy<HiddenComponent>()
			.connect<&StaticMeshRenderSystem::OnHiddenDestroy>(this);
	}

	m_dirty = true;
}

void StaticMeshRenderSystem::OnStaticMeshConstruct(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void StaticMeshRenderSystem::OnStaticMeshDestroy(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void StaticMeshRenderSystem::OnTerrainConstruct(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void StaticMeshRenderSystem::OnTerrainDestroy(entt::registry& registry, entt::entity entity)
{
	m_terrain = nullptr;
	m_terrainEntity = entt::null;
	m_dirty = true;
}

void StaticMeshRenderSystem::OnHiddenConstruct(entt::registry& registry, entt::entity entity)
{
	// Only care about entities with render components
	if (registry.any_of<StaticMeshRenderComponent, TerrainRenderComponent>(entity))
	{
		m_dirty = true;
	}
}

void StaticMeshRenderSystem::OnHiddenDestroy(entt::registry& registry, entt::entity entity)
{
	// Only care about entities with render components
	if (registry.any_of<StaticMeshRenderComponent, TerrainRenderComponent>(entity))
	{
		m_dirty = true;
	}
}

void StaticMeshRenderSystem::RebuildRenderData()
{
	m_batches.clear();
	m_terrain = nullptr;
	m_terrainEntity = entt::null;

	if (!m_registry)
		return;

	// Collect terrain
	{
		auto view = m_registry->view<TerrainRenderComponent>();
		for (auto entity : view)
		{
			if (m_registry->any_of<HiddenComponent>(entity))
				continue;

			const auto& terrainComp = view.get<TerrainRenderComponent>(entity);
			if (terrainComp.terrain)
			{
				m_terrain = terrainComp.terrain;
				m_terrainEntity = entity;
				break;  // Only one terrain per zone
			}
		}
	}

	// Collect static meshes grouped by definition
	{
		auto view = m_registry->view<StaticMeshRenderComponent, TransformComponent>();
		for (auto entity : view)
		{
			if (m_registry->any_of<HiddenComponent>(entity))
				continue;

			const auto& meshComp = view.get<StaticMeshRenderComponent>(entity);
			const auto& transformComp = view.get<TransformComponent>(entity);

			if (!meshComp.model || !meshComp.model->GetDefinition())
				continue;

			eqg::SimpleModelDefinition* def = meshComp.model->GetDefinition().get();

			// Get world transform
			entt::handle handle{ *m_registry, entity };
			glm::mat4 worldMatrix = GetWorldSpaceTransformMatrix(handle);

			auto& batch = m_batches[def];
			batch.definition = def;
			batch.transforms.push_back(worldMatrix);
			batch.models.push_back(meshComp.model);
		}
	}

	m_dirty = false;
}

void StaticMeshRenderSystem::Update()
{
	if (!m_registry)
		return;

	if (m_dirty)
	{
		RebuildRenderData();
	}
}

void StaticMeshRenderSystem::Render()
{
	if (!m_registry)
		return;

	if (!bgfx::isValid(m_program))
		return;

	// Render terrain first
	if (m_terrain)
	{
		// Build GPU buffers if needed
		if (!m_terrain->HasGPUBuffers())
		{
			m_terrain->BuildGPUBuffers();
		}

		if (m_terrain->HasGPUBuffers())
		{
			bgfx::Encoder* encoder = bgfx::begin();

			// Set identity transform for terrain (it's already in world space)
			glm::mat4 identity(1.0f);
			encoder->setTransform(glm::value_ptr(identity));

			encoder->setVertexBuffer(0, m_terrain->GetVertexBuffer());
			encoder->setIndexBuffer(m_terrain->GetIndexBuffer(), 0, m_terrain->GetIndexCount());
			encoder->setState(
				BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
				BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
				BGFX_STATE_CULL_CW);

			encoder->submit(0, m_program);
		}
	}

	// Render static meshes
	// For now, render each model individually with its transform
	// Future: use instancing for models sharing the same definition
	for (auto& [def, batch] : m_batches)
	{
		for (size_t i = 0; i < batch.models.size(); ++i)
		{
			MGSimpleModel* model = batch.models[i];
			const glm::mat4& transform = batch.transforms[i];

			// Build GPU buffers if needed
			if (!model->HasGPUBuffers())
			{
				model->BuildGPUBuffers();
			}

			if (!model->HasGPUBuffers())
				continue;

			bgfx::Encoder* encoder = bgfx::begin();

			encoder->setTransform(glm::value_ptr(transform));

			encoder->setVertexBuffer(0, model->GetVertexBuffer());
			encoder->setIndexBuffer(model->GetIndexBuffer(), 0, model->GetIndexCount());
			encoder->setState(
				BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
				BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
				BGFX_STATE_CULL_CW);

			encoder->submit(0, m_program);
		}
	}
}

