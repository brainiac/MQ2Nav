//
// RenderHandler.cpp
//

#include "RenderHandler.h"
#include "MQ2Nav_Hooks.h"
#include "MQ2Navigation.h"

#include "ImGuiDX9.h"

#include <cassert>
#include <DetourDebugDraw.h>
#include <RecastDebugDraw.h>
#include <DebugDraw.h>

#define GLM_FORCE_RADIANS
#include <glm.hpp>

//============================================================================

std::shared_ptr<RenderHandler> g_renderHandler;

RenderHandler::RenderHandler()
{
	CreateDeviceObjects();
}

RenderHandler::~RenderHandler()
{
	InvalidateDeviceObjects();
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

void RenderHandler::CreateDeviceObjects()
{
	if (m_deviceAcquired)
		return;
	if (!g_pDevice)
		return;

	for (auto& p : m_renderables)
	{
		p->CreateDeviceObjects();
	}
	m_deviceAcquired = true;
}

void RenderHandler::InvalidateDeviceObjects()
{
	if (!m_deviceAcquired)
		return;
	m_deviceAcquired = false;

	for (auto& p : m_renderables)
	{
		p->InvalidateDeviceObjects();
	}
}

void RenderHandler::AddRenderable(const std::shared_ptr<Renderable>& renderable)
{
	assert(std::find(m_renderables.begin(), m_renderables.end(), renderable) == m_renderables.cend());

	m_renderables.push_back(renderable);

	if (m_deviceAcquired)
	{
		renderable->CreateDeviceObjects();
	}
}

void RenderHandler::RemoveRenderable(const std::shared_ptr<Renderable>& renderable)
{
	assert(std::find(m_renderables.begin(), m_renderables.end(), renderable) != m_renderables.end());

	if (m_deviceAcquired)
	{
		renderable->InvalidateDeviceObjects();
	}

	m_renderables.erase(std::remove(m_renderables.begin(), m_renderables.end(), renderable),
		m_renderables.end());
}

void RenderHandler::PerformRender(Renderable::RenderPhase phase)
{
	for (auto& r : m_renderables)
	{
		r->Render(phase);
	}
}

void ResetDeviceState()
{
	g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
	g_pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	g_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, true);
	g_pDevice->SetRenderState(D3DRS_ALPHAREF, 0);
	g_pDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);

	g_pDevice->SetRenderState(D3DRS_ZENABLE, true);
	g_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, true);

	g_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
	g_pDevice->SetRenderState(D3DRS_LIGHTING, false);
	g_pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, true);

	g_pDevice->SetRenderState(D3DRS_STENCILENABLE, false);

	// disable the rest of the texture stages
	for (int i = 0; i < 8; i++)
	{
		g_pDevice->SetTextureStageState(i, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		g_pDevice->SetTextureStageState(i, D3DTSS_COLOROP, D3DTOP_DISABLE);
	}

	D3DXMATRIX matrix;
	D3DXMatrixIdentity(&matrix);

	g_pDevice->SetTransform(D3DTS_WORLD, &matrix);
}
