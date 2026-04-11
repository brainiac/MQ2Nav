//
// MaterialBatchRenderer.h
//
// Shared utility for rendering material batches with the staticmesh shader.
// Used by both StaticMeshRenderSystem and SkeletalMeshRenderSystem.
//

#pragma once

#include "bgfx/bgfx.h"
#include "glm/glm.hpp"

struct MaterialBatch;
class ZoneRenderManager;

// Manages shared GPU resources for rendering material batches.
// Owns the shader program, uniforms, and fallback texture.
class MaterialBatchRenderer
{
public:
	MaterialBatchRenderer();
	~MaterialBatchRenderer();

	void Init(ZoneRenderManager* renderManager);
	void Shutdown();

	void RenderMaterialBatch(const glm::mat4& worldMtx, const MaterialBatch& batch,
		bgfx::VertexBufferHandle vertexBuffer, bgfx::IndexBufferHandle indexBuffer);

	bgfx::ProgramHandle GetProgram() const { return m_program; }
	bool IsValid() const { return bgfx::isValid(m_program); }

private:
	ZoneRenderManager* m_renderManager = nullptr;

	bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uniformUseVertexColors = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_texColorSampler = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uniformTextureFlags = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_uniformGlobalAmbient = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle m_whiteTexture = BGFX_INVALID_HANDLE;
};
