//
// RenderBatchManager.h
//

#pragma once

class ZoneRenderManager;
struct MaterialBatch;

class RenderBatchManager
{
public:
	RenderBatchManager(ZoneRenderManager* renderManager);
	~RenderBatchManager();

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
