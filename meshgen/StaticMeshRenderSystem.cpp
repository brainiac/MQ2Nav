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
#include "meshgen/ResourceManager.h"
#include "meshgen/ZoneRenderManager.h"

#include "eqglib/eqg_geometry.h"
#include "eqglib/eqg_material.h"

#include "entt/entity/handle.hpp"
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

	// Create uniforms
	m_uniformUseVertexColors = bgfx::createUniform("u_useVertexColors", bgfx::UniformType::Vec4);
	m_texColorSampler = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
	m_uniformTextureFlags = bgfx::createUniform("u_textureFlags", bgfx::UniformType::Vec4);
	m_uniformGlobalAmbient = bgfx::createUniform("u_globalAmbient", bgfx::UniformType::Vec4);

	// Create 1x1 white fallback texture
	uint32_t whitePixel = 0xFFFFFFFF;
	m_whiteTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
		BGFX_TEXTURE_NONE, bgfx::copy(&whitePixel, 4));

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
		m_terrainTileConstructConnection.release();
		m_terrainTileDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();
	}

	if (bgfx::isValid(m_uniformUseVertexColors))
	{
		bgfx::destroy(m_uniformUseVertexColors);
		m_uniformUseVertexColors = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_texColorSampler))
	{
		bgfx::destroy(m_texColorSampler);
		m_texColorSampler = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_uniformTextureFlags))
	{
		bgfx::destroy(m_uniformTextureFlags);
		m_uniformTextureFlags = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_uniformGlobalAmbient))
	{
		bgfx::destroy(m_uniformGlobalAmbient);
		m_uniformGlobalAmbient = BGFX_INVALID_HANDLE;

	}

	if (bgfx::isValid(m_whiteTexture))
	{
		bgfx::destroy(m_whiteTexture);
		m_whiteTexture = BGFX_INVALID_HANDLE;
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

void StaticMeshRenderSystem::RenderMaterialBatch(const glm::mat4& worldMtx, const MaterialBatch& batch,
	bgfx::VertexBufferHandle vertexBuffer, bgfx::IndexBufferHandle indexBuffer)
{
	if (batch.indexCount == 0)
		return;

	bgfx::Encoder* encoder = bgfx::begin();
	encoder->setTransform(glm::value_ptr(worldMtx));

	// Bind textures
	bgfx::TextureHandle texHandle = BGFX_INVALID_HANDLE;
	bool hasTexture = false;

	// Note: we need special handling for single detail using alt texture...
	if (batch.material)
	{
		if (eqg::STextureSet* textureSet = batch.material->GetTextureSet())
		{
			// This would be where we want to "lazy load" textures...

			eqg::STexture* texture = &textureSet->textures[textureSet->currentFrame];

			// Currently only supporting the default texture, but this is where we'd have
			// other textures (bump map, etc) handled as well.
			MGBitmap* bitmap = static_cast<MGBitmap*>(texture->textures[0].get());

			texHandle = bitmap->GetTextureHandle();
			hasTexture = bgfx::isValid(texHandle);
		}
	}

	encoder->setTexture(0, m_texColorSampler, hasTexture ? texHandle : m_whiteTexture);

	bool showInvisible = m_renderManager->GetDrawInvisibleWalls();
	bool isTransparent = true;
	bool isChroma = false;
	bool isAlpha = false;
	bool isAlphaAdditive = false;
	bool isDepthWrite = true;
	bool isChromaHigh = false;

	if (batch.material)
	{
		isTransparent = batch.material->IsTransparent();
		isChroma = batch.material->IsChroma();
		isAlpha = batch.material->IsAlphaBlend();
		isAlphaAdditive = batch.material->IsAdditiveAlpha();
		isDepthWrite = batch.material->IsDepthWrite();
		isChromaHigh = batch.material->IsChromaHigh();
	}

	// todo: make this configurable
	glm::vec4 globalAmbient = { 0.5f, 0.5f, 0.5f, 1.0f };

	glm::vec4 uTextureFlags(
		hasTexture ? 1.0f : 0.0f,
		showInvisible ? 1.0f : 0.0f,
		isTransparent ? 1.0f : 0.0f,
		isChromaHigh ? 0.75294117f : isChroma ? 0.0627450980392157 : 0.0f
	);

	// Set the vertex colors uniform value
	glm::vec4 useVertexColors(
		m_useVertexColors ? 1.0f : 0.0f,  // 1.0 = modulate by vertex color, 0.0 = modulate by 1.0f
		batch.material ? static_cast<float>(batch.material->m_alpha) / 255.0f : 1.0f, // use material alpha
		0.0f,
		0.0f
	);

	encoder->setUniform(m_uniformGlobalAmbient, glm::value_ptr(globalAmbient));
	encoder->setUniform(m_uniformTextureFlags, glm::value_ptr(uTextureFlags));
	encoder->setUniform(m_uniformUseVertexColors, glm::value_ptr(useVertexColors));

	if (bgfx::isValid(vertexBuffer))
		encoder->setVertexBuffer(0, vertexBuffer);
	if (bgfx::isValid(indexBuffer))
		encoder->setIndexBuffer(indexBuffer, batch.startIndex, batch.indexCount);

	uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
		BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW;

	if (isAlpha)
	{
		state |= BGFX_STATE_BLEND_ALPHA;
	}

	if (isAlphaAdditive)
	{
		state |= BGFX_STATE_BLEND_ADD;
	}

	if (isDepthWrite)
	{
		state |= BGFX_STATE_WRITE_Z;
	}

	if (isChroma)
	{
		if (isChromaHigh)
			state |= BGFX_STATE_ALPHA_REF(0xc0);
		else
			state |= BGFX_STATE_ALPHA_REF(0x10);
	}

	encoder->setState(state);
	encoder->submit(0, m_program);
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
			glm::mat4 identity(1.0f);

			// Render each material batch
			for (const MaterialBatch& matBatch : m_terrain->GetMaterialBatches())
			{
				if (matBatch.indexCount == 0)
					continue;

				RenderMaterialBatch(identity, matBatch,
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

			RenderMaterialBatch(identity, matBatch,
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

			// Render each material batch
			for (const auto& matBatch : model->GetMaterialBatches())
			{
				if (matBatch.indexCount == 0)
					continue;

				RenderMaterialBatch(transform, matBatch,
					model->GetVertexBuffer(), model->GetIndexBuffer());
			}
		}
	}
}

