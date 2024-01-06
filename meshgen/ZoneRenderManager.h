#pragma once

#include <unordered_map>
#include <recast/Detour/Include/DetourNavMesh.h>
#include <bgfx/bgfx.h>

#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <recast/DebugUtils/Include/DebugDraw.h>

class InputGeom;
class ZoneInputGeometryRender;
class ZoneNavMeshRender;
class ZoneNavMeshTileRender;
struct NavMeshConfig;
class NavMesh;
class dtNavMeshQuery;
class dtNavMesh;
struct dtMeshTile;

struct DebugDrawGridTexturedVertex
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

struct DebugDrawPolyVertex
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

	DebugDrawPolyVertex(glm::vec3 pos, uint32_t color) : pos(pos), color(color) {}

	inline static bgfx::VertexLayout ms_layout;
};


struct DebugDrawLineVertex
{
	glm::vec4 linePosWidthA;
	glm::vec4 lineColA;
	glm::vec4 linePosWidthB;
	glm::vec4 lineColB;

	DebugDrawLineVertex(glm::vec3 posA, float widthA, ImColor colA, glm::vec3 posB, float widthB, ImColor colB)
		: linePosWidthA(posA.x, posA.y, posA.z, widthA)
		, lineColA(colA.Value.x, colA.Value.y, colA.Value.z, colA.Value.w)
		, linePosWidthB(posB.x, posB.y, posB.z, widthB)
		, lineColB(colB.Value.x, colB.Value.y, colB.Value.z, colB.Value.w)
	{
	}

	DebugDrawLineVertex(glm::vec3 posA, float widthA, ImColor colA)
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

class ZoneRenderDebugDraw;

class ZoneRenderManager
{
	friend ZoneRenderDebugDraw;

public:
	ZoneRenderManager();
	~ZoneRenderManager();

	static void init();
	static void shutdown();

	void DestroyObjects();
	void Render();

	void SetInputGeometry(InputGeom* geom);

	void SetNavMeshConfig(const NavMeshConfig* config);
	const NavMeshConfig* GetNavMeshConfig() const { return m_meshConfig; }

	ZoneNavMeshRender* GetNavMeshRender() { return m_navMeshRender; }
	ZoneInputGeometryRender* GetInputGeoRender() { return m_zoneInputGeometry; }

private:
	ZoneInputGeometryRender* m_zoneInputGeometry = nullptr;
	ZoneNavMeshRender* m_navMeshRender = nullptr;
	const NavMeshConfig* m_meshConfig = nullptr;

	std::vector<DebugDrawLineVertex> m_lines;
	size_t m_lastLinesSize = 0;
	std::vector<DebugDrawPolyVertex> m_tris;
	size_t m_lastTrisSize = 0;
	std::vector<uint16_t> m_triIndices;
	size_t m_lastTrisIndicesSize = 0;

	bgfx::DynamicVertexBufferHandle m_ddLinesVB = BGFX_INVALID_HANDLE;
	bgfx::DynamicVertexBufferHandle m_ddTrisVB = BGFX_INVALID_HANDLE;
	bgfx::DynamicIndexBufferHandle m_ddIndexBuffer = BGFX_INVALID_HANDLE;
};

extern ZoneRenderManager* g_zoneRenderManager;

class ZoneRenderDebugDraw : public duDebugDraw
{
public:
	ZoneRenderDebugDraw(ZoneRenderManager* render);
	virtual ~ZoneRenderDebugDraw() override;

	virtual void depthMask(bool) override {}
	virtual void texture(bool) override {}

	virtual void begin(duDebugDrawPrimitives type, float size = 1.0f) override;
	virtual void end() override;

	virtual void vertex(const float* pos_, unsigned int color) override;
	virtual void vertex(const float x, const float y, const float z, unsigned int color) override;

	virtual void vertex(const float* pos, unsigned int color, const float* uv) override {}
	virtual void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v) override {}

	virtual unsigned int polyToCol(const dtPoly* poly) override;

private:
	ZoneRenderManager* m_render;

	// last pos and color
	duDebugDrawPrimitives m_type = DU_DRAW_POINTS;
	glm::vec3 m_lastPos{ 0, 0, 0 };
	unsigned int m_lastColor = 0;
	bool m_first = false;
	float m_width = 1.0f;
	int m_numVertices = 0;
};


class ZoneInputGeometryRender
{
public:
	explicit ZoneInputGeometryRender(ZoneRenderManager* manager);
	~ZoneInputGeometryRender();

	void SetInputGeometry(InputGeom* geom);

	void Render();
	void DestroyObjects();

private:
	void CreateObjects();

	ZoneRenderManager* m_mgr;
	InputGeom* m_geom = nullptr;

	bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle m_ibh = BGFX_INVALID_HANDLE;
	int m_numIndices = 0;
};

struct DebugDrawLineVertex;
struct DebugDrawPolyVertex;

class ZoneNavMeshRender
{
public:
	explicit ZoneNavMeshRender(ZoneRenderManager* manager);
	~ZoneNavMeshRender();

	enum Flags
	{
		DRAW_BV_TREE          = 0x0001,
		DRAW_OFFMESH_CONNS    = 0x0002,
		DRAW_CLOSED_LIST      = 0x0004,
		DRAW_COLORED_TILES    = 0x0008,
		DRAW_POLY_BOUNDARIES  = 0x0010,
		DRAW_TILES            = 0x0020,
	};

	void SetNavMesh(const std::shared_ptr<NavMesh>& navMesh);
	NavMesh* GetNavMesh() const { return m_navMesh.get(); }

	void SetNavMeshQuery(const dtNavMeshQuery* query);

	void SetFlags(uint32_t flags);
	uint32_t GetFlags() const { return m_flags; }

	void SetLineAARadius(float radius) { m_lineAARadius = radius; }
	float GetLineAARadius() const { return m_lineAARadius; }

	void Build();
	void UpdateQuery();

	void Render();
	void DestroyObjects();

	// Note: Need to do equivalent behavior to duDebugDrawNavMeshPoly
	uint32_t PolyToCol(const dtPoly* poly);

private:
	void BuildMeshTile(std::vector<DebugDrawPolyVertex>& vertices, std::vector<uint32_t>& indices,
		dtPolyRef base, const dtNavMesh& mesh, const dtNavMeshQuery* query, const dtMeshTile* tile, uint8_t flags);
	
	static void BuildPolyBoundaries(std::vector<DebugDrawLineVertex>& vertices,
		const dtMeshTile* tile, uint32_t color, float width, bool inner);
	void BuildOffmeshConnections(std::vector<DebugDrawLineVertex>& vertices, dtPolyRef base, const dtMeshTile* tile, const dtNavMeshQuery* query);
	void BuildBVTree(const dtMeshTile* tile);

	ZoneRenderManager* m_mgr;

	std::shared_ptr<NavMesh> m_navMesh;
	const dtNavMeshQuery* m_query = nullptr;
	uint32_t m_flags = 0;
	bool m_dirty = false;
	float m_lineAARadius = 0.0f;

	bgfx::VertexBufferHandle m_tilePolysVB = BGFX_INVALID_HANDLE;
	bgfx::VertexBufferHandle m_linesVB = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle m_indexBuffer = BGFX_INVALID_HANDLE;
	std::pair<int, int> m_tileIndices;
	std::pair<int, int> m_lineIndices;

	bgfx::VertexBufferHandle m_debugDrawVB = BGFX_INVALID_HANDLE;
	std::pair<int, int> m_debugIndices;
};