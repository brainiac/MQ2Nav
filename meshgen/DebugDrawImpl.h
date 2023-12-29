//
// DebugDraw.h
//

#pragma once

#include "recast/Recast/Include/Recast.h"
#include "recast/DebugUtils/Include/DebugDraw.h"
#include "recast/DebugUtils/Include/RecastDump.h"
#include "engine/debugdraw/debugdraw.h"

#include <bgfx/bgfx.h>
#include <glm/glm.hpp>

struct DebugDrawVertex
{
	glm::vec3 pos;
	glm::vec2 uv;
	uint32_t  color;

	static void init()
	{
		ms_layout
			.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();
	}

	static bgfx::VertexLayout ms_layout;
};

class DebugDrawImpl : public duDebugDraw
{
	bgfx::Encoder* m_encoder = nullptr;
	bgfx::ViewId m_viewId = 0;
	bool m_depthEnabled = true;
	bool m_textureEnabled = false;

	enum PrimitiveType
	{
		Points,
		Lines,
		Triangles,
		Quads,
		
		Count
	};
	PrimitiveType m_type = PrimitiveType::Count;
	uint64_t m_state = 0;

	static const uint32_t kCacheSize = 1024;
	static const uint32_t kCacheQuadSize = 1024;

	DebugDrawVertex m_cache[kCacheSize + 1];
	uint16_t        m_indices[kCacheSize * 4];
	uint16_t        m_indexPos;
	uint16_t        m_pos;
	uint8_t         m_vtxCurrent;
	uint8_t         m_vtxMax;

	void flush();

public:
	DebugDrawImpl();
	~DebugDrawImpl();

	static void init();
	static void shutdown();

	virtual void begin(duDebugDrawPrimitives prim, float size = 1.0f) override;
	virtual void end() override;

	virtual void depthMask(bool state) override;
	virtual void texture(bool state) override;

	void vertex(const glm::vec3& pos, const glm::vec2& uv, uint32_t color);

	// Colored vertices
	virtual void vertex(const float* pos, unsigned int color) override
	{
		vertex(glm::vec3(pos[0], pos[1], pos[2]), glm::vec2(), color);
	}

	virtual void vertex(const float x, const float y, const float z, unsigned int color) override
	{
		vertex(glm::vec3(x, y, z), glm::vec2(), color);
	}

	// Textured vertices
	virtual void vertex(const float* pos, unsigned int color, const float* uv) override
	{
		vertex(glm::vec3(pos[0], pos[1], pos[2]), glm::vec2(uv[0], uv[1]), color);
	}

	virtual void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v) override
	{
		vertex(glm::vec3(x, y, z), glm::vec2(u, v), color);
	}
};
