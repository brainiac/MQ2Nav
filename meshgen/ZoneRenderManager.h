#pragma once

#include <unordered_map>
#include <recast/Detour/Include/DetourNavMesh.h>
#include <bgfx/bgfx.h>

class InputGeom;
class ZoneInputGeometryRender;
class ZoneNavMeshRender;
class ZoneNavMeshTileRender;
struct NavMeshConfig;
class NavMesh;
class dtNavMeshQuery;
class dtNavMesh;
struct dtMeshTile;

class ZoneRenderManager
{
public:
	ZoneRenderManager();
	~ZoneRenderManager();

	static void init();
	static void shutdown();

	void DestroyObjects();

	void SetInputGeometry(InputGeom* geom);

	void SetNavMeshConfig(const NavMeshConfig* config);
	const NavMeshConfig* GetNavMeshConfig() const { return m_meshConfig; }

	ZoneNavMeshRender* GetNavMeshRender() { return m_navMeshRender; }
	ZoneInputGeometryRender* GetInputGeoRender() { return m_zoneInputGeometry; }

private:
	ZoneInputGeometryRender* m_zoneInputGeometry = nullptr;
	ZoneNavMeshRender* m_navMeshRender = nullptr;
	const NavMeshConfig* m_meshConfig = nullptr;
};

extern ZoneRenderManager* g_zoneRenderManager;

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

struct LineInstanceVertex;
struct NavMeshTileVertex;

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

private:
	void BuildMeshTile(std::vector<NavMeshTileVertex>& vertices, std::vector<uint32_t>& indices,
		dtPolyRef base, const dtNavMesh& mesh, const dtNavMeshQuery* query, const dtMeshTile* tile, uint8_t flags);

	//void BuildNodes();
	void BuildPolyBoundaries(std::vector<LineInstanceVertex>& vertices,
		const dtNavMesh& mesh, const dtNavMeshQuery* query, const dtMeshTile* tile, uint8_t flags);
	void BuildPolyBoundaries(std::vector<LineInstanceVertex>& vertices,
		const dtMeshTile* tile, uint32_t color, float width, bool inner);
	void BuildOffmeshConnections(dtPolyRef base, const dtMeshTile* tile, const dtNavMeshQuery* query);
	void BuildBVTree(const dtMeshTile* tile);

	uint32_t PolyToCol(const dtPoly* poly);

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
};
