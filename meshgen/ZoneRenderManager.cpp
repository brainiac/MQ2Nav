#include "ZoneRenderManager.h"

#include "InputGeom.h"
#include "common/NavMeshData.h"
#include "engine/embedded_shader.h"
#include "shaders/navmesh/fs_inputgeom.bin.h"
#include "shaders/navmesh/vs_inputgeom.bin.h"

#include <glm/glm.hpp>
#include <recast/DebugUtils/Include/DebugDraw.h>


static const bgfx::EmbeddedShader s_embeddedShaders[] = {
	BGFX_EMBEDDED_SHADER(fs_inputgeom),
	BGFX_EMBEDDED_SHADER(vs_inputgeom),

	BGFX_EMBEDDED_SHADER_END()
};

ZoneRenderManager::ZoneRenderManager()
{
	m_zoneInputGeometry = new ZoneInputGeometryRender(this);
}

ZoneRenderManager::~ZoneRenderManager()
{
	delete m_zoneInputGeometry;
	m_zoneInputGeometry = nullptr;
}

void ZoneRenderManager::init()
{
	ZoneInputGeometryRender::init();

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
}

void ZoneRenderManager::Render()
{
	m_zoneInputGeometry->Render();
}

void ZoneRenderManager::DestroyObjects()
{
	m_zoneInputGeometry->DestroyObjects();

	if (isValid(m_gridTexture))
	{
		bgfx::destroy(m_gridTexture);
		m_gridTexture = BGFX_INVALID_HANDLE;
	}
}

void ZoneRenderManager::SetInputGeometry(InputGeom* geom)
{
	m_zoneInputGeometry->SetInputGeometry(geom);
}

void ZoneRenderManager::SetNavMeshConfig(const NavMeshConfig* config)
{
	m_meshConfig = config;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

struct InputGeometryVertex
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

bgfx::VertexLayout InputGeometryVertex::ms_layout;

//----------------------------------------------------------------------------

/* static */
void ZoneInputGeometryRender::init()
{
	InputGeometryVertex::init();
}

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
	const bgfx::Memory* vb = bgfx::alloc(ntris * 3 * InputGeometryVertex::ms_layout.getStride());
	const bgfx::Memory* ib = bgfx::alloc(ntris * 3 * sizeof(uint32_t));

	InputGeometryVertex* pVertex = reinterpret_cast<InputGeometryVertex*>(vb->data);
	uint32_t* pIndex = reinterpret_cast<uint32_t*>(ib->data);

	for (int index = 0; index < ntris * 3; index += 3)
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

	m_vbh = bgfx::createVertexBuffer(vb, InputGeometryVertex::ms_layout);
	m_ibh = bgfx::createIndexBuffer(ib, BGFX_BUFFER_INDEX32);

	bgfx::RendererType::Enum type = bgfx::RendererType::Direct3D11;
	m_program = bgfx::createProgram(
		bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_inputgeom"),
		bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_inputgeom"), true);

	m_texSampler = bgfx::createUniform("textureSampler", bgfx::UniformType::Sampler);
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

	if (isValid(m_program))
	{
		bgfx::destroy(m_program);
		m_program = BGFX_INVALID_HANDLE;
	}

	if (isValid(m_texSampler))
	{
		bgfx::destroy(m_texSampler);
		m_texSampler = BGFX_INVALID_HANDLE;
	}
}

void ZoneInputGeometryRender::Render()
{
	if (isValid(m_ibh) && isValid(m_vbh))
	{
		bgfx::Encoder* encoder = bgfx::begin();

		encoder->setVertexBuffer(0, m_vbh);
		encoder->setIndexBuffer(m_ibh);
		encoder->setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
		encoder->setTexture(0, m_texSampler, m_mgr->GetGridTexture());

		encoder->submit(0, m_program);
	}
}
