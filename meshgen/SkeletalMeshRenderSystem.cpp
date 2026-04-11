//
// SkeletalMeshRenderSystem.cpp
//

#include "pch.h"
#include "SkeletalMeshRenderSystem.h"

#include "meshgen/Components.h"
#include "meshgen/EQComponents.h"
#include "meshgen/Entity.h"
#include "meshgen/MGHierarchicalModel.h"
#include "meshgen/ZoneRenderManager.h"

#include "eqglib/eqg_geometry.h"
#include "eqglib/eqg_material.h"

#include "entt/entity/handle.hpp"
#include "spdlog/spdlog.h"

//============================================================================

SkeletalMeshRenderSystem::SkeletalMeshRenderSystem()
{
}

SkeletalMeshRenderSystem::~SkeletalMeshRenderSystem()
{
	Shutdown();
}

void SkeletalMeshRenderSystem::Init(ZoneRenderManager* renderManager)
{
	m_renderManager = renderManager;

	m_batchRenderer.Init(renderManager);
}

void SkeletalMeshRenderSystem::Shutdown()
{
	if (m_registry)
	{
		m_skeletalMeshConstructConnection.release();
		m_skeletalMeshDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();
	}

	m_batchRenderer.Shutdown();

	m_batches.clear();
	m_registry = nullptr;
	m_renderManager = nullptr;
}

void SkeletalMeshRenderSystem::SetRegistry(entt::registry* registry)
{
	if (m_registry)
	{
		m_skeletalMeshConstructConnection.release();
		m_skeletalMeshDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();

		m_batches.clear();
	}

	m_registry = registry;

	if (m_registry)
	{
		m_skeletalMeshConstructConnection = m_registry->on_construct<SkeletalMeshRenderComponent>()
			.connect<&SkeletalMeshRenderSystem::OnSkeletalMeshConstruct>(this);
		m_skeletalMeshDestroyConnection = m_registry->on_destroy<SkeletalMeshRenderComponent>()
			.connect<&SkeletalMeshRenderSystem::OnSkeletalMeshDestroy>(this);
		m_hiddenConstructConnection = m_registry->on_construct<HiddenComponent>()
			.connect<&SkeletalMeshRenderSystem::OnHiddenConstruct>(this);
		m_hiddenDestroyConnection = m_registry->on_destroy<HiddenComponent>()
			.connect<&SkeletalMeshRenderSystem::OnHiddenDestroy>(this);
	}

	m_dirty = true;
}

void SkeletalMeshRenderSystem::OnSkeletalMeshConstruct(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void SkeletalMeshRenderSystem::OnSkeletalMeshDestroy(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void SkeletalMeshRenderSystem::OnHiddenConstruct(entt::registry& registry, entt::entity entity)
{
	if (registry.any_of<SkeletalMeshRenderComponent>(entity))
	{
		m_dirty = true;
	}
}

void SkeletalMeshRenderSystem::OnHiddenDestroy(entt::registry& registry, entt::entity entity)
{
	if (registry.any_of<SkeletalMeshRenderComponent>(entity))
	{
		m_dirty = true;
	}
}

void SkeletalMeshRenderSystem::RebuildRenderData()
{
	m_batches.clear();

	if (!m_registry)
		return;

	// Collect skeletal meshes grouped by definition
	auto view = m_registry->view<SkeletalMeshRenderComponent, TransformComponent>();
	int meshCount = 0;
	for (auto entity : view)
	{
		if (m_registry->any_of<HiddenComponent>(entity))
			continue;

		const auto& meshComp = view.get<SkeletalMeshRenderComponent>(entity);

		if (!meshComp.model || !meshComp.model->GetDefinition())
			continue;

		eqg::HierarchicalModelDefinition* def = meshComp.model->GetDefinition().get();

		// Get world transform
		entt::handle handle{ *m_registry, entity };
		glm::mat4 worldMatrix = GetWorldSpaceTransformMatrix(handle);

		auto& batch = m_batches[def];
		batch.definition = def;
		batch.transforms.push_back(worldMatrix);
		batch.models.push_back(meshComp.model);
		++meshCount;
	}

	SPDLOG_DEBUG("SkeletalMeshRenderSystem: Collected {} skeletal mesh entities in {} batches", meshCount, m_batches.size());

	m_dirty = false;
}

void SkeletalMeshRenderSystem::Update()
{
	if (!m_registry)
		return;

	if (m_dirty)
	{
		RebuildRenderData();
	}

	// Update materials
	auto now = eqg::world_clock::now();

	auto view = m_registry->view<SkeletalMeshRenderComponent>();
	for (auto entity : view)
	{
		if (m_registry->any_of<HiddenComponent>(entity))
			continue;

		const auto& meshComp = view.get<SkeletalMeshRenderComponent>(entity);
		MGHierarchicalModel* model = meshComp.model;
		model->GetActor()->Update(now);
	}
}

void SkeletalMeshRenderSystem::Render()
{
	if (!m_registry)
		return;

	if (!m_batchRenderer.IsValid())
		return;

	for (auto& [def, batch] : m_batches)
	{
		for (size_t i = 0; i < batch.models.size(); ++i)
		{
			MGHierarchicalModel* model = batch.models[i];
			const glm::mat4& transform = batch.transforms[i];

			// Build GPU buffers if needed (one-time CPU skinning)
			if (!model->HasGPUBuffers())
			{
				model->BuildGPUBuffers();
			}

			if (!model->HasGPUBuffers())
				continue;

			// Render each material batch
			for (const auto& matBatch : model->GetMaterialBatches())
			{
				if (matBatch.indexCount == 0)
					continue;

				m_batchRenderer.RenderMaterialBatch(transform, matBatch,
					model->GetVertexBuffer(), model->GetIndexBuffer());
			}
		}
	}
}
