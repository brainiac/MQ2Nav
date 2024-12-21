#pragma once

#include <bgfx/bgfx.h>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <recast/DebugUtils/Include/DebugDraw.h>
#include <recast/Detour/Include/DetourNavMesh.h>
#include <unordered_map>
#include <mq/base/Signal.h>

class dtNavMesh;
class dtNavMeshQuery;
class NavMesh;
class NavMeshProject;
class ZoneInputGeometryRender;
class ZoneNavMeshRender;
class ZoneProject;
class ZoneRenderDebugDraw;
struct dtMeshTile;
struct NavMeshConfig;


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

struct DebugDrawPointVertex
{
	glm::vec4 pos;
	glm::vec4 color;
	glm::vec4 width;

	DebugDrawPointVertex(const glm::vec3& pos, float width, ImColor color)
		: pos(pos.x, pos.y, pos.z, 1.0f)
		, color(color.Value.x, color.Value.y, color.Value.z, color.Value.w)
		, width(width, 0, 0, 0)
	{}

	static void init()
	{
		ms_layout
			.begin()
				.add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
			.end();
	}

	inline static bgfx::VertexLayout ms_layout;
};

class ZoneRenderManager
{
	friend ZoneRenderDebugDraw;

public:
	ZoneRenderManager(ZoneProject* project);
	~ZoneRenderManager();

	static void InitShared();
	static void ShutdownShared();

	void DestroyObjects();
	void Render();

	void Rebuild();

	void OnNavMeshChanged(const std::shared_ptr<NavMeshProject>& navMesh);

	std::shared_ptr<NavMeshProject> GetNavMesh() const { return m_navMesh; }

	ZoneNavMeshRender* GetNavMeshRender() { return m_navMeshRender; }
	ZoneInputGeometryRender* GetInputGeoRender() { return m_zoneInputGeometry; }
	ZoneProject* GetZone() const { return m_project; }

	float GetPointSize() const {
		return m_pointSize;
	}
	void SetPointSize(float pointSize) {
		m_pointSize = pointSize;
	}

private:
	ZoneProject* m_project;
	std::shared_ptr<NavMeshProject> m_navMesh;

	ZoneInputGeometryRender* m_zoneInputGeometry = nullptr;
	ZoneNavMeshRender* m_navMeshRender = nullptr;
	float m_pointSize = 0.5f;

	std::vector<DebugDrawPointVertex> m_points;
	size_t m_lastPointsSize = 0;
	std::vector<DebugDrawLineVertex> m_lines;
	size_t m_lastLinesSize = 0;
	std::vector<DebugDrawPolyVertex> m_tris;
	size_t m_lastTrisSize = 0;
	std::vector<uint16_t> m_triIndices;
	size_t m_lastTrisIndicesSize = 0;

	bgfx::DynamicVertexBufferHandle m_ddPointsVB = BGFX_INVALID_HANDLE;
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

	void vertex(const float* pos, float width, uint32_t color);

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

	void Rebuild();

	void Render();
	void DestroyObjects();

private:
	void CreateObjects();

	ZoneRenderManager* m_mgr;

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
		DRAW_TILES            = 0x0001,
		DRAW_TILE_BOUNDARIES  = 0x0002,
		DRAW_POLY_BOUNDARIES  = 0x0004,

		DRAW_CLOSED_LIST      = 0x0008,
		DRAW_COLORED_TILES    = 0x0010,
		DRAW_POLY_VERTICES    = 0x0020,

		DRAW_BOUNDARIES       = DRAW_TILE_BOUNDARIES | DRAW_POLY_BOUNDARIES
	};

	enum PolyColor
	{
		OuterBoundary = 1,
		InnerBoundary = 2,
		Border = 3,
		TileGrid = 4,
	};

	void SetNavMesh(const std::shared_ptr<NavMeshProject>& navMesh);
	NavMeshProject* GetNavMesh() const { return m_navMesh.get(); }

	void SetNavMeshQuery(const dtNavMeshQuery* query);

	void SetFlags(uint32_t flags);
	uint32_t GetFlags() const { return m_flags; }

	void Build();
	void UpdateQuery();

	void Render();
	void DestroyObjects();

	float GetPointSize() const {
		return m_pointSize;
	}
	void SetPointSize(float pointSize) {
		m_pointSize = pointSize;
		m_dirty = true;
	}

	// Note: Need to do equivalent behavior to duDebugDrawNavMeshPoly
	uint32_t PolyToCol(const dtPoly* poly);

private:
	void BuildMeshTile(std::vector<DebugDrawPolyVertex>& vertices, std::vector<uint32_t>& indices,
		dtPolyRef base, const dtNavMesh& mesh, const dtNavMeshQuery* query, const dtMeshTile* tile, uint8_t flags);
	
	void BuildPolyBoundaries(std::vector<DebugDrawLineVertex>& vertices,
		const dtMeshTile* tile, uint32_t color, float width, bool inner);

	ZoneRenderManager* m_mgr;

	std::shared_ptr<NavMeshProject> m_navMesh;
	const dtNavMeshQuery* m_query = nullptr;
	uint32_t m_flags = 0;
	bool m_dirty = false;
	float m_pointSize = 0.5f;
	mq::Signal<>::ScopedConnection m_navMeshConn;

	bgfx::VertexBufferHandle m_tilePolysVB = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle m_indexBuffer = BGFX_INVALID_HANDLE;
	std::pair<int, int> m_tileIndices;
	bgfx::VertexBufferHandle m_lineInstances = BGFX_INVALID_HANDLE;
	int m_lineIndices = 0;
	bgfx::VertexBufferHandle m_pointsInstances = BGFX_INVALID_HANDLE;
	int m_pointsIndices = 0;
};
