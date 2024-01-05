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
	void BuildNodes();

	ZoneRenderManager* m_mgr;

	std::shared_ptr<NavMesh> m_navMesh;
	const dtNavMeshQuery* m_query = nullptr;
	uint32_t m_flags = 0;
	bool m_dirty = false;
	float m_lineAARadius = 2.0f;

	std::vector<std::unique_ptr<ZoneNavMeshTileRender>> m_tiles;
};

struct LineInstanceVertex;

class ZoneNavMeshTileRender
{
public:
	explicit ZoneNavMeshTileRender(ZoneNavMeshRender* parent);
	~ZoneNavMeshTileRender();

	void Render(uint32_t flags);

	void Build(const dtNavMesh& mesh, const dtNavMeshQuery* query, const dtMeshTile* tile, uint8_t flags);

private:
	void BuildMeshTile(dtPolyRef base, const dtNavMesh& mesh, const dtNavMeshQuery* query, const dtMeshTile* tile, uint8_t flags);
	void BuildPolyBoundaries(const dtMeshTile* tile);
	void BuildPolyBoundaries(const dtMeshTile* tile, LineInstanceVertex* pOutVertex, int& currIndex, uint32_t color, float width, bool inner);
	void BuildOffmeshConnections(dtPolyRef base, const dtMeshTile* tile, const dtNavMeshQuery* query);
	void BuildBVTree(const dtMeshTile* tile);
	//void BuildPortals();

	uint32_t PolyToCol(const dtPoly* poly);

	ZoneNavMeshRender* m_parent;

	// NavMesh Tile Polys
	bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle m_ibh = BGFX_INVALID_HANDLE;
	int m_numIndices = 0;

	// Poly Boundary lines
	bgfx::VertexBufferHandle m_lineInstances = BGFX_INVALID_HANDLE;
	std::pair<int, int> m_lineIndices;
	std::pair<int, int> m_pointIndices;
};
