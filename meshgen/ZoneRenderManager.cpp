#include "ZoneRenderManager.h"

#include "InputGeom.h"
#include "common/NavMeshData.h"
#include "common/NavMesh.h"
#include "engine/embedded_shader.h"
#include "shaders/navmesh/fs_inputgeom.bin.h"
#include "shaders/navmesh/vs_inputgeom.bin.h"
#include "shaders/navmesh/fs_meshtile.bin.h"
#include "shaders/navmesh/vs_meshtile.bin.h"
#include "shaders/lines/fs_lines.bin.h"
#include "shaders/lines/vs_lines.bin.h"
#include "shaders/points/fs_points.bin.h"
#include "shaders/points/vs_points.bin.h"

#include <glm/gtc/type_ptr.hpp>
#include <recast/DebugUtils/Include/DebugDraw.h>
#include <recast/Detour/Include/DetourNavMeshQuery.h>
#include <imgui/imgui.h>

#include "Camera.h"

ZoneRenderManager* g_zoneRenderManager = nullptr;

static const bgfx::EmbeddedShader s_embeddedShaders[] = {
	BGFX_EMBEDDED_SHADER(fs_inputgeom),
	BGFX_EMBEDDED_SHADER(vs_inputgeom),
	BGFX_EMBEDDED_SHADER(fs_meshtile),
	BGFX_EMBEDDED_SHADER(vs_meshtile),
	BGFX_EMBEDDED_SHADER(fs_lines),
	BGFX_EMBEDDED_SHADER(vs_lines),
	BGFX_EMBEDDED_SHADER(fs_points),
	BGFX_EMBEDDED_SHADER(vs_points),

	BGFX_EMBEDDED_SHADER_END()
};

//============================================================================

struct DebugDrawQuadTemplateVertex
{
	glm::vec3 pos;

	static void init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		.end();
	}

	inline static bgfx::VertexLayout ms_layout;
};

struct ZoneRenderShared
{
	bool m_initialized = false;

	bgfx::TextureHandle m_gridTexture = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_texSampler = BGFX_INVALID_HANDLE;

	bgfx::ProgramHandle m_inputGeoProgram = BGFX_INVALID_HANDLE;
	bgfx::ProgramHandle m_meshTileProgram = BGFX_INVALID_HANDLE;

	bgfx::UniformHandle m_cameraRight = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_cameraUp = BGFX_INVALID_HANDLE;

	// Primitives implementation
	bgfx::ProgramHandle m_pointsProgram = BGFX_INVALID_HANDLE;
	bgfx::ProgramHandle m_linesProgram = BGFX_INVALID_HANDLE;
	bgfx::VertexBufferHandle m_quad1VB = BGFX_INVALID_HANDLE;
	bgfx::VertexBufferHandle m_quad2VB = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle m_quadIB = BGFX_INVALID_HANDLE;

	void init()
	{
		DebugDrawGridTexturedVertex::init();
		DebugDrawPolyVertex::init();
		DebugDrawQuadTemplateVertex::init();
		DebugDrawLineVertex::init();
		DebugDrawPointVertex::init();

		// Create the grid texture
		static constexpr int TEXTURE_SIZE = 64;
		uint32_t data[TEXTURE_SIZE * TEXTURE_SIZE];
		int size = TEXTURE_SIZE;
		int mipLevel = 0;

		m_gridTexture = bgfx::createTexture2D(size, size, true, 1, bgfx::TextureFormat::RGBA8, 0, nullptr);
		while (size > 0)
		{
			for (int y = 0; y < size; ++y)
			{
				for (int x = 0; x < size; ++x)
					data[x + y * size] = (x == 0 || y == 0) ? 0xffd7d7d7 : 0xffffffff;
			}
			bgfx::updateTexture2D(m_gridTexture, 0, mipLevel++, 0, 0, size, size, bgfx::copy(data, sizeof(uint32_t) * size * size));

			size /= 2;
		}

		m_texSampler = bgfx::createUniform("textureSampler", bgfx::UniformType::Sampler);
		m_cameraRight = bgfx::createUniform("CameraRight_worldspace", bgfx::UniformType::Vec4);
		m_cameraUp = bgfx::createUniform("CameraUp_worldspace", bgfx::UniformType::Vec4);

		bgfx::RendererType::Enum type = bgfx::RendererType::Direct3D11;
		m_inputGeoProgram = bgfx::createProgram(
			bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_inputgeom"),
			bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_inputgeom"), true);
		m_meshTileProgram = bgfx::createProgram(
			bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_meshtile"),
			bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_meshtile"), true);

		m_linesProgram = bgfx::createProgram(
			bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_lines"),
			bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_lines"), true);
		m_pointsProgram = bgfx::createProgram(
			bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_points"),
			bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_points"), true);

		glm::vec3 linesQuad[] = {
			{ 0.0, -1.0, 0.0 },
			{ 0.0,  1.0, 0.0 },
			{ 1.0,  1.0, 0.0 },
			{ 1.0, -1.0, 0.0 }
		};
		m_quad1VB = bgfx::createVertexBuffer(bgfx::copy(linesQuad, sizeof(linesQuad)), DebugDrawQuadTemplateVertex::ms_layout, 0);

		glm::vec3 pointQuad[] = {
			{ -0.5f, -0.5f, 0.0f },
			{ -0.5f,  0.5f, 0.0f },
			{  0.5f,  0.5f, 0.0f },
			{  0.5f, -0.5f, 0.0f }
		};
		m_quad2VB = bgfx::createVertexBuffer(bgfx::copy(pointQuad, sizeof(pointQuad)), DebugDrawQuadTemplateVertex::ms_layout, 0);

		uint16_t linesIndices[] = {
			0, 1, 2, 0, 2, 3
		};
		m_quadIB = bgfx::createIndexBuffer(bgfx::copy(linesIndices, sizeof(linesIndices)), 0);

		m_initialized = true;
	}

	void shutdown()
	{
		if (m_initialized)
		{
			bgfx::destroy(m_gridTexture);
			bgfx::destroy(m_texSampler);
			bgfx::destroy(m_cameraRight);
			bgfx::destroy(m_cameraUp);
			bgfx::destroy(m_inputGeoProgram);
			bgfx::destroy(m_meshTileProgram);
			bgfx::destroy(m_linesProgram);
			bgfx::destroy(m_quad1VB);
			bgfx::destroy(m_quad2VB);
			bgfx::destroy(m_quadIB);
			bgfx::destroy(m_pointsProgram);
		}
	}
};

static ZoneRenderShared s_shared;

//============================================================================

ZoneRenderDebugDraw::ZoneRenderDebugDraw(ZoneRenderManager* render)
	: m_render(render)
{
}

ZoneRenderDebugDraw::~ZoneRenderDebugDraw()
{
}

void ZoneRenderDebugDraw::begin(duDebugDrawPrimitives type, float size)
{
	m_width = size;
	m_type = type;
	m_first = false;
	m_numVertices = 0;
}

void ZoneRenderDebugDraw::end()
{
}

void ZoneRenderDebugDraw::vertex(const float* pos_, unsigned int color)
{
	switch (m_type)
	{
	case DU_DRAW_POINTS:
		m_render->m_points.emplace_back(*reinterpret_cast<const glm::vec3*>(pos_),
			m_width * m_render->m_pointSize, ImColor(color));
		break;

	case DU_DRAW_LINES: {
		if (m_first)
		{
			m_render->m_lines.emplace_back(m_lastPos, m_width, ImColor(m_lastColor),
				*reinterpret_cast<const glm::vec3*>(pos_), m_width, ImColor(color));
		}
		else
		{
			m_lastPos = *reinterpret_cast<const glm::vec3*>(pos_);
			m_lastColor = color;
		}

		m_first = !m_first;
		break;
	}
	case DU_DRAW_TRIS:
		m_render->m_tris.emplace_back(*reinterpret_cast<const glm::vec3*>(pos_), color);
		++m_numVertices;
		if (m_numVertices == 3)
		{
			uint16_t index = static_cast<uint16_t>(m_render->m_tris.size()) - 3;
			m_render->m_triIndices.push_back(index);
			m_render->m_triIndices.push_back(index + 1);
			m_render->m_triIndices.push_back(index + 2);
			m_numVertices = 0;
		}
		break;
	case DU_DRAW_QUADS:
		m_render->m_tris.emplace_back(*reinterpret_cast<const glm::vec3*>(pos_), color);
		++m_numVertices;
		if (m_numVertices == 4)
		{
			uint16_t index = static_cast<uint16_t>(m_render->m_tris.size()) - 4;
			m_render->m_triIndices.push_back(index);
			m_render->m_triIndices.push_back(index + 1);
			m_render->m_triIndices.push_back(index + 2);
			m_render->m_triIndices.push_back(index + 1);
			m_render->m_triIndices.push_back(index + 3);
			m_render->m_triIndices.push_back(index + 2);
			m_numVertices = 0;
		}
		break;
	default: break;
	}
}

void ZoneRenderDebugDraw::vertex(const float x, const float y, const float z, unsigned int color)
{
	switch (m_type)
	{
	case DU_DRAW_POINTS:
		m_render->m_points.emplace_back(glm::vec3(x, y, z), m_width * m_render->m_pointSize, ImColor(color));
		break;

	case DU_DRAW_LINES: {
		if (m_first)
		{
			m_render->m_lines.emplace_back(m_lastPos, m_width, ImColor(m_lastColor),
				glm::vec3(x, y, z), m_width, ImColor(color));
		}
		else
		{
			m_lastPos = glm::vec3(x, y, z);
			m_lastColor = color;
		}

		m_first = !m_first;
		break;
	}
	case DU_DRAW_TRIS:
		m_render->m_tris.emplace_back(glm::vec3(x, y, z), color);
		++m_numVertices;
		if (m_numVertices == 3)
		{
			uint16_t index = static_cast<uint16_t>(m_render->m_tris.size()) - 3;
			m_render->m_triIndices.push_back(index);
			m_render->m_triIndices.push_back(index + 1);
			m_render->m_triIndices.push_back(index + 2);
			m_numVertices = 0;
		}
		break;
	case DU_DRAW_QUADS:
		m_render->m_tris.emplace_back(glm::vec3(x, y, z), color);
		++m_numVertices;
		if (m_numVertices == 4)
		{
			uint16_t index = static_cast<uint16_t>(m_render->m_tris.size()) - 4;
			m_render->m_triIndices.push_back(index);
			m_render->m_triIndices.push_back(index + 1);
			m_render->m_triIndices.push_back(index + 2);
			m_render->m_triIndices.push_back(index + 1);
			m_render->m_triIndices.push_back(index + 3);
			m_render->m_triIndices.push_back(index + 2);
			m_numVertices = 0;
		}
		break;

	default: break;
	}
}

unsigned int ZoneRenderDebugDraw::polyToCol(const dtPoly* poly)
{
	return m_render->GetNavMeshRender()->PolyToCol(poly);
}

//============================================================================

ZoneRenderManager::ZoneRenderManager()
{
	m_zoneInputGeometry = new ZoneInputGeometryRender(this);
	m_navMeshRender = new ZoneNavMeshRender(this);

	// TEMP
	g_zoneRenderManager = this;
}

ZoneRenderManager::~ZoneRenderManager()
{
	delete m_zoneInputGeometry;
	m_zoneInputGeometry = nullptr;

	delete m_navMeshRender;
	m_navMeshRender = nullptr;

	// TEMP
	g_zoneRenderManager = nullptr;
}

void ZoneRenderManager::init()
{
	s_shared.init();
}

void ZoneRenderManager::shutdown()
{
	s_shared.shutdown();
}

void ZoneRenderManager::DestroyObjects()
{
	m_zoneInputGeometry->DestroyObjects();
	m_navMeshRender->DestroyObjects();

	if (isValid(m_ddLinesVB))
	{
		bgfx::destroy(m_ddLinesVB);
		m_ddLinesVB = BGFX_INVALID_HANDLE;
	}

	if (isValid(m_ddPointsVB))
	{
		bgfx::destroy(m_ddPointsVB);
		m_ddPointsVB = BGFX_INVALID_HANDLE;
	}

	if (isValid(m_ddTrisVB))
	{
		bgfx::destroy(m_ddTrisVB);
		m_ddTrisVB = BGFX_INVALID_HANDLE;
	}

	if (isValid(m_ddIndexBuffer))
	{
		bgfx::destroy(m_ddIndexBuffer);
		m_ddIndexBuffer = BGFX_INVALID_HANDLE;
	}

	m_lastLinesSize = 0;
	m_lastTrisSize = 0;
	m_lastTrisIndicesSize = 0;
}

void ZoneRenderManager::SetInputGeometry(InputGeom* geom)
{
	m_zoneInputGeometry->SetInputGeometry(geom);
}

void ZoneRenderManager::SetNavMeshConfig(const NavMeshConfig* config)
{
	m_meshConfig = config;
}

void ZoneRenderManager::Render()
{
	glm::mat4 view;
	m_camera->GetViewMatrix(glm::value_ptr(view));

	glm::vec4 cameraRight(view[0][0], view[1][0], view[2][0], 1.0f);
	glm::vec3 cameraRight2 = m_camera->GetRight();

	bgfx::setUniform(s_shared.m_cameraRight, glm::value_ptr(cameraRight2));

	glm::vec4 cameraUp(view[0][1], view[1][1], view[2][1], 1.0f);
	glm::vec3 cameraUp2 = m_camera->GetUp();

	bgfx::setUniform(s_shared.m_cameraUp, glm::value_ptr(cameraUp2));

	if (!m_points.empty())
	{
		if (isValid(m_ddPointsVB) && m_points.size() == m_lastPointsSize)
		{
			bgfx::update(m_ddPointsVB, 0, bgfx::copy(m_points.data(), static_cast<uint32_t>(m_points.size()) * DebugDrawPointVertex::ms_layout.getStride()));
		}
		else
		{
			if (isValid(m_ddPointsVB))
			{
				bgfx::destroy(m_ddPointsVB);
			}

			m_ddPointsVB = bgfx::createDynamicVertexBuffer(
				bgfx::copy(m_points.data(), static_cast<uint32_t>(m_points.size()) * DebugDrawPointVertex::ms_layout.getStride()),
				DebugDrawPointVertex::ms_layout);
		}

		m_lastPointsSize = m_points.size();

		bgfx::Encoder* encoder = bgfx::begin();
		encoder->setVertexBuffer(0, s_shared.m_quad2VB);
		encoder->setIndexBuffer(s_shared.m_quadIB);
		encoder->setInstanceDataBuffer(m_ddPointsVB, 0, static_cast<uint32_t>(m_points.size()));
		encoder->setState(BGFX_STATE_MSAA | BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_BLEND_ALPHA);
		encoder->submit(0, s_shared.m_pointsProgram);

		m_points.clear();
	}

	if (!m_lines.empty())
	{
		if (isValid(m_ddLinesVB) && m_lines.size() == m_lastLinesSize)
		{
			bgfx::update(m_ddLinesVB, 0, bgfx::copy(m_lines.data(), static_cast<uint32_t>(m_lines.size()) * DebugDrawLineVertex::ms_layout.getStride()));
		}
		else
		{
			if (isValid(m_ddLinesVB))
			{
				bgfx::destroy(m_ddLinesVB);
			}

			m_ddLinesVB = bgfx::createDynamicVertexBuffer(
				bgfx::copy(m_lines.data(), static_cast<uint32_t>(m_lines.size()) * DebugDrawLineVertex::ms_layout.getStride()),
				DebugDrawLineVertex::ms_layout);
		}

		m_lastLinesSize = m_lines.size();

		bgfx::Encoder* encoder = bgfx::begin();
		encoder->setVertexBuffer(0, s_shared.m_quad1VB);
		encoder->setIndexBuffer(s_shared.m_quadIB);
		encoder->setInstanceDataBuffer(m_ddLinesVB, 0, static_cast<uint32_t>(m_lines.size()));
		encoder->setState(BGFX_STATE_MSAA | BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_BLEND_ALPHA);
		encoder->submit(0, s_shared.m_linesProgram);

		m_lines.clear();
	}

	if (!m_tris.empty())
	{
		if (isValid(m_ddTrisVB) && m_tris.size() == m_lastTrisSize && m_triIndices.size() == m_lastTrisIndicesSize)
		{
			bgfx::update(m_ddTrisVB, 0, bgfx::copy(m_tris.data(), static_cast<uint32_t>(m_tris.size()) * DebugDrawPolyVertex::ms_layout.getStride()));
			bgfx::update(m_ddIndexBuffer, 0, bgfx::copy(m_triIndices.data(), static_cast<uint32_t>(m_triIndices.size() * sizeof(uint16_t))));
		}
		else
		{
			if (isValid(m_ddTrisVB))
			{
				bgfx::destroy(m_ddTrisVB);
			}

			if (isValid(m_ddIndexBuffer))
			{
				bgfx::destroy(m_ddIndexBuffer);
			}

			m_ddTrisVB = bgfx::createDynamicVertexBuffer(
				bgfx::copy(m_tris.data(), static_cast<uint32_t>(m_tris.size()) * DebugDrawPolyVertex::ms_layout.getStride()),
				DebugDrawPolyVertex::ms_layout);
			m_ddIndexBuffer = bgfx::createDynamicIndexBuffer(
				bgfx::copy(m_triIndices.data(), static_cast<uint32_t>(m_triIndices.size() * sizeof(uint16_t))));
		}

		m_lastTrisSize = m_tris.size();
		m_lastTrisIndicesSize = m_triIndices.size();

		bgfx::Encoder* encoder = bgfx::begin();
		encoder->setVertexBuffer(0, m_ddTrisVB);
		encoder->setIndexBuffer(m_ddIndexBuffer, 0, static_cast<uint32_t>(m_triIndices.size()));
		encoder->setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
		encoder->submit(0, s_shared.m_meshTileProgram);

		m_tris.clear();
		m_triIndices.clear();
	}
}

//----------------------------------------------------------------------------

ZoneInputGeometryRender::ZoneInputGeometryRender(ZoneRenderManager* manager)
	: m_mgr(manager)
{
}

ZoneInputGeometryRender::~ZoneInputGeometryRender()
{
}

void ZoneInputGeometryRender::SetInputGeometry(InputGeom* geom)
{
	if (m_geom == geom)
		return;

	m_geom = geom;

	DestroyObjects();

	if (m_geom)
	{
		CreateObjects();
	}
}

void ZoneInputGeometryRender::CreateObjects()
{
	auto loader = m_geom->getMeshLoader();
	const NavMeshConfig* config = m_mgr->GetNavMeshConfig();

	const glm::vec3* verts = reinterpret_cast<const glm::vec3*>(loader->getVerts());
	const int numVerts = loader->getVertCount();
	const int* tris = loader->getTris();
	const float* normals = loader->getNormals();
	const int ntris = loader->getTriCount();
	const float texScale = 1.0f / (config->cellSize * 10.0f);
	const float walkableThr = cosf(config->agentMaxSlope / 180.0f * DU_PI);
	const uint32_t unwalkable = duRGBA(192, 128, 0, 255);

	// Create buffer to hold list of triangles
	const bgfx::Memory* vb = bgfx::alloc(ntris * 3 * DebugDrawGridTexturedVertex::ms_layout.getStride());
	const bgfx::Memory* ib = bgfx::alloc(ntris * 3 * sizeof(uint32_t));

	DebugDrawGridTexturedVertex* pVertex = reinterpret_cast<DebugDrawGridTexturedVertex*>(vb->data);
	uint32_t* pIndex = reinterpret_cast<uint32_t*>(ib->data);

	int index;
	for (index = 0; index < ntris * 3; index += 3)
	{
		const float* norm = &normals[index];
		uint32_t color;

		uint8_t a = static_cast<uint8_t>(220 * (2 + norm[0] + norm[1]) / 4);
		if (norm[1] < walkableThr)
			color = duLerpCol(duRGBA(a, a, a, 255), unwalkable, 64);
		else
			color = duRGBA(a, a, a, 255);

		int ax = 0, ay = 0;
		if (glm::abs(norm[1]) > glm::abs(norm[ax]))
			ax = 1;
		if (glm::abs(norm[2]) > glm::abs(norm[ax]))
			ax = 2;
		ax = (1 << ax) & 3; // +1 mod 3
		ay = (1 << ax) & 3; // +1 mod 3

		const glm::vec3& v1 = verts[tris[index + 0]];
		pVertex[index + 0].pos = v1;
		pVertex[index + 0].uv = glm::vec2(v1[ax] * texScale, v1[ay] * texScale);
		pVertex[index + 0].color = color;
		pIndex[index + 0] = index;

		const glm::vec3& v2 = verts[tris[index + 1]];
		pVertex[index + 1].pos = v2;
		pVertex[index + 1].uv = glm::vec2(v2[ax] * texScale, v2[ay] * texScale);
		pVertex[index + 1].color = color;
		pIndex[index + 1] = index + 1;

		const glm::vec3& v3 = verts[tris[index + 2]];
		pVertex[index + 2].pos = v3;
		pVertex[index + 2].uv = glm::vec2(v3[ax] * texScale, v3[ay] * texScale);
		pVertex[index + 2].color = color;
		pIndex[index + 2] = index + 2;
	}

	m_numIndices = index;
	m_vbh = bgfx::createVertexBuffer(vb, DebugDrawGridTexturedVertex::ms_layout);
	m_ibh = bgfx::createIndexBuffer(ib, BGFX_BUFFER_INDEX32);
}

void ZoneInputGeometryRender::DestroyObjects()
{
	if (isValid(m_vbh))
	{
		bgfx::destroy(m_vbh);
		m_vbh = BGFX_INVALID_HANDLE;
	}

	if (isValid(m_ibh))
	{
		bgfx::destroy(m_ibh);
		m_ibh = BGFX_INVALID_HANDLE;
	}
}

void ZoneInputGeometryRender::Render()
{
	if (isValid(m_ibh) && isValid(m_vbh))
	{
		bgfx::Encoder* encoder = bgfx::begin();

		encoder->setVertexBuffer(0, m_vbh);
		encoder->setIndexBuffer(m_ibh, 0, m_numIndices);
		encoder->setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
		encoder->setTexture(0, s_shared.m_texSampler, s_shared.m_gridTexture);

		encoder->submit(0, s_shared.m_inputGeoProgram);
	}
}

//----------------------------------------------------------------------------
//============================================================================


ZoneNavMeshRender::ZoneNavMeshRender(ZoneRenderManager* manager)
	: m_mgr(manager)
{
}

ZoneNavMeshRender::~ZoneNavMeshRender()
{
}

void ZoneNavMeshRender::SetNavMesh(const std::shared_ptr<NavMesh>& navMesh)
{
	if (m_navMesh == navMesh)
		return;

	m_navMesh = navMesh;
	m_dirty = true;
}

void ZoneNavMeshRender::SetNavMeshQuery(const dtNavMeshQuery* query)
{
	if (m_query == query)
		return;

	m_query = query;
	m_dirty = true;
}

void ZoneNavMeshRender::SetFlags(uint32_t flags)
{
	if (m_flags == flags)
		return;

	m_flags = flags;
	m_dirty = true;
}

void ZoneNavMeshRender::Render()
{
	if (m_dirty)
	{
		Build();
		m_dirty = false;
	}

	if ((m_flags & ZoneNavMeshRender::DRAW_TILES) && isValid(m_indexBuffer) && isValid(m_tilePolysVB))
	{
		bgfx::Encoder* encoder = bgfx::begin();

		encoder->setVertexBuffer(0, m_tilePolysVB);
		encoder->setIndexBuffer(m_indexBuffer, m_tileIndices.first, m_tileIndices.second - m_tileIndices.first);
		encoder->setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);

		encoder->submit(0, s_shared.m_meshTileProgram);
	}

	if ((m_flags & ZoneNavMeshRender::DRAW_POLY_BOUNDARIES) && isValid(m_lineInstances))
	{
		bgfx::Encoder* encoder = bgfx::begin();

		encoder->setVertexBuffer(0, s_shared.m_quad1VB);
		encoder->setIndexBuffer(s_shared.m_quadIB);
		encoder->setInstanceDataBuffer(m_lineInstances, 0, m_lineIndices);
		encoder->setState(BGFX_STATE_MSAA | BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_BLEND_ALPHA);

		encoder->submit(0, s_shared.m_linesProgram);
	}

	if ((m_flags & ZoneNavMeshRender::DRAW_POLY_BOUNDARIES) && isValid(m_pointsInstances))
	{
		bgfx::Encoder* encoder = bgfx::begin();

		encoder->setVertexBuffer(0, s_shared.m_quad2VB);
		encoder->setIndexBuffer(s_shared.m_quadIB);
		encoder->setInstanceDataBuffer(m_pointsInstances, 0, m_pointsIndices);
		encoder->setState(BGFX_STATE_MSAA | BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_BLEND_ALPHA);

		encoder->submit(0, s_shared.m_pointsProgram);
	}
}

void ZoneNavMeshRender::DestroyObjects()
{
	if (isValid(m_indexBuffer))
	{
		bgfx::destroy(m_indexBuffer);
		m_indexBuffer = BGFX_INVALID_HANDLE;
	}

	if (isValid(m_tilePolysVB))
	{
		bgfx::destroy(m_tilePolysVB);
		m_tilePolysVB = BGFX_INVALID_HANDLE;
	}

	if (isValid(m_lineInstances))
	{
		bgfx::destroy(m_lineInstances);
		m_lineInstances = BGFX_INVALID_HANDLE;
	}

	if (isValid(m_pointsInstances))
	{
		bgfx::destroy(m_pointsInstances);
		m_pointsInstances = BGFX_INVALID_HANDLE;
	}

	m_tileIndices = { 0, 0 };
	m_lineIndices = 0;
	m_pointsIndices = 0;
}

void ZoneNavMeshRender::Build()
{
	std::shared_ptr<dtNavMesh> navMesh = m_navMesh->GetNavMesh();
	if (!navMesh)
	{
		return;
	}

	DestroyObjects();

	const dtNavMeshQuery* q = m_flags & DRAW_CLOSED_LIST ? m_query : nullptr;
	const dtNavMesh& mesh = *m_navMesh->GetNavMesh();

	// Build navmesh tiles
	std::vector<DebugDrawPolyVertex> navMeshTileVertices;
	std::vector<uint32_t> navMeshTileIndices;
	std::vector<DebugDrawLineVertex> polyBoundaryVertices;
	std::vector<DebugDrawPointVertex> polyPointVertices;

	size_t numMeshTileVertices = 0;
	size_t numPolyBoundaryVertices = 0;
	size_t numPolyPointVertices = 0;

	// Pre-calculate the number of vertices we will need
	for (int i = 0; i < mesh.getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh.getTile(i);
		if (!tile->header) continue;

		// Calculate number of vertices that we will need to store.
		for (int j = 0; j < tile->header->polyCount; ++j)
		{
			const dtPoly* p = &tile->polys[j];
			if (p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
				continue;

			// Three vertices for each triangle in the detail mesh.
			const dtPolyDetail* pd = &tile->detailMeshes[j];
			numMeshTileVertices += pd->triCount * 3;
			numPolyBoundaryVertices += pd->triCount * 3 * 2;
		}

		numPolyPointVertices += tile->header->vertCount * 3;
		numPolyBoundaryVertices += tile->header->vertCount;
	}

	navMeshTileIndices.reserve(numMeshTileVertices);
	navMeshTileIndices.reserve(numMeshTileVertices);

	// Build tile polygons
	for (int i = 0; i < mesh.getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh.getTile(i);
		if (!tile->header) continue;

		dtPolyRef base = mesh.getPolyRefBase(tile);

		// Draw the tile polygons
		BuildMeshTile(navMeshTileVertices, navMeshTileIndices, base, mesh, q, tile, m_flags);

		// Draw inner poly boundaries
		BuildPolyBoundaries(polyBoundaryVertices, tile, duRGBA(0, 48, 64, 32), 1.5, true);

		// Draw outer poly boundaries
		BuildPolyBoundaries(polyBoundaryVertices, tile, duRGBA(0, 48, 64, 220), 2.5f, false);

		// Build points.
		const uint32_t vcol = duRGBA(0, 0, 0, 196);
		for (int j = 0; j < tile->header->vertCount; ++j)
		{
			const float* v = &tile->verts[j * 3];

			polyPointVertices.emplace_back(*reinterpret_cast<const glm::vec3*>(v), m_pointSize * 3.0f, vcol);
		}
	}

	// Create vertex buffer from gathered data.
	m_tilePolysVB = bgfx::createVertexBuffer(bgfx::copy(navMeshTileVertices.data(),
		static_cast<uint32_t>(navMeshTileVertices.size()) * DebugDrawPolyVertex::ms_layout.getStride()),
		DebugDrawPolyVertex::ms_layout);
	m_tileIndices = { 0, static_cast<uint32_t>(navMeshTileIndices.size()) };
	m_indexBuffer = bgfx::createIndexBuffer(
		bgfx::copy(navMeshTileIndices.data(), static_cast<uint32_t>(navMeshTileIndices.size()) * sizeof(uint32_t)),
		BGFX_BUFFER_INDEX32);

	// Lines buffer
	m_lineInstances = bgfx::createVertexBuffer(bgfx::copy(polyBoundaryVertices.data(),
		static_cast<uint32_t>(polyBoundaryVertices.size()) * DebugDrawLineVertex::ms_layout.getStride()),
		DebugDrawLineVertex::ms_layout);
	m_lineIndices = static_cast<int>(polyBoundaryVertices.size());

	// Points buffer
	m_pointsInstances = bgfx::createVertexBuffer(bgfx::copy(polyPointVertices.data(),
		static_cast<uint32_t>(polyPointVertices.size()) * DebugDrawPointVertex::ms_layout.getStride()),
		DebugDrawPointVertex::ms_layout);
	m_pointsIndices = static_cast<int>(polyPointVertices.size());
}

void ZoneNavMeshRender::UpdateQuery()
{
	// Do the work to update the existing mesh with updated closed list indicators...
}

#if 0
void ZoneNavMeshRender::BuildNodes()
{
	if (!m_query) return;

	const dtNavMeshQuery& query = *m_query;

	if (const dtNodePool* pool = query.getNodePool())
	{
		constexpr float off = 0.5f;
	
		//dd->begin(DU_DRAW_POINTS, 4.0f);
		for (int i = 0; i < pool->getHashSize(); ++i)
		{
			for (dtNodeIndex j = pool->getFirst(i); j != DT_NULL_IDX; j = pool->getNext(j))
			{
				if (const dtNode* node = pool->getNodeAtIdx(j + 1))
				{
					//dd->vertex(node->pos[0], node->pos[1] + off, node->pos[2], duRGBA(255, 192, 0, 255));
				}
			}
		}
		//dd->end();

		//dd->begin(DU_DRAW_LINES, 2.0f);
		for (int i = 0; i < pool->getHashSize(); ++i)
		{
			for (dtNodeIndex j = pool->getFirst(i); j != DT_NULL_IDX; j = pool->getNext(j))
			{
				const dtNode* node = pool->getNodeAtIdx(j + 1);
				if (!node) continue;
				if (!node->pidx) continue;
				if (const dtNode* parent = pool->getNodeAtIdx(node->pidx))
				{
					//dd->vertex(node->pos[0], node->pos[1] + off, node->pos[2], duRGBA(255, 192, 0, 128));
					//dd->vertex(parent->pos[0], parent->pos[1] + off, parent->pos[2], duRGBA(255, 192, 0, 128));
				}
			}
		}
		//dd->end();
	}
}
#endif


//============================================================================
//============================================================================

void ZoneNavMeshRender::BuildMeshTile(std::vector<DebugDrawPolyVertex>& vertices, std::vector<uint32_t>& indices,
	dtPolyRef base, const dtNavMesh& mesh, const dtNavMeshQuery* query, const dtMeshTile* tile, uint8_t flags)
{
	int currIndex = (int)vertices.size();

	int tileNum = mesh.decodePolyIdTile(base);
	const uint32_t tileColor = duIntToCol(tileNum, 128);

	for (int i = 0; i < tile->header->polyCount; ++i)
	{
		const dtPoly* p = &tile->polys[i];
		if (p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
			continue;

		const dtPolyDetail* pd = &tile->detailMeshes[i];

		// select color for the polygon.
		uint32_t col;
		if (flags & ZoneNavMeshRender::DRAW_COLORED_TILES)
			col = tileColor;
		else
			col = duTransCol(PolyToCol(p), 64);

		if (query && query->isInClosedList(base | static_cast<dtPolyRef>(i)))
		{
			col = duLerpCol(col, duRGBA(255, 196, 0, 64), 127);
		}

		// Iterate triangles
		for (int j = 0; j < pd->triCount; ++j)
		{
			const uint8_t* t = &tile->detailTris[(pd->triBase + j) * 4];

			for (int k = 0; k < 3; ++k)
			{
				if (t[k] < p->vertCount)
				{
					glm::vec3 pos = *reinterpret_cast<glm::vec3*>(&tile->verts[p->verts[t[k]] * 3]);
					pos.y += 0.1f;

					vertices.emplace_back(pos, col);
				}
				else
				{
					glm::vec3 pos = *reinterpret_cast<glm::vec3*>(&tile->detailVerts[(pd->vertBase + t[k] - p->vertCount) * 3]);
					pos.y += 0.1f;

					vertices.emplace_back(pos, col);
				}

				indices.push_back(currIndex);
				currIndex++;
			}
		}
	}
}

static float distancePtLine2d(const float* pt, const float* p, const float* q)
{
	float pqx = q[0] - p[0];
	float pqz = q[2] - p[2];
	float dx = pt[0] - p[0];
	float dz = pt[2] - p[2];
	float d = pqx * pqx + pqz * pqz;
	float t = pqx * dx + pqz * dz;
	if (d != 0) t /= d;
	dx = p[0] + t * pqx - pt[0];
	dz = p[2] + t * pqz - pt[2];
	return dx * dx + dz * dz;
}

/* static */
void ZoneNavMeshRender::BuildPolyBoundaries(std::vector<DebugDrawLineVertex>& vertices,
	const dtMeshTile* tile, uint32_t color, float width, bool inner)
{
	static constexpr float threshold = 0.01f * 0.01f;

	// Draw Lines:
	for (int i = 0; i < tile->header->polyCount; ++i)
	{
		const dtPoly* p = &tile->polys[i];
		if (p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
			continue;

		const dtPolyDetail* pd = &tile->detailMeshes[i];

		// Iterate triangles
		for (int j = 0, nj = (int)p->vertCount; j < nj; ++j)
		{
			// Determine color of the triangle
			uint32_t c = color;

			if (inner)
			{
				if (p->neis[j] == 0)
					continue;

				// If poly links to external poly, check for external poly.
				if (p->neis[j] & DT_EXT_LINK)
				{
					bool con = false;

					for (uint32_t k = p->firstLink; k != DT_NULL_LINK; k = tile->links[k].next)
					{
						if (tile->links[k].edge == j)
						{
							con = true;
							break;
						}
					}

					if (con)
						c = duRGBA(255, 255, 255, 48);
					else
						c = duRGBA(0, 0, 0, 48);
				}
				else
				{
					c = duRGBA(0, 48, 64, 32);
				}
			}
			else
			{
				if (p->neis[j] != 0)
					continue;
			}

			const float* v0 = &tile->verts[p->verts[j] * 3];
			const float* v1 = &tile->verts[p->verts[(j + 1) % nj] * 3];

			// Draw detail mesh edges which align with the actual poly edge.
			// This is really slow.
			for (int k = 0; k < pd->triCount; ++k)
			{
				const uint8_t* t = &tile->detailTris[(pd->triBase + k) * 4];
				const float* tv[3];

				for (int m = 0; m < 3; ++m)
				{
					if (t[m] < p->vertCount)
						tv[m] = &tile->verts[p->verts[t[m]] * 3];
					else
						tv[m] = &tile->detailVerts[(pd->vertBase + (t[m] - p->vertCount)) * 3];
				}

				for (int m = 0, n = 2; m < 3; n = m++)
				{
					if (((t[3] >> (n * 2)) & 0x3) == 0)
						continue; // Skip inner detail edges.

					if (distancePtLine2d(tv[n], v0, v1) < threshold
						&& distancePtLine2d(tv[m], v0, v1) < threshold)
					{
						vertices.emplace_back(
							*reinterpret_cast<const glm::vec3*>(tv[n]), width, c,
							*reinterpret_cast<const glm::vec3*>(tv[m]), width, c);
					}
				}
			}
		}
	}
}

void ZoneNavMeshRender::BuildOffmeshConnections(std::vector<DebugDrawLineVertex>& vertices,
	dtPolyRef base, const dtMeshTile* tile, const dtNavMeshQuery* query)
{
	ZoneRenderDebugDraw ddu(m_mgr);
	duDebugDraw* dd = &ddu;

	dd->begin(DU_DRAW_LINES, 2.0f);

	for (int i = 0; i < tile->header->polyCount; ++i)
	{
		const dtPoly* p = &tile->polys[i];
		if (p->getType() != DT_POLYTYPE_OFFMESH_CONNECTION)	// Skip regular polys.
			continue;

		unsigned int col;
		if (query && query->isInClosedList(base | static_cast<dtPolyRef>(i)))
			col = duRGBA(255, 196, 0, 220);
		else
			col = duDarkenCol(duTransCol(PolyToCol(p), 220));

		const dtOffMeshConnection* con = &tile->offMeshCons[i - tile->header->offMeshBase];
		const float* va = &tile->verts[p->verts[0] * 3];
		const float* vb = &tile->verts[p->verts[1] * 3];

		// Check to see if start and end end-points have links.
		bool startSet = false;
		bool endSet = false;
		for (unsigned int k = p->firstLink; k != DT_NULL_LINK; k = tile->links[k].next)
		{
			if (tile->links[k].edge == 0)
				startSet = true;
			if (tile->links[k].edge == 1)
				endSet = true;
		}

		// End points and their on-mesh locations.
		dd->vertex(va[0], va[1], va[2], col);
		dd->vertex(con->pos[0], con->pos[1], con->pos[2], col);
		unsigned int col2 = startSet ? col : duRGBA(220, 32, 16, 196);
		duAppendCircle(dd, con->pos[0], con->pos[1] + 0.1f, con->pos[2], con->rad, col2);

		dd->vertex(vb[0], vb[1], vb[2], col);
		dd->vertex(con->pos[3], con->pos[4], con->pos[5], col);
		col2 = endSet ? col : duRGBA(220, 32, 16, 196);
		duAppendCircle(dd, con->pos[3], con->pos[4] + 0.1f, con->pos[5], con->rad, col2);

		// End point vertices.
		dd->vertex(con->pos[0], con->pos[1], con->pos[2], duRGBA(0, 48, 64, 196));
		dd->vertex(con->pos[0], con->pos[1] + 0.2f, con->pos[2], duRGBA(0, 48, 64, 196));

		dd->vertex(con->pos[3], con->pos[4], con->pos[5], duRGBA(0, 48, 64, 196));
		dd->vertex(con->pos[3], con->pos[4] + 0.2f, con->pos[5], duRGBA(0, 48, 64, 196));

		// Connection arc.
		duAppendArc(dd, con->pos[0], con->pos[1], con->pos[2], con->pos[3], con->pos[4], con->pos[5], 0.25f,
			(con->flags & 1) ? 0.6f : 0, 0.6f, col);
	}
	dd->end();
}

void ZoneNavMeshRender::BuildBVTree(const dtMeshTile* tile)
{
	// Draw BV nodes.
	const float cs = 1.0f / tile->header->bvQuantFactor;

	//dd->begin(DU_DRAW_LINES, 1.0f);
	for (int i = 0; i < tile->header->bvNodeCount; ++i)
	{
		const dtBVNode* n = &tile->bvTree[i];
		if (n->i < 0) // Leaf indices are positive.
			continue;

		//duAppendBoxWire(dd, tile->header->bmin[0] + n->bmin[0] * cs,
		//	tile->header->bmin[1] + n->bmin[1] * cs,
		//	tile->header->bmin[2] + n->bmin[2] * cs,
		//	tile->header->bmin[0] + n->bmax[0] * cs,
		//	tile->header->bmin[1] + n->bmax[1] * cs,
		//	tile->header->bmin[2] + n->bmax[2] * cs,
		//	duRGBA(255, 255, 255, 128));
	}
	//dd->end();
}

uint32_t ZoneNavMeshRender::PolyToCol(const dtPoly* poly)
{
	if (poly)
	{
		return m_navMesh->GetPolyArea(poly->getArea()).color;
	}

	return 0;
}
