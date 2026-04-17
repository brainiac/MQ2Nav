//
// RenderBatchManager.cpp
//

#include "pch.h"
#include "RenderBatchManager.h"

#include "meshgen/RenderBatch.h"
#include "meshgen/ResourceManager.h"
#include "meshgen/MGBitmap.h"
#include "meshgen/ZoneRenderManager.h"

#include "eqglib/eqg_material.h"

#include "glm/gtc/type_ptr.hpp"

glm::vec4 g_globalAmbient = { 0.5f, 0.5f, 0.5f, 1.0f };

glm::vec4 g_directionalLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
glm::vec4 g_directionalLightNormal = { 0, -0.707f, -0.707f, 1.0f };

RenderBatchManager::RenderBatchManager(ZoneRenderManager* renderManager)
{
	m_renderManager = renderManager;

	// Load shader program
	m_program = g_resourceMgr->GetProgramHandle("staticmesh");

	// Create uniforms
	m_texColorSampler = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

	m_uShadingMode = bgfx::createUniform("u_shadingMode", bgfx::UniformType::Vec4);
	m_uTextureFlags = bgfx::createUniform("u_textureFlags", bgfx::UniformType::Vec4);
	m_uGlobalAmbient = bgfx::createUniform("u_globalAmbient", bgfx::UniformType::Vec4);
	m_uDirectionalLightColor = bgfx::createUniform("u_directionalLightColor", bgfx::UniformType::Vec4);
	m_uDirectionalLightNormal = bgfx::createUniform("u_directionalLightNormal", bgfx::UniformType::Vec4);
	m_uPointLightPosRadius = bgfx::createUniform("u_pointLightPosRadius", bgfx::UniformType::Vec4, MAX_POINT_LIGHTS);
	m_uPointLightColor = bgfx::createUniform("u_pointLightColor", bgfx::UniformType::Vec4, MAX_POINT_LIGHTS);

	// Create 1x1 white fallback texture
	uint32_t whitePixel = 0xFFFFFFFF;
	m_whiteTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
		BGFX_TEXTURE_NONE, bgfx::copy(&whitePixel, 4));

	// Initialize vertex layouts
	StaticMeshVertex::Init();
}

RenderBatchManager::~RenderBatchManager()
{
	if (bgfx::isValid(m_uShadingMode))
	{
		bgfx::destroy(m_uShadingMode);
		m_uShadingMode = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_texColorSampler))
	{
		bgfx::destroy(m_texColorSampler);
		m_texColorSampler = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_uTextureFlags))
	{
		bgfx::destroy(m_uTextureFlags);
		m_uTextureFlags = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_uGlobalAmbient))
	{
		bgfx::destroy(m_uGlobalAmbient);
		m_uGlobalAmbient = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_uDirectionalLightColor))
	{
		bgfx::destroy(m_uDirectionalLightColor);
		m_uDirectionalLightColor = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_uDirectionalLightNormal))
	{
		bgfx::destroy(m_uDirectionalLightNormal);
		m_uDirectionalLightNormal = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_uPointLightPosRadius))
	{
		bgfx::destroy(m_uPointLightPosRadius);
		m_uPointLightPosRadius = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_uPointLightColor))
	{
		bgfx::destroy(m_uPointLightColor);
		m_uPointLightColor = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(m_whiteTexture))
	{
		bgfx::destroy(m_whiteTexture);
		m_whiteTexture = BGFX_INVALID_HANDLE;
	}

	m_renderManager = nullptr;
}

void RenderBatchManager::SetActivePointLights(const ActivePointLights* lights)
{
	if (lights != nullptr)
	{
		memcpy(&m_activePointLights, lights, sizeof(ActivePointLights));
	}
	else
	{
		memset(&m_activePointLights, 0, sizeof(m_activePointLights));
	}
}

void RenderBatchManager::RenderMaterialBatch(const glm::mat4& worldMtx, const MaterialBatch& batch, bgfx::VertexBufferHandle vertexBuffer, bgfx::IndexBufferHandle indexBuffer)
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
			if (MGBitmap* bitmap = static_cast<MGBitmap*>(texture->textures[0].get()))
			{
				texHandle = bitmap->GetTextureHandle();
			}
			else
			{
				texHandle = m_whiteTexture;
			}

			hasTexture = bgfx::isValid(texHandle);
		}
	}

	encoder->setTexture(0, m_texColorSampler, hasTexture ? texHandle : m_whiteTexture);

	bool useVertexColors = m_renderManager->GetUseVertexColors();
	bool useVertexTints = m_renderManager->GetUseVertexTints();

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

	glm::vec4 uTextureFlags(
		hasTexture ? 1.0f : 0.0f,
		showInvisible ? 1.0f : 0.0f,
		isTransparent ? 1.0f : 0.0f,
		isChromaHigh ? 0.75294117f : isChroma ? 0.0627450980392157 : 0.0f
	);

	// Set the vertex colors uniform value
	glm::vec4 uShadingMode(
		useVertexColors ? 1.0f : 0.0f,  // 1.0 = modulate by vertex color, 0.0 = modulate by 1.0f
		batch.material ? static_cast<float>(batch.material->m_alpha) / 255.0f : 1.0f, // use material alpha
		useVertexTints && batch.isTint ? 1.0f : 0.0f,
		m_activePointLights.posRadius[0].w < 0.001f ? 0 : (static_cast<float>(m_pointLightShadingMode) + 1.0f)
	);

	encoder->setUniform(m_uGlobalAmbient, glm::value_ptr(g_globalAmbient));
	encoder->setUniform(m_uTextureFlags, glm::value_ptr(uTextureFlags));
	encoder->setUniform(m_uShadingMode, glm::value_ptr(uShadingMode));
	encoder->setUniform(m_uDirectionalLightColor, glm::value_ptr(g_directionalLightColor));
	encoder->setUniform(m_uDirectionalLightNormal, glm::value_ptr(g_directionalLightNormal));

	// Set point light uniforms
	encoder->setUniform(m_uPointLightPosRadius, m_activePointLights.posRadius, MAX_POINT_LIGHTS);
	encoder->setUniform(m_uPointLightColor, m_activePointLights.color, MAX_POINT_LIGHTS);

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
