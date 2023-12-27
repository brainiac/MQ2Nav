//
// GLUtilities.cpp
//

#include "meshgen/DebugDrawImpl.h"
#include "engine/embedded_shader.h"
#include "engine/bgfx_utils.h"

#include <bgfx/bgfx.h>
#include <bx/string.h>
#include <glm/glm.hpp>

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
	static const int TEXTURE_SIZE = 64;

	bgfx::TextureHandle m_gridTexture     = BGFX_INVALID_HANDLE;
	int                 m_miplevels       = 0;
	bgfx::UniformHandle m_texColor        = BGFX_INVALID_HANDLE; // our 2d texture sampler
	bgfx::UniformHandle m_lineThickness   = BGFX_INVALID_HANDLE; // line thickness uniform

	bgfx::ProgramHandle m_program;
	bgfx::ProgramHandle m_textureProgram  = BGFX_INVALID_HANDLE;

	void init()
	{
		DebugDrawVertex::init();

		m_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
		m_lineThickness = bgfx::createUniform("s_lineThickness", bgfx::UniformType::Vec4);

		bgfx::RendererType::Enum type = bgfx::RendererType::Direct3D11;
		m_program = bgfx::createProgram(
			bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_debugdraw_primitive"),
			bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_debugdraw_primitive"), true);

		// Create the grid texture
		const unsigned int col0 = duRGBA(215, 215, 215, 255);
		const unsigned int col1 = duRGBA(255, 255, 255, 255);
		uint8_t data[TEXTURE_SIZE * TEXTURE_SIZE];
		int size = TEXTURE_SIZE;

		m_gridTexture = bgfx::createTexture2D(size, size, true, 1, bgfx::TextureFormat::RGB8, 0, nullptr);
		while (size > 0)
		{
			for (int y = 0; y < size; ++y)
			{
				for (int x = 0; x < size; ++x)
					data[x + y * size] = (x == 0 || y == 0) ? col0 : col1;
			}
			bgfx::updateTexture2D(m_gridTexture, 0, m_miplevels, 0, 0, size, size, bgfx::copy(data, size * size));

			size /= 2;
			m_miplevels++;
		}
	}

	void shutdown()
	{
		bgfx::destroy(m_gridTexture);
		bgfx::destroy(m_texColor);
		bgfx::destroy(m_lineThickness);
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

	m_lineThickness = size;
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

			float thickness[4] = { m_lineThickness, 0.f, 0.f, 0.f };
			m_encoder->setUniform(s_dds.m_lineThickness, thickness);

			bgfx::ProgramHandle program = s_dds.m_program;
			m_encoder->submit(m_viewId, program);
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
