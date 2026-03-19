//
// PointLightRenderSystem.h
//

#pragma once

#include "bgfx/bgfx.h"
#include "entt/entity/registry.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "imgui/imgui.h"

#include <vector>

class ZoneRenderManager;

// Vertex data for point light billboard instances
struct PointLightInstanceVertex
{
	glm::vec4 pos;      // xyz position, w unused
	glm::vec4 color;    // rgba color from light definition
	glm::vec4 data;     // x=size, y=radius
	glm::vec4 uv;       // atlas UV region (u0, v0, u1, v1)

	PointLightInstanceVertex(const glm::vec3& position, const glm::vec4& col, float size, float radius, const glm::vec4& atlasUV)
		: pos(position.x, position.y, position.z, 1.0f)
		, color(col)
		, data(size, radius, 0, 0)
		, uv(atlasUV)
	{}

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

class PointLightRenderSystem
{
public:
	PointLightRenderSystem();
	~PointLightRenderSystem();

	void Init(ZoneRenderManager* renderManager);
	void Shutdown();

	void SetRegistry(entt::registry* registry);
	void SetDirty() { m_dirty = true; }

	void Update();
	void Render();

	bool GetVisible() const { return m_visible; }
	void SetVisible(bool visible) { m_visible = visible; }

	float GetIconSize() const { return m_iconSize; }
	void SetIconSize(float size) { m_iconSize = size; m_dirty = true; }

private:
	void RebuildBuffers();
	void DestroyBuffers();

	void OnPointLightConstruct(entt::registry& registry, entt::entity entity);
	void OnPointLightDestroy(entt::registry& registry, entt::entity entity);
	void OnHiddenConstruct(entt::registry& registry, entt::entity entity);
	void OnHiddenDestroy(entt::registry& registry, entt::entity entity);

private:
	ZoneRenderManager* m_renderManager = nullptr;
	entt::registry* m_registry = nullptr;
	bool m_visible = true;
	bool m_dirty = true;

	// Point light billboard buffers
	bgfx::VertexBufferHandle m_instanceVB = BGFX_INVALID_HANDLE;
	bgfx::ProgramHandle m_billboardProgram = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_texColorUniform = BGFX_INVALID_HANDLE;

	uint32_t m_lightCount = 0;
	float m_iconSize = 8.0f;

	// Font atlas glyph tracking
	ImFont* m_iconFont = nullptr;
	float m_iconFontSize = 20.0f;
	glm::vec4 m_cachedGlyphUV = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
	ImTextureID m_cachedAtlasTexId = ImTextureID_Invalid;

	entt::connection m_pointLightConstructConnection;
	entt::connection m_pointLightDestroyConnection;
	entt::connection m_hiddenConstructConnection;
	entt::connection m_hiddenDestroyConnection;
};
