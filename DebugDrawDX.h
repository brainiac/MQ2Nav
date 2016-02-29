//
// DebugDrawDX.h
//

#pragma once

#include "RenderList.h"

#include <DebugDraw.h>

#define DD_F32_TO_INT8(_VAL)  ((int)((_VAL) * 255.0f + 0.5f))


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

struct DXColor
{
	glm::vec4 Value;

	DXColor() { Value.r = Value.g = Value.b = Value.a = 0.0f; }
	DXColor(int r, int g, int b, int a = 255) { float sc = 1.0f / 255.0f; Value.r = (float)r * sc; Value.g = (float)g * sc; Value.b = (float)b * sc; Value.a = (float)a * sc; }
	DXColor(float r, float g, float b, float a = 1.0f) { Value.r = r; Value.g = g; Value.b = b; Value.a = a; }
	DXColor(const DXColor& col) { Value = col.Value; }

	inline operator glm::vec4() const { return Value; }
	inline operator uint32_t() const { return ColorConvertFloat4ToU32(Value); }

	static inline uint32_t ColorConvertFloat4ToU32(const glm::vec4& in)
	{
		uint32_t out;
		out = ((uint32_t)DD_F32_TO_INT8(Saturate(in.x)));
		out |= ((uint32_t)DD_F32_TO_INT8(Saturate(in.y))) << 8;
		out |= ((uint32_t)DD_F32_TO_INT8(Saturate(in.z))) << 16;
		out |= ((uint32_t)DD_F32_TO_INT8(Saturate(in.w))) << 24;
		return out;
	}

	static inline float Saturate(float f) { return (f < 0.0f) ? 0.0f : (f > 1.0f) ? 1.0f : f; }
};
