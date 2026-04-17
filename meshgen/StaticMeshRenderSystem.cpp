//
// StaticMeshRenderSystem.cpp
//

#include "pch.h"
#include "StaticMeshRenderSystem.h"

#include "meshgen/Components.h"
#include "meshgen/EQComponents.h"
#include "meshgen/Entity.h"
#include "meshgen/MGBitmap.h"
#include "meshgen/MGSimpleModel.h"
#include "meshgen/MGTerrain.h"
#include "meshgen/MGTerrainTile.h"
#include "meshgen/RenderBatchManager.h"
#include "meshgen/ResourceManager.h"
#include "meshgen/ZoneRenderManager.h"

#include "eqglib/eqg_geometry.h"
#include "eqglib/eqg_material.h"
#include "eqglib/eqg_light.h"

#include "entt/entity/handle.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>

//============================================================================

StaticMeshRenderSystem::StaticMeshRenderSystem(ZoneRenderManager* renderManager)
{
	m_renderManager = renderManager;
}

StaticMeshRenderSystem::~StaticMeshRenderSystem()
{
	if (m_registry)
	{
		m_staticMeshConstructConnection.release();
		m_staticMeshDestroyConnection.release();
		m_terrainConstructConnection.release();
		m_terrainDestroyConnection.release();
		m_terrainTileConstructConnection.release();
		m_terrainTileDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();
		m_pointLightConstructConnection.release();
		m_pointLightDestroyConnection.release();
	}

	m_batches.clear();
	m_terrain = nullptr;
	m_terrainEntity = entt::null;
	m_terrainTiles.clear();
	m_registry = nullptr;
	m_renderManager = nullptr;
}

void StaticMeshRenderSystem::SetRegistry(entt::registry* registry)
{
	if (m_registry)
	{
		m_staticMeshConstructConnection.release();
		m_staticMeshDestroyConnection.release();
		m_terrainConstructConnection.release();
		m_terrainDestroyConnection.release();
		m_terrainTileConstructConnection.release();
		m_terrainTileDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();
		m_pointLightConstructConnection.release();
		m_pointLightDestroyConnection.release();

		m_batches.clear();
		m_terrain = nullptr;
		m_terrainEntity = entt::null;
		m_terrainTiles.clear();
	}

	m_registry = registry;

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
		m_terrainTileConstructConnection = m_registry->on_construct<TerrainTileRenderComponent>()
			.connect<&StaticMeshRenderSystem::OnTerrainTileConstruct>(this);
		m_terrainTileDestroyConnection = m_registry->on_destroy<TerrainTileRenderComponent>()
			.connect<&StaticMeshRenderSystem::OnTerrainTileDestroy>(this);
		m_hiddenConstructConnection = m_registry->on_construct<HiddenComponent>()
			.connect<&StaticMeshRenderSystem::OnHiddenConstruct>(this);
		m_hiddenDestroyConnection = m_registry->on_destroy<HiddenComponent>()
			.connect<&StaticMeshRenderSystem::OnHiddenDestroy>(this);
		m_pointLightConstructConnection = m_registry->on_construct<PointLightComponent>()
			.connect<&StaticMeshRenderSystem::OnPointLightConstruct>(this);
		m_pointLightDestroyConnection = m_registry->on_destroy<PointLightComponent>()
			.connect<&StaticMeshRenderSystem::OnPointLightDestroy>(this);
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

void StaticMeshRenderSystem::OnTerrainTileConstruct(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void StaticMeshRenderSystem::OnTerrainTileDestroy(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void StaticMeshRenderSystem::OnHiddenConstruct(entt::registry& registry, entt::entity entity)
{
	if (registry.any_of<StaticMeshRenderComponent, TerrainRenderComponent, TerrainTileRenderComponent>(entity))
	{
		m_dirty = true;
	}
}

void StaticMeshRenderSystem::OnHiddenDestroy(entt::registry& registry, entt::entity entity)
{
	if (registry.any_of<StaticMeshRenderComponent, TerrainRenderComponent, TerrainTileRenderComponent>(entity))
	{
		m_dirty = true;
	}
}

void StaticMeshRenderSystem::OnPointLightConstruct(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void StaticMeshRenderSystem::OnPointLightDestroy(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void StaticMeshRenderSystem::RebuildRenderData()
{
	m_batches.clear();
	m_terrain = nullptr;
	m_terrainEntity = entt::null;
	m_terrainTiles.clear();

	if (!m_registry)
		return;

	// Collect WLD terrain
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
				SPDLOG_DEBUG("StaticMeshRenderSystem: Found terrain entity");
				break;  // Only one terrain per zone
			}
		}
	}

	// Collect EQG terrain tiles
	{
		auto view = m_registry->view<TerrainTileRenderComponent>();
		for (auto entity : view)
		{
			if (m_registry->any_of<HiddenComponent>(entity))
				continue;

			const auto& tileComp = view.get<TerrainTileRenderComponent>(entity);
			if (tileComp.tile)
			{
				m_terrainTiles.push_back(tileComp.tile);
			}
		}
		if (!m_terrainTiles.empty())
		{
			SPDLOG_DEBUG("StaticMeshRenderSystem: Found {} terrain tiles", m_terrainTiles.size());
		}
	}

	// Collect static meshes grouped by definition
	{
		auto view = m_registry->view<StaticMeshRenderComponent, TransformComponent>();
		int meshCount = 0;
		for (auto entity : view)
		{
			if (m_registry->any_of<HiddenComponent>(entity))
				continue;

			const auto& meshComp = view.get<StaticMeshRenderComponent>(entity);

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
			++meshCount;
		}
		SPDLOG_DEBUG("StaticMeshRenderSystem: Collected {} static mesh entities in {} batches", meshCount, m_batches.size());
	}

	// Collect visible point lights for light assignment
	struct CachedLight
	{
		glm::vec3 position;
		float radius;
		glm::vec3 color;
	};
	std::vector<CachedLight> cachedLights;

	{
		auto lightView = m_registry->view<PointLightComponent, TransformComponent>();
		for (auto entity : lightView)
		{
			if (m_registry->any_of<HiddenComponent>(entity))
				continue;

			const auto& lightComp = lightView.get<PointLightComponent>(entity);
			const auto& transformComp = lightView.get<TransformComponent>(entity);

			glm::vec3 lightColor = lightComp.definition->GetColor(0);
			cachedLights.emplace_back(transformComp.position, lightComp.radius, lightColor);
		}
	}

	// Assign closest point lights to each model
	for (auto& [def, batch] : m_batches)
	{
		batch.lightAssignments.resize(batch.transforms.size());

		for (size_t i = 0; i < batch.transforms.size(); ++i)
		{
			auto& assignment = batch.lightAssignments[i];
			memset(&assignment, 0, sizeof(assignment));

			glm::vec3 modelPos = glm::vec3(batch.transforms[i][3]);

			// Find closest lights within radius
			struct LightDist { size_t index; float distSq; };
			std::vector<LightDist> candidates;

			for (size_t li = 0; li < cachedLights.size(); ++li)
			{
				float distSq = glm::dot(
					cachedLights[li].position - modelPos,
					cachedLights[li].position - modelPos);
				float radius = cachedLights[li].radius;

				if (distSq < radius * radius)
				{
					candidates.emplace_back(li, distSq);
				}
			}

			// Sort by distance, take closest
			std::ranges::sort(candidates,
				[](const LightDist& a, const LightDist& b) { return a.distSq < b.distSq; });

			int count = std::min(static_cast<int>(candidates.size()), MAX_POINT_LIGHTS);
			for (int j = 0; j < count; ++j)
			{
				const auto& light = cachedLights[candidates[j].index];
				assignment.posRadius[j] = glm::vec4(light.position, light.radius);
				assignment.color[j] = glm::vec4(light.color, 0.0f);
			}
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

	// Update materials
	auto now = eqg::world_clock::now();

	if (m_terrain)
	{
		if (eqg::MaterialPalettePtr palette = m_terrain->GetMaterialPalette())
		{
			palette->Update(now);
		}
	}

	auto view = m_registry->view<StaticMeshRenderComponent>();
	for (auto entity : view)
	{
		if (m_registry->any_of<HiddenComponent>(entity))
			continue;

		const auto& meshComp = view.get<StaticMeshRenderComponent>(entity);
		MGSimpleModel* model = meshComp.model;
		model->GetActor()->Update(now);
	}
}

void StaticMeshRenderSystem::Render()
{
	if (!m_registry)
		return;

	RenderBatchManager* batchMgr = m_renderManager->GetRenderBatchManager();

	// Disable point lights for terrain
	batchMgr->SetActivePointLights(nullptr);

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
			glm::mat4 identity(1.0f);

			// Render each material batch
			for (const MaterialBatch& matBatch : m_terrain->GetMaterialBatches())
			{
				if (matBatch.indexCount == 0)
					continue;

				batchMgr->RenderMaterialBatch(identity, matBatch,
					m_terrain->GetVertexBuffer(), m_terrain->GetIndexBuffer());
			}
		}
	}

	// Render EQG terrain tiles
	for (MGTerrainTile* tile : m_terrainTiles)
	{
		// Build GPU buffers if needed
		if (!tile->HasGPUBuffers())
		{
			tile->BuildGPUBuffers();
		}

		if (!tile->HasGPUBuffers())
			continue;

		glm::mat4 identity(1.0f);

		// Render each material batch
		for (const MaterialBatch& matBatch : tile->GetMaterialBatches())
		{
			if (matBatch.indexCount == 0)
				continue;

			batchMgr->RenderMaterialBatch(identity, matBatch,
				tile->GetVertexBuffer(), tile->GetIndexBuffer());
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

			if (m_renderManager->UsePointLightShading())
			{
				// Set cached point light assignment for this model
				const auto& assignment = batch.lightAssignments[i];
				batchMgr->SetActivePointLights(&assignment);
				batchMgr->SetPointLightShadingMode(m_renderManager->GetPointLightShadingMode());
			}
			else
			{
				batchMgr->SetActivePointLights(nullptr);
			}

			// Render each material batch
			for (const auto& matBatch : model->GetMaterialBatches())
			{
				if (matBatch.indexCount == 0)
					continue;

				batchMgr->RenderMaterialBatch(transform, matBatch,
					model->GetVertexBuffer(), model->GetIndexBuffer());
			}
		}
	}
}

