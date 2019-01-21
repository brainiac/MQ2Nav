//
// RenderHandler.cpp
//

#include "pch.h"
#include "RenderHandler.h"

#include "plugin/PluginHooks.h"
#include "plugin/MQ2Navigation.h"

#include <cassert>
#include <DetourDebugDraw.h>
#include <RecastDebugDraw.h>
#include <DebugDraw.h>

#define GLM_FORCE_RADIANS
#include <glm.hpp>

//============================================================================

RenderHandler::RenderHandler()
{
	// TODO: Fixme, i shouldn't need to be here
	if (g_deviceAcquired)
	{
		CreateDeviceObjects();
	}
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

void RenderHandler::AddRenderable(Renderable* renderable)
{
	assert(std::find(m_renderables.begin(), m_renderables.end(), renderable) == m_renderables.cend());

	m_renderables.push_back(renderable);

	if (m_deviceAcquired)
	{
		renderable->CreateDeviceObjects();
	}
}

void RenderHandler::RemoveRenderable(Renderable* renderable)
{
	auto iter = std::find(m_renderables.begin(), m_renderables.end(), renderable);
	assert(iter != m_renderables.end());

	if (m_deviceAcquired)
	{
		renderable->InvalidateDeviceObjects();
	}

	m_renderables.erase(iter);
}

void RenderHandler::PerformRender()
{
	ResetDeviceState();

	D3DPERF_BeginEvent(D3DCOLOR_XRGB(255, 0, 0), L"RenderHandler::PerformRender");

	for (auto& r : m_renderables)
	{
		r->Render();
	}

	D3DPERF_EndEvent();
}

void ResetDeviceState()
{
	g_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	g_pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, false);
	g_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, false);
	g_pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
	g_pDevice->SetRenderState(D3DRS_ZENABLE, true);
	g_pDevice->SetRenderState(D3DRS_FOGENABLE, false);
	g_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, true);
	g_pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	g_pDevice->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
	g_pDevice->SetRenderState(D3DRS_CLIPPING, true);
	g_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	g_pDevice->SetRenderState(D3DRS_DEPTHBIAS, 0);
	g_pDevice->SetRenderState(D3DRS_DITHERENABLE, false);
	g_pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	g_pDevice->SetRenderState(D3DRS_LASTPIXEL, true);
	g_pDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, true);
	g_pDevice->SetRenderState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);
	g_pDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	g_pDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
	g_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
	g_pDevice->SetRenderState(D3DRS_WRAP0, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP1, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP2, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP3, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP4, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP5, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP6, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP7, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP8, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP9, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP10, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP11, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP12, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP13, 0);
	g_pDevice->SetRenderState(D3DRS_WRAP15, 0);

	// disable the rest of the texture stages
	for (int i = 1; i < 8; i++)
	{
		g_pDevice->SetTextureStageState(i, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		g_pDevice->SetTextureStageState(i, D3DTSS_COLOROP, D3DTOP_DISABLE);
	}
	g_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	g_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);


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
