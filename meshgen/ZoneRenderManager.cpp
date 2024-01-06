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

#include <glm/glm.hpp>
#include <recast/DebugUtils/Include/DebugDraw.h>
#include <recast/Detour/Include/DetourNavMeshQuery.h>
#include <recast/Detour/Include/DetourNode.h>
#include <imgui/imgui.h>

#include "DetourDebugDraw.h"


ZoneRenderManager* g_zoneRenderManager = nullptr;

static const bgfx::EmbeddedShader s_embeddedShaders[] = {
	BGFX_EMBEDDED_SHADER(fs_inputgeom),
	BGFX_EMBEDDED_SHADER(vs_inputgeom),
	BGFX_EMBEDDED_SHADER(fs_meshtile),
	BGFX_EMBEDDED_SHADER(vs_meshtile),
	BGFX_EMBEDDED_SHADER(fs_lines),
	BGFX_EMBEDDED_SHADER(vs_lines),

	BGFX_EMBEDDED_SHADER_END()
};

//============================================================================

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

	inline static bgfx::VertexLayout ms_layout;
};

struct NavMeshTileVertex
{
	glm::vec3 pos;
	uint32_t  color;

	static void init()
	{
		ms_layout
			.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();
	}

	NavMeshTileVertex(glm::vec3 pos, uint32_t color) : pos(pos), color(color) {}

	inline static bgfx::VertexLayout ms_layout;
};

struct NavMeshLineVertex
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

struct LineInstanceVertex
{
	glm::vec4 linePosWidthA;
	glm::vec4 lineColA;
	glm::vec4 linePosWidthB;
	glm::vec4 lineColB;

	LineInstanceVertex(glm::vec3 posA, float widthA, ImColor colA, glm::vec3 posB, float widthB, ImColor colB)
		: linePosWidthA(posA.x, posA.y, posA.z, widthA)
		, lineColA(colA.Value.x, colA.Value.y, colA.Value.z, colA.Value.w)
		, linePosWidthB(posB.x, posB.y, posB.z, widthB)
		, lineColB(colB.Value.x, colB.Value.y, colB.Value.z, colB.Value.w)
	{
	}

	LineInstanceVertex(glm::vec3 posA, float widthA, ImColor colA)
		: linePosWidthA(posA.x, posA.y, posA.z, widthA)
		, lineColA(colA.Value.x, colA.Value.y, colA.Value.z, colA.Value.w)
	{
	}

	static void init()
	{
		ms_layout
			.begin()
				.add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
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

	// Lines implementation
	bgfx::ProgramHandle m_linesProgram = BGFX_INVALID_HANDLE;
	bgfx::UniformHandle m_aaRadius = BGFX_INVALID_HANDLE;
	bgfx::VertexBufferHandle m_linesVBH = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle m_linesIBH = BGFX_INVALID_HANDLE;
};

static ZoneRenderShared s_shared;

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
	InputGeometryVertex::init();
	NavMeshTileVertex::init();
	NavMeshLineVertex::init();
	LineInstanceVertex::init();

	// Create the grid texture
	static constexpr int TEXTURE_SIZE = 64;
	uint32_t data[TEXTURE_SIZE * TEXTURE_SIZE];
	int size = TEXTURE_SIZE;
	int mipLevel = 0;

	s_shared.m_gridTexture = bgfx::createTexture2D(size, size, true, 1, bgfx::TextureFormat::RGBA8, 0, nullptr);
	while (size > 0)
	{
		for (int y = 0; y < size; ++y)
		{
			for (int x = 0; x < size; ++x)
				data[x + y * size] = (x == 0 || y == 0) ? 0xffd7d7d7 : 0xffffffff;
		}
		bgfx::updateTexture2D(s_shared.m_gridTexture, 0, mipLevel++, 0, 0, size, size, bgfx::copy(data, sizeof(uint32_t) * size * size));

		size /= 2;
	}

	s_shared.m_texSampler = bgfx::createUniform("textureSampler", bgfx::UniformType::Sampler);

	bgfx::RendererType::Enum type = bgfx::RendererType::Direct3D11;
	s_shared.m_inputGeoProgram = bgfx::createProgram(
		bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_inputgeom"),
		bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_inputgeom"), true);
	s_shared.m_meshTileProgram = bgfx::createProgram(
		bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_meshtile"),
		bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_meshtile"), true);

	s_shared.m_aaRadius = bgfx::createUniform("u_aa_radius", bgfx::UniformType::Vec4);
	s_shared.m_linesProgram = bgfx::createProgram(
		bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_lines"),
		bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_lines"), true);

	glm::vec3 linesQuad[] = {
		{ 0.0, -1.0, 0.0 },
		{ 0.0,  1.0, 0.0 },
		{ 1.0,  1.0, 0.0 },
		{ 1.0, -1.0, 0.0 }
	};
	s_shared.m_linesVBH = bgfx::createVertexBuffer(bgfx::copy(linesQuad, sizeof(linesQuad)), NavMeshLineVertex::ms_layout, 0);

	uint16_t linesIndices[] = {
		0, 1, 2, 0, 2, 3
	};
	s_shared.m_linesIBH = bgfx::createIndexBuffer(bgfx::copy(linesIndices, sizeof(linesIndices)), 0);

	s_shared.m_initialized = true;
}

void ZoneRenderManager::shutdown()
{
	if (s_shared.m_initialized)
	{
		bgfx::destroy(s_shared.m_gridTexture);
		bgfx::destroy(s_shared.m_texSampler);
		bgfx::destroy(s_shared.m_inputGeoProgram);
		bgfx::destroy(s_shared.m_meshTileProgram);
		bgfx::destroy(s_shared.m_linesProgram);
		bgfx::destroy(s_shared.m_aaRadius);
		bgfx::destroy(s_shared.m_linesVBH);
		bgfx::destroy(s_shared.m_linesIBH);
	}
}

void ZoneRenderManager::DestroyObjects()
{
	m_zoneInputGeometry->DestroyObjects();
	m_navMeshRender->DestroyObjects();
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
	m_vbh = bgfx::createVertexBuffer(vb, InputGeometryVertex::ms_layout);
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
	}

	float aaRadiusData[4] = { m_lineAARadius, m_lineAARadius, 0.0f, 0.0f };
	bgfx::setUniform(s_shared.m_aaRadius, aaRadiusData);

	if ((m_flags & ZoneNavMeshRender::DRAW_TILES) && isValid(m_indexBuffer) && isValid(m_tilePolysVB))
	{
		bgfx::Encoder* encoder = bgfx::begin();

		encoder->setVertexBuffer(0, m_tilePolysVB);
		encoder->setIndexBuffer(m_indexBuffer, m_tileIndices.first, m_tileIndices.second - m_tileIndices.first);
		encoder->setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);

		encoder->submit(0, s_shared.m_meshTileProgram);
	}

	if ((m_flags & ZoneNavMeshRender::DRAW_POLY_BOUNDARIES) && isValid(m_linesVB))
	{
		bgfx::Encoder* encoder = bgfx::begin();

		encoder->setVertexBuffer(0, s_shared.m_linesVBH);
		encoder->setIndexBuffer(s_shared.m_linesIBH);
		encoder->setInstanceDataBuffer(m_linesVB, m_lineIndices.first, m_lineIndices.second - m_lineIndices.first + 1);
		encoder->setState(BGFX_STATE_MSAA | BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CCW | BGFX_STATE_BLEND_ALPHA);

		encoder->submit(0, s_shared.m_linesProgram);
	}

	m_dirty = false;
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

	if (isValid(m_linesVB))
	{
		bgfx::destroy(m_linesVB);
		m_linesVB = BGFX_INVALID_HANDLE;
	}

	m_tileIndices = { 0, 0 };
	m_lineIndices = { 0, 0 };
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
	std::vector<NavMeshTileVertex> navMeshTileVertices;
	std::vector<uint32_t> navMeshTileIndices;
	std::vector<LineInstanceVertex> polyBoundaryVertices;

	size_t numMeshTileVertices = 0;
	size_t numPolyBoundaryVertices = 0;

	// Pre-calculate the number of vertices we will need
	for (int i = 0; i < mesh.getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh.getTile(i);
		if (!tile->header) continue;

		// Calculate number of vertices that we will need to store.
		for (int i = 0; i < tile->header->polyCount; ++i)
		{
			const dtPoly* p = &tile->polys[i];
			if (p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
				continue;

			// Three vertices for each triangle in the detail mesh.
			const dtPolyDetail* pd = &tile->detailMeshes[i];
			numMeshTileVertices += pd->triCount * 3;
		}

		for (int i = 0; i < tile->header->polyCount; ++i)
		{
			const dtPoly* p = &tile->polys[i];
			if (p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
				continue;

			// Three vertices for each triangle in the detail mesh.
			const dtPolyDetail* pd = &tile->detailMeshes[i];
			numPolyBoundaryVertices += pd->triCount * 3 * 2;
		}
		numPolyBoundaryVertices += tile->header->vertCount;
	}

	navMeshTileIndices.reserve(numMeshTileVertices);
	navMeshTileIndices.reserve(numMeshTileVertices);
	polyBoundaryVertices.reserve(numPolyBoundaryVertices);

	// Build tile polygons
	for (int i = 0; i < mesh.getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = mesh.getTile(i);
		if (!tile->header) continue;

		dtPolyRef base = mesh.getPolyRefBase(tile);

		BuildMeshTile(navMeshTileVertices, navMeshTileIndices, base, mesh, q, tile, m_flags);

		BuildPolyBoundaries(polyBoundaryVertices, mesh, q, tile, m_flags);

		if (m_flags & ZoneNavMeshRender::DRAW_OFFMESH_CONNS)
		{
			// Draw offmesh connections
			BuildOffmeshConnections(base, tile, q);
		}
	}

	// Create vertex buffer from gathered data.
	m_tilePolysVB = bgfx::createVertexBuffer(bgfx::copy(navMeshTileVertices.data(), (uint32_t)navMeshTileVertices.size() * NavMeshTileVertex::ms_layout.getStride()),
		NavMeshTileVertex::ms_layout);
	m_linesVB = bgfx::createVertexBuffer(bgfx::copy(polyBoundaryVertices.data(), (uint32_t)polyBoundaryVertices.size() * LineInstanceVertex::ms_layout.getStride()),
		LineInstanceVertex::ms_layout);

	// Copy poly boundary indices into tile indices
	m_tileIndices = { 0, (uint32_t)navMeshTileIndices.size() };
	m_lineIndices = { 0, (uint32_t)polyBoundaryVertices.size()};
	m_indexBuffer = bgfx::createIndexBuffer(bgfx::copy(navMeshTileIndices.data(), (uint32_t)(navMeshTileIndices.size() * sizeof(uint32_t))), BGFX_BUFFER_INDEX32);
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

void ZoneNavMeshRender::BuildMeshTile(std::vector<NavMeshTileVertex>& vertices, std::vector<uint32_t>& indices,
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

void ZoneNavMeshRender::BuildPolyBoundaries(std::vector<LineInstanceVertex>& vertices,
	const dtNavMesh& mesh, const dtNavMeshQuery* query, const dtMeshTile* tile, uint8_t flags)
{
	// Based on drawMeshTile from DetourDebugDraw

	// Draw inner poly boundaries
	BuildPolyBoundaries(vertices, tile, duRGBA(0, 48, 64, 32), 1.5, true);

	// Draw outer poly boundaries
	BuildPolyBoundaries(vertices, tile, duRGBA(0, 48, 64, 220), 2.5f, false);

	// Build points. Points will be treated as a special version of lines, with only the first vertex populated, but we'll use a different shader
	//const uint32_t vcol = duRGBA(0, 0, 0, 196);
	//for (int i = 0; i < tile->header->vertCount; ++i)
	//{
	//	const float* v = &tile->verts[i * 3];
	//	pVertex[pointIndex++] = LineInstanceVertex(*reinterpret_cast<const glm::vec3*>(v), 5.0f, vcol);
	//}
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

void ZoneNavMeshRender::BuildPolyBoundaries(std::vector<LineInstanceVertex>& vertices,
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

void ZoneNavMeshRender::BuildOffmeshConnections(dtPolyRef base, const dtMeshTile* tile, const dtNavMeshQuery* query)
{
	//dd->begin(DU_DRAW_LINES, 2.0f);
	for (int i = 0; i < tile->header->polyCount; ++i)
	{
		const dtPoly* p = &tile->polys[i];
		if (p->getType() != DT_POLYTYPE_OFFMESH_CONNECTION)	// Skip regular polys.
			continue;

		unsigned int col, col2;
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
		//dd->vertex(va[0], va[1], va[2], col);
		//dd->vertex(con->pos[0], con->pos[1], con->pos[2], col);
		col2 = startSet ? col : duRGBA(220, 32, 16, 196);
		//duAppendCircle(dd, con->pos[0], con->pos[1] + 0.1f, con->pos[2], con->rad, col2);

		//dd->vertex(vb[0], vb[1], vb[2], col);
		//dd->vertex(con->pos[3], con->pos[4], con->pos[5], col);
		col2 = endSet ? col : duRGBA(220, 32, 16, 196);
		//duAppendCircle(dd, con->pos[3], con->pos[4] + 0.1f, con->pos[5], con->rad, col2);

		// End point vertices.
		//dd->vertex(con->pos[0], con->pos[1], con->pos[2], duRGBA(0, 48, 64, 196));
		//dd->vertex(con->pos[0], con->pos[1] + 0.2f, con->pos[2], duRGBA(0, 48, 64, 196));

		//dd->vertex(con->pos[3], con->pos[4], con->pos[5], duRGBA(0, 48, 64, 196));
		//dd->vertex(con->pos[3], con->pos[4] + 0.2f, con->pos[5], duRGBA(0, 48, 64, 196));

		// Connection arc.
		//duAppendArc(dd, con->pos[0], con->pos[1], con->pos[2], con->pos[3], con->pos[4], con->pos[5], 0.25f,
		//	(con->flags & 1) ? 0.6f : 0, 0.6f, col);
	}
	//dd->end();
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
