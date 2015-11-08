//
// DebugDrawDX.h
//

#pragma once

#include "RenderList.h"

#include <DebugDraw.h>


class DebugDrawDX : public duDebugDraw
{
	RenderGroup* m_rl = nullptr;

public:
	DebugDrawDX(RenderGroup* rl_)
		: m_rl(rl_)
	{
	}

	virtual void end() override
	{
		m_rl->End();
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

		m_rl->Begin(type, size);
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
		m_rl->AddVertex(x, y, z, color, u, v);
	}
};