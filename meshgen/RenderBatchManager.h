//
// RenderBatchManager.h
//

#pragma once

#include <glm/vec4.hpp>

class ZoneRenderManager;
struct MaterialBatch;

static constexpr int MAX_POINT_LIGHTS = 3;

struct ActivePointLights
{
	glm::vec4 posRadius[MAX_POINT_LIGHTS];        // xyz = world position, w = radius
	glm::vec4 colorIntensity[MAX_POINT_LIGHTS];   // rgb = color * intensity
};

class RenderBatchManager
{
public:
	RenderBatchManager(ZoneRenderManager* renderManager);
	~RenderBatchManager();

	void RenderMaterialBatch(const glm::mat4& worldMtx, const MaterialBatch& batch,
		bgfx::VertexBufferHandle vertexBuffer, bgfx::IndexBufferHandle indexBuffer);

	void SetActivePointLights(const ActivePointLights* lights);

	bgfx::ProgramHandle GetProgram() const { return m_program; }
	bool IsValid() const { return bgfx::isValid(m_program); }

private:
	ZoneRenderManager* m_renderManager = nullptr;

	bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uniformUseVertexColors = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_texColorSampler = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uniformTextureFlags = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uniformGlobalAmbient = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uDirectionalLightColor = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uDirectionalLightNormal = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle m_whiteTexture = BGFX_INVALID_HANDLE;

	// Point light uniforms
	bgfx::UniformHandle m_uPointLightPosRadius = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uPointLightColorIntensity = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uPointLightParams = BGFX_INVALID_HANDLE;

	// Current active point lights (set per-model before rendering)
	ActivePointLights m_activePointLights;
};
