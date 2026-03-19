//
// InvisibleWallRenderSystem.h
//

#pragma once

#include "bgfx/bgfx.h"
#include "entt/entity/registry.hpp"
#include "glm/vec3.hpp"

#include <vector>

class ZoneRenderManager;

class InvisibleWallRenderSystem
{
public:
	InvisibleWallRenderSystem();
	~InvisibleWallRenderSystem();

	void Init(ZoneRenderManager* renderManager);
	void Shutdown();

	void SetRegistry(entt::registry* registry);
	void SetDirty() { m_dirty = true; }

	void Update();
	void Render();

	bool GetVisible() const { return m_visible; }
	void SetVisible(bool visible) { m_visible = visible; }

	float GetLineWidth() const { return m_lineWidth; }
	void SetLineWidth(float width) { m_lineWidth = width; m_dirty = true; }

private:
	void RebuildBuffers();
	void DestroyBuffers();

	void OnInvisibleWallConstruct(entt::registry& registry, entt::entity entity);
	void OnInvisibleWallDestroy(entt::registry& registry, entt::entity entity);
	void OnHiddenConstruct(entt::registry& registry, entt::entity entity);
	void OnHiddenDestroy(entt::registry& registry, entt::entity entity);

private:
	ZoneRenderManager* m_renderManager = nullptr;
	entt::registry* m_registry = nullptr;
	bool m_visible = false;
	bool m_dirty = true;

	// Invisible wall buffers
	bgfx::VertexBufferHandle m_volumeVB = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle m_volumeIB = BGFX_INVALID_HANDLE;
	bgfx::VertexBufferHandle m_outlineVB = BGFX_INVALID_HANDLE;
	bgfx::ProgramHandle m_volumeProgram = BGFX_INVALID_HANDLE;
	bgfx::ProgramHandle m_linesProgram = BGFX_INVALID_HANDLE;

	uint32_t m_indexCount = 0;
	uint32_t m_edgeCount = 0;

	float m_lineWidth = 2.0f;

	entt::connection m_invisibleWallConstructConnection;
	entt::connection m_invisibleWallDestroyConnection;
	entt::connection m_hiddenConstructConnection;
	entt::connection m_hiddenDestroyConnection;
};
