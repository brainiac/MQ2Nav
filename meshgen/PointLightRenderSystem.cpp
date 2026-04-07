//
// PointLightRenderSystem.cpp
//

#include "pch.h"
#include "meshgen/PointLightRenderSystem.h"

#include "meshgen/Application.h"
#include "meshgen/Components.h"
#include "meshgen/EQComponents.h"
#include "meshgen/ResourceManager.h"
#include "meshgen/ZoneRenderManager.h"
#include "meshgen/imgui/imgui_impl_bgfx.h"

#include "eqglib/eqg_light.h"

#include "spdlog/spdlog.h"

//============================================================================

PointLightRenderSystem::PointLightRenderSystem()
{
}

PointLightRenderSystem::~PointLightRenderSystem()
{
	Shutdown();
}

void PointLightRenderSystem::Init(ZoneRenderManager* renderManager)
{
	m_renderManager = renderManager;

	m_billboardProgram = g_resourceMgr->GetProgramHandle("billboard");
	m_texColorUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
	m_iconFont = LCIconFont;

	PointLightInstanceVertex::Init();
}

void PointLightRenderSystem::SetRegistry(entt::registry* registry)
{
	if (m_registry)
	{
		m_pointLightConstructConnection.release();
		m_pointLightDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();
	}

	m_registry = registry;

	if (m_registry)
	{
		m_pointLightConstructConnection = m_registry->on_construct<PointLightComponent>()
			.connect<&PointLightRenderSystem::OnPointLightConstruct>(this);
		m_pointLightDestroyConnection = m_registry->on_destroy<PointLightComponent>()
			.connect<&PointLightRenderSystem::OnPointLightDestroy>(this);
		m_hiddenConstructConnection = m_registry->on_construct<HiddenComponent>()
			.connect<&PointLightRenderSystem::OnHiddenConstruct>(this);
		m_hiddenDestroyConnection = m_registry->on_destroy<HiddenComponent>()
			.connect<&PointLightRenderSystem::OnHiddenDestroy>(this);
	}

	m_dirty = true;
}

void PointLightRenderSystem::OnPointLightConstruct(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void PointLightRenderSystem::OnPointLightDestroy(entt::registry& registry, entt::entity entity)
{
	m_dirty = true;
}

void PointLightRenderSystem::OnHiddenConstruct(entt::registry& registry, entt::entity entity)
{
	if (registry.any_of<PointLightComponent>(entity))
		m_dirty = true;
}

void PointLightRenderSystem::OnHiddenDestroy(entt::registry& registry, entt::entity entity)
{
	if (registry.any_of<PointLightComponent>(entity))
		m_dirty = true;
}

void PointLightRenderSystem::Shutdown()
{
	if (m_registry)
	{
		m_pointLightConstructConnection.release();
		m_pointLightDestroyConnection.release();
		m_hiddenConstructConnection.release();
		m_hiddenDestroyConnection.release();
	}

	if (bgfx::isValid(m_texColorUniform))
	{
		bgfx::destroy(m_texColorUniform);
		m_texColorUniform = BGFX_INVALID_HANDLE;
	}

	DestroyBuffers();
	m_registry = nullptr;
	m_renderManager = nullptr;
}

void PointLightRenderSystem::DestroyBuffers()
{
	if (bgfx::isValid(m_instanceVB))
	{
		bgfx::destroy(m_instanceVB);
		m_instanceVB = BGFX_INVALID_HANDLE;
	}

	m_lightCount = 0;
}

void PointLightRenderSystem::RebuildBuffers()
{
	DestroyBuffers();

	if (!m_registry)
		return;

	std::vector<PointLightInstanceVertex> instances;

	auto view = m_registry->view<PointLightComponent, TransformComponent>();

	for (auto entity : view)
	{
		if (m_registry->any_of<HiddenComponent>(entity))
			continue;

		const auto& lightComp = view.get<PointLightComponent>(entity);
		const auto& transformComp = view.get<TransformComponent>(entity);

		// Get light color from definition (frame 0)
		glm::vec3 lightColor = lightComp.definition->GetColor(0);
		float intensity = lightComp.definition->GetIntensity(0);
		glm::vec4 color = glm::vec4(lightColor.xyz * intensity, 1.0f);

		instances.emplace_back(transformComp.position, color, m_iconSize, lightComp.radius, m_cachedGlyphUV);
	}

	if (instances.empty())
	{
		m_dirty = false;
		return;
	}

	m_instanceVB = bgfx::createVertexBuffer(
		bgfx::copy(instances.data(), static_cast<uint32_t>(instances.size() * sizeof(PointLightInstanceVertex))),
		PointLightInstanceVertex::ms_layout);

	m_lightCount = static_cast<uint32_t>(instances.size());

	SPDLOG_DEBUG("PointLightRenderSystem: Built {} point light instances", m_lightCount);

	m_dirty = false;
}

void PointLightRenderSystem::Update()
{
	// Check every frame whether the ImGui font atlas has changed.
	// If the glyph UVs or the texture handle have shifted, mark dirty so instance
	// buffers get rebuilt with the correct coordinates.
	if (m_iconFont)
	{
		ImFontBaked* baked = m_iconFont->GetFontBaked(m_iconFontSize);
		const ImFontGlyph* glyph = baked ? baked->FindGlyphNoFallback(0xe17b) : nullptr;

		glm::vec4 newUV = glyph
			? glm::vec4(glyph->U0, glyph->V0, glyph->U1, glyph->V1)
			: glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
		ImTextureID newTexId = m_iconFont->OwnerAtlas->TexRef.GetTexID();

		if (newUV != m_cachedGlyphUV || newTexId != m_cachedAtlasTexId)
		{
			m_cachedGlyphUV = newUV;
			m_cachedAtlasTexId = newTexId;
			m_dirty = true;
		}
	}

	if (m_dirty)
	{
		RebuildBuffers();
	}
}

void PointLightRenderSystem::Render()
{
	if (!m_visible)
		return;

	if (!bgfx::isValid(m_instanceVB) || m_lightCount == 0)
		return;

	if (!bgfx::isValid(m_billboardProgram))
		return;

	bgfx::TextureHandle atlasHandle = bx::bitCast<ImGui::TextureBgfx>(m_cachedAtlasTexId).handle;
	if (!bgfx::isValid(atlasHandle))
		return;

	bgfx::Encoder* encoder = bgfx::begin();

	encoder->setVertexBuffer(0, g_zoneRenderManager->GetShared()->m_quad2VB);
	encoder->setIndexBuffer(g_zoneRenderManager->GetShared()->m_quadIB);
	encoder->setInstanceDataBuffer(m_instanceVB, 0, m_lightCount);
	encoder->setTexture(0, m_texColorUniform, atlasHandle);
	encoder->setState(BGFX_STATE_MSAA | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
		BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_BLEND_ALPHA);

	encoder->submit(0, m_billboardProgram);
}
