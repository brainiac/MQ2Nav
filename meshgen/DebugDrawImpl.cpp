//
// GLUtilities.cpp
//

#include "meshgen/DebugDrawImpl.h"
#include "engine/embedded_shader.h"
#include "engine/bgfx_utils.h"

#include <bgfx/bgfx.h>
#include <bx/string.h>
#include <glm/glm.hpp>

#include "ZoneRenderManager.h"

bgfx::VertexLayout DebugDrawVertex::ms_layout;

#include "shaders/debugdraw/fs_debugdraw_primitive.bin.h"
#include "shaders/debugdraw/vs_debugdraw_primitive.bin.h"

static const bgfx::EmbeddedShader s_embeddedShaders[] = {
	BGFX_EMBEDDED_SHADER(fs_debugdraw_primitive),
	BGFX_EMBEDDED_SHADER(vs_debugdraw_primitive),

	BGFX_EMBEDDED_SHADER_END()
};

struct DebugDrawSharedData
{
	bgfx::ProgramHandle m_program;

	void init()
	{
		DebugDrawVertex::init();

		bgfx::RendererType::Enum type = bgfx::RendererType::Direct3D11;
		m_program = bgfx::createProgram(
			bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_debugdraw_primitive"),
			bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_debugdraw_primitive"), true);
	}

	void shutdown()
	{
		bgfx::destroy(m_program);
	}
};

static DebugDrawSharedData s_dds;

void DebugDrawImpl::init()
{
	s_dds.init();
}

void DebugDrawImpl::shutdown()
{
	s_dds.shutdown();
}

DebugDrawImpl::DebugDrawImpl()
{
}

DebugDrawImpl::~DebugDrawImpl()
{
}

void DebugDrawImpl::depthMask(bool state)
{
	m_depthEnabled = state;
}

void DebugDrawImpl::texture(bool state)
{
	m_textureEnabled = state;
}

void DebugDrawImpl::begin(duDebugDrawPrimitives prim, float size)
{
	m_encoder = bgfx::begin();
	m_indexPos = 0;
	m_pos = 0;
	m_vtxCurrent = 0;
	m_state = BGFX_STATE_MSAA | BGFX_STATE_WRITE_RGB | (m_depthEnabled ? BGFX_STATE_WRITE_Z : 0) | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA;

	switch (prim)
	{
	case DU_DRAW_POINTS: m_type = Points; m_vtxMax = 1; m_state |= BGFX_STATE_PT_POINTS;  break;
	case DU_DRAW_LINES: m_type = Lines; m_vtxMax = 2; m_state |= BGFX_STATE_PT_LINES;  break;
	case DU_DRAW_TRIS: m_type = Triangles; m_vtxMax = 3; break;
	case DU_DRAW_QUADS: m_type = Quads; m_vtxMax = 4; m_state |= BGFX_STATE_PT_TRISTRIP; break;
	}

}

void DebugDrawImpl::end()
{
	flush();

	m_encoder = nullptr;
}

void DebugDrawImpl::flush()
{
	if (m_pos != 0)
	{
		if (checkAvailTransientBuffers(m_pos, DebugDrawVertex::ms_layout, m_indexPos))
		{
			bgfx::TransientVertexBuffer tvb;
			bgfx::allocTransientVertexBuffer(&tvb, m_pos, DebugDrawVertex::ms_layout);
			bx::memCopy(tvb.data, m_cache, m_pos * DebugDrawVertex::ms_layout.m_stride);

			bgfx::TransientIndexBuffer tib;
			bgfx::allocTransientIndexBuffer(&tib, m_indexPos);
			bx::memCopy(tib.data, m_indices, m_indexPos * sizeof(uint16_t));

			m_encoder->setVertexBuffer(0, &tvb);
			m_encoder->setIndexBuffer(&tib);
			m_encoder->setState(m_state);

			m_encoder->submit(m_viewId, s_dds.m_program);
		}
		else
		{
			m_pos = 0;
		}

		m_pos = 0;
		m_indexPos = 0;
	}
}

void DebugDrawImpl::vertex(const glm::vec3& pos, const glm::vec2& uv, uint32_t color)
{
	uint16_t curr = m_pos++;
	DebugDrawVertex& vertex = m_cache[curr];
	vertex.pos = pos;
	vertex.uv = uv;
	vertex.color = color;

	m_vtxCurrent++;

	// Finished a shape. add indices
	if (m_vtxCurrent == m_vtxMax)
	{
		m_vtxCurrent = 0;

		if (m_type == Quads)
		{
			int index = curr - 3;
			m_indices[m_indexPos++] = index;
			m_indices[m_indexPos++] = index + 1;
			m_indices[m_indexPos++] = index + 2;
			m_indices[m_indexPos++] = index + 1;
			m_indices[m_indexPos++] = index + 3;
			m_indices[m_indexPos++] = index + 2;
		}
		else if (m_type == Triangles)
		{
			int index = curr - 2;
			m_indices[m_indexPos++] = index + 0;
			m_indices[m_indexPos++] = index + 1;
			m_indices[m_indexPos++] = index + 2;
		}
		else
		{
			for (int i = -m_vtxMax + 1; i < 1; ++i)
			{
				m_indices[m_indexPos++] = curr + i;
			}
		}

		if (m_pos + 6 > uint16_t(BX_COUNTOF(m_cache)))
		{
			flush();
		}
	}
}
