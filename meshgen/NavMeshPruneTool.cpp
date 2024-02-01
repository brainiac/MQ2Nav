//
// NavMeshPruneTool.cpp
//

#include "meshgen/NavMeshPruneTool.h"
#include "meshgen/NavMeshTool.h"
#include "common/NavMeshData.h"

#include <DetourNavMesh.h>
#include <DetourCommon.h>
#include <DetourAssert.h>
#include <DetourDebugDraw.h>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <vector>

//-----------------------------------------------------------------------------

class NavmeshFlags
{
	struct TileFlags
	{
		inline void purge() { dtFree(flags); }
		unsigned char* flags;
		int nflags;
		dtPolyRef base;
	};

	const dtNavMesh* m_nav;
	TileFlags* m_tiles;
	int m_ntiles;

public:
	NavmeshFlags() :
		m_nav(0), m_tiles(0), m_ntiles(0)
	{
	}

	~NavmeshFlags()
	{
		for (int i = 0; i < m_ntiles; ++i)
			m_tiles[i].purge();
		dtFree(m_tiles);
	}

	bool init(const dtNavMesh* nav)
	{
		m_ntiles = nav->getMaxTiles();
		if (!m_ntiles)
			return true;
		m_tiles = (TileFlags*)dtAlloc(sizeof(TileFlags)*m_ntiles, DT_ALLOC_TEMP);
		if (!m_tiles)
		{
			return false;
		}
		memset(m_tiles, 0, sizeof(TileFlags)*m_ntiles);

		// Alloc flags for each tile.
		for (int i = 0; i < nav->getMaxTiles(); ++i)
		{
			const dtMeshTile* tile = nav->getTile(i);
			if (!tile->header) continue;
			TileFlags* tf = &m_tiles[i];
			tf->nflags = tile->header->polyCount;
			tf->base = nav->getPolyRefBase(tile);
			if (tf->nflags)
			{
				tf->flags = (unsigned char*)dtAlloc(tf->nflags, DT_ALLOC_TEMP);
				if (!tf->flags)
					return false;
				memset(tf->flags, 0, tf->nflags);
			}
		}

		m_nav = nav;

		return false;
	}

	inline void clearAllFlags()
	{
		for (int i = 0; i < m_ntiles; ++i)
		{
			TileFlags* tf = &m_tiles[i];
			if (tf->nflags)
				memset(tf->flags, 0, tf->nflags);
		}
	}

	inline unsigned char getFlags(dtPolyRef ref)
	{
		dtAssert(m_nav);
		dtAssert(m_ntiles);
		// Assume the ref is valid, no bounds checks.
		unsigned int salt, it, ip;
		m_nav->decodePolyId(ref, salt, it, ip);
		return m_tiles[it].flags[ip];
	}

	inline void setFlags(dtPolyRef ref, unsigned char flags)
	{
		dtAssert(m_nav);
		dtAssert(m_ntiles);
		// Assume the ref is valid, no bounds checks.
		unsigned int salt, it, ip;
		m_nav->decodePolyId(ref, salt, it, ip);
		m_tiles[it].flags[ip] = flags;
	}
};

static void floodNavmesh(dtNavMesh* nav, NavmeshFlags* flags, dtPolyRef start, uint8_t flag)
{
	// If already visited, skip.
	if (flags->getFlags(start))
		return;

	std::vector<dtPolyRef> openList;
	openList.push_back(start);

	while (!openList.empty())
	{
		const dtPolyRef ref = openList.back();
		openList.pop_back();

		// Get current poly and tile.
		// The API input has been checked already, skip checking internal data.
		const dtMeshTile* tile = nullptr;
		const dtPoly* poly = nullptr;
		nav->getTileAndPolyByRefUnsafe(ref, &tile, &poly);

		// Visit linked polygons.
		for (auto i = poly->firstLink; i != DT_NULL_LINK; i = tile->links[i].next)
		{
			const dtPolyRef neiRef = tile->links[i].ref;

			// Skip invalid and already visited.
			if (!neiRef || flags->getFlags(neiRef))
				continue;

			// Mark as visited
			flags->setFlags(neiRef, flag);

			// Visit neighbors
			openList.push_back(neiRef);
		}
	}
}

static void disableUnvisitedPolys(dtNavMesh* nav, NavmeshFlags* flags)
{
	for (int i = 0; i < nav->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = const_cast<const dtNavMesh*>(nav)->getTile(i);

		if (!tile->header) continue;
		const dtPolyRef base = nav->getPolyRefBase(tile);
		for (int j = 0; j < tile->header->polyCount; ++j)
		{
			const dtPolyRef ref = base | (unsigned int)j;
			if (!flags->getFlags(ref))
			{
				uint16_t f = 0;
				nav->getPolyFlags(ref, &f);
				nav->setPolyFlags(ref, f | +PolyFlags::Disabled);
			}
		}
	}
}

//----------------------------------------------------------------------------

NavMeshPruneTool::NavMeshPruneTool()
{
}

NavMeshPruneTool::~NavMeshPruneTool()
{
}

void NavMeshPruneTool::init(NavMeshTool* meshtool)
{
	m_meshTool = meshtool;
}

void NavMeshPruneTool::reset()
{
	m_hitPosSet = false;
	m_flags.reset();
}

void NavMeshPruneTool::handleMenu()
{
	auto nav = m_meshTool->GetNavMesh()->GetNavMesh();
	if (!nav) return;
	if (!m_flags) return;

	if (ImGui::Button("Clear Selection"))
	{
		m_flags->clearAllFlags();
	}

	if (ImGui::Button("Prune Unselected"))
	{
		disableUnvisitedPolys(nav.get(), m_flags.get());
		m_flags.reset();
	}
}

void NavMeshPruneTool::handleClick(const glm::vec3& p, bool /*shift*/)
{
	if (!m_meshTool)
		return;
	if (m_meshTool->GetZoneContext())
		return;
	auto nav = m_meshTool->GetNavMesh()->GetNavMesh();
	if (!nav) return;
	auto query = m_meshTool->GetNavMesh()->GetNavMeshQuery();
	if (!query) return;

	m_hitPos = p;
	m_hitPosSet = true;

	if (!m_flags)
	{
		m_flags = std::make_unique<NavmeshFlags>();
		m_flags->init(nav.get());
	}

	const float ext[3] = { 2,4,2 };
	dtQueryFilter filter;
	dtPolyRef ref = 0;
	query->findNearestPoly(glm::value_ptr(p), ext, &filter, &ref, 0);

	floodNavmesh(nav.get(), m_flags.get(), ref, 1);
}

void NavMeshPruneTool::handleRender()
{
	ZoneRenderDebugDraw dd(g_zoneRenderManager);

	if (m_hitPosSet)
	{
		const float s = m_meshTool->GetNavMesh()->GetNavMeshConfig().agentRadius;
		const unsigned int col = duRGBA(255, 255, 255, 255);
		dd.begin(DU_DRAW_LINES);
		dd.vertex(m_hitPos[0] - s, m_hitPos[1], m_hitPos[2], col);
		dd.vertex(m_hitPos[0] + s, m_hitPos[1], m_hitPos[2], col);
		dd.vertex(m_hitPos[0], m_hitPos[1] - s, m_hitPos[2], col);
		dd.vertex(m_hitPos[0], m_hitPos[1] + s, m_hitPos[2], col);
		dd.vertex(m_hitPos[0], m_hitPos[1], m_hitPos[2] - s, col);
		dd.vertex(m_hitPos[0], m_hitPos[1], m_hitPos[2] + s, col);
		dd.end();
	}

	const dtNavMesh* nav = m_meshTool->GetNavMesh()->GetNavMesh().get();
	if (m_flags && nav)
	{
		for (int i = 0; i < nav->getMaxTiles(); ++i)
		{
			const dtMeshTile* tile = nav->getTile(i);
			if (!tile->header) continue;
			const dtPolyRef base = nav->getPolyRefBase(tile);
			for (int j = 0; j < tile->header->polyCount; ++j)
			{
				const dtPolyRef ref = base | (unsigned int)j;
				if (m_flags->getFlags(ref))
				{
					duDebugDrawNavMeshPoly(&dd, *nav, ref, duRGBA(255, 255, 255, 128));
				}
			}
		}
	}
}

void NavMeshPruneTool::handleRenderOverlay()
{
	// Tool help
	ImGui::TextColored(ImVec4(255, 255, 255, 192),
		"LMB: Click to fill area.");
}
