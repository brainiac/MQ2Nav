//
// RenderHandler.cpp
//

#include "RenderHandler.h"
#include "MQ2Nav_Hooks.h"
#include "MQ2Navigation.h"

#include "FindPattern.h"
#include "imgui_impl_dx9.h"

#include <cassert>
#include <DetourDebugDraw.h>
#include <RecastDebugDraw.h>
#include <DebugDraw.h>

#define GLM_FORCE_RADIANS
#include <glm.hpp>

//============================================================================

/*
class DebugDrawDX : public duDebugDraw
{
	RenderList& m_rl;

public:
	DebugDrawDX(RenderList& rl_)
		: m_rl(rl_)
	{
	}

	virtual void end() override
	{
		m_rl.End();
	}
	
	virtual void depthMask(bool state) override
	{
		//m_pDevice->SetRenderState(D3DRS_ZENABLE, state ? D3DZB_TRUE : D3DZB_FALSE);
	}

	virtual void texture(bool state) override
	{
	}

	virtual void begin(duDebugDrawPrimitives prim, float size = 1.0f) override
	{
		RenderList::PrimitiveType type = RenderList::Prim_Points;

		switch (prim)
		{
		case DU_DRAW_POINTS: type = RenderList::Prim_Points; break;
		case DU_DRAW_LINES: type = RenderList::Prim_Lines; break;
		case DU_DRAW_TRIS: type = RenderList::Prim_Triangles; break;
		case DU_DRAW_QUADS: type = RenderList::Prim_Quads; break;
		}

		m_rl.Begin(type, size);
	}

	virtual void vertex(const float* pos, unsigned int color) override
	{
		vertex(pos[0], pos[1], pos[2], color, 0.0f, 0.0f);
	}

	virtual void vertex(const float x, const float y, const float z, unsigned int color) override
	{
		vertex(x, y, z, color, 0.0f, 0.0f);
	}

	virtual void vertex(const float* pos, unsigned int color, const float* uv) override
	{
		vertex(pos[0], pos[1], pos[2], color, uv[0], uv[1]);
	}

	virtual void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v) override
	{
		m_rl.AddVertex(x, y, z, color, u, v);
	}
};
*/
//----------------------------------------------------------------------------

std::shared_ptr<RenderHandler> g_renderHandler;


RenderHandler::RenderHandler()
	//: m_navMeshConnection(g_pl)
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

	D3DXMatrixIdentity(&m_worldMatrix);
	D3DXMatrixIdentity(&m_viewMatrix);
	D3DXMatrixIdentity(&m_projMatrix);

	ZeroMemory(&m_navPathMaterial, sizeof(D3DMATERIAL9));
	m_navPathMaterial.Diffuse.r = 1.0f;
	m_navPathMaterial.Diffuse.g = 1.0f;
	m_navPathMaterial.Diffuse.b = 1.0f;
	m_navPathMaterial.Diffuse.a = 1.0f;

	UpdateNavigationPath();
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
	m_renderables.clear();

	// should be extract from this class
	if (m_pVertexBuffer)
	{
		m_pVertexBuffer->Release();
		m_pVertexBuffer = nullptr;
	}
	if (m_pTexture)
	{
		m_pTexture->Release();
		m_pTexture = nullptr;
	}

	ClearNavigationPath();
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

	if (phase == Renderable::Render_Geometry)
	{
		Render3D();
	}
}

void RenderHandler::Render3D()
{
	// move these out
	g_pDevice->GetTransform(D3DTS_WORLD, &m_worldMatrix);
	g_pDevice->GetTransform(D3DTS_PROJECTION, &m_projMatrix);
	g_pDevice->GetTransform(D3DTS_VIEW, &m_viewMatrix);

	UpdateNavigationPath();

	if (m_pLines)
	{
		g_pDevice->SetMaterial(&m_navPathMaterial);
		g_pDevice->SetStreamSource(0, m_pLines, 0, sizeof(LineVertex));
		g_pDevice->SetFVF(LineVertex::FVF_Flags);

		g_pDevice->DrawPrimitive(D3DPT_LINESTRIP, 0, m_navpathLen - 1);
	}
}

void RenderHandler::SetNavigationPath(MQ2NavigationPath* navPath)
{
	ClearNavigationPath();

	m_navPath = navPath;

	UpdateNavigationPath();
}

void RenderHandler::UpdateNavigationPath()
{
	if (m_navPath == nullptr)
		return;

	int newLength = m_navPath->GetPathSize() + 1;
	const float* navPathPoints = m_navPath->GetCurrentPath();

	if (newLength != m_navpathLen)
	{
		// changed length, need to create new buffer?
		m_navpathLen = newLength;

		if (m_pLines)
		{
			m_pLines->Release();
			m_pLines = nullptr;
		}

		// nothing to render if there is no path
		if (m_navpathLen == 1)
			return;
	}

	PSPAWNINFO me = GetCharInfo()->pSpawn;

	LineVertex* vertices = new LineVertex[m_navpathLen];
	vertices[0] = { me->Y, me->X, me->Z + HEIGHT_OFFSET };
	for (int i = 0; i < m_navpathLen - 1; i++)
	{
		vertices[i + 1].x = navPathPoints[(i * 3) + 2];
		vertices[i + 1].y = navPathPoints[(i * 3) + 0];
		vertices[i + 1].z = navPathPoints[(i * 3) + 1] + HEIGHT_OFFSET;
	}

	HRESULT result = D3D_OK;

	if (m_pLines == nullptr )
	{
		result = g_pDevice->CreateVertexBuffer((m_navpathLen + 1) * sizeof(LineVertex),
			D3DUSAGE_WRITEONLY,
			LineVertex::FVF_Flags,
			D3DPOOL_DEFAULT,
			&m_pLines,
			NULL);
	}

	if (result == D3D_OK)
	{
		void* pVertices = NULL;

		// lock v_buffer and load the vertices into it
		m_pLines->Lock(0, m_navpathLen * sizeof(LineVertex), (void**)&pVertices, 0);
		memcpy(pVertices, vertices, m_navpathLen * sizeof(LineVertex));
		m_pLines->Unlock();
	}

	delete[] vertices;
}

void RenderHandler::ClearNavigationPath()
{
	m_navpathLen = 0;
	m_navPath = nullptr;

	if (m_pLines)
	{
		m_pLines->Release();
		m_pLines = 0;
	}
}
