//
// AreaVolumeRenderSystem.h
//

#pragma once

#include <bgfx/bgfx.h>
#include <entt/entity/registry.hpp>

#include <unordered_map>
#include <vector>

class ZoneRenderManager;

// Draw batch for a group of area volumes sharing the same color.
// References ranges within the unified vertex/index buffers.
struct AreaVolumeDrawBatch
{
	uint32_t color = 0;         // ABGR color
	uint32_t indexStart = 0;    // Start index in unified index buffer
	uint32_t indexCount = 0;    // Number of indices (triangles * 3)
	uint32_t edgeStart = 0;     // Start index in outline instance buffer
	uint32_t edgeCount = 0;     // Number of edge instances
};

class AreaVolumeRenderSystem
{
public:
	AreaVolumeRenderSystem();
	~AreaVolumeRenderSystem();

	void Init(ZoneRenderManager* renderManager);
	void Shutdown();

	void SetRegistry(entt::registry* registry);
	void SetDirty() { m_dirty = true; }

	void Update();
	void Render();

	float GetLineWidth() const { return m_lineWidth; }
	void SetLineWidth(float width) { m_lineWidth = width; }

	bool GetDebugColorByPlane() const { return m_debugColorByPlane; }
	void SetDebugColorByPlane(bool enabled) { m_debugColorByPlane = enabled; m_dirty = true; }

private:
	void RebuildBuffers();
	void DestroyBuffers();

	void OnAreaVolumeConstruct(entt::registry& registry, entt::entity entity);
	void OnAreaVolumeDestroy(entt::registry& registry, entt::entity entity);
	void OnHiddenConstruct(entt::registry& registry, entt::entity entity);
	void OnHiddenDestroy(entt::registry& registry, entt::entity entity);

private:
	entt::registry* m_registry = nullptr;
	ZoneRenderManager* m_renderManager = nullptr;

	// area volumes
	bgfx::VertexBufferHandle m_volumeVB = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle m_volumeIB = BGFX_INVALID_HANDLE;
	bgfx::VertexBufferHandle m_outlineVB = BGFX_INVALID_HANDLE;
	bgfx::ProgramHandle m_volumeProgram = BGFX_INVALID_HANDLE;
	bgfx::ProgramHandle m_linesProgram = BGFX_INVALID_HANDLE;

	std::vector<AreaVolumeDrawBatch> m_batches;

	std::unordered_map<entt::entity, uint32_t> m_entityToBatch;    // entity -> batch color
	std::unordered_map<entt::entity, uint32_t> m_entityColorCache; // entity -> last known fillColor

	bool m_dirty = true;
	float m_lineWidth = 2.0f;
	bool m_debugColorByPlane = false;

	entt::connection m_areaVolumeConstructConnection;
	entt::connection m_areaVolumeDestroyConnection;
	entt::connection m_hiddenConstructConnection;
	entt::connection m_hiddenDestroyConnection;
};
