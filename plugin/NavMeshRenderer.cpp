//
// NavMeshRenderer.cpp
//

#include "pch.h"
#include "NavMeshRenderer.h"

#include "common/NavMesh.h"
#include "plugin/MQ2Navigation.h"
#include "plugin/PluginSettings.h"
#include "plugin/RenderHandler.h"
#include "plugin/DebugDrawDX.h"
#include "plugin/NavMeshLoader.h"

#include <cassert>
#include <DetourDebugDraw.h>
#include <RecastDebugDraw.h>
#include <DetourNavMesh.h>
#include <DebugDraw.h>

#include <imgui.h>

#include <glm/glm.hpp>

//----------------------------------------------------------------------------

class NavMeshDebugDraw : public DebugDrawDX
{
public:
	NavMeshDebugDraw(NavMeshRenderer* renderer, RenderGroup* grp)
		: DebugDrawDX(grp)
		, m_renderer(renderer) {}

	virtual unsigned int polyToCol(const dtPoly* poly) override
	{
		if (poly)
		{
			return m_renderer->GetColorForPolyArea(poly->getArea());
		}

		return 0;
	}

private:
	NavMeshRenderer* m_renderer = nullptr;
};

//----------------------------------------------------------------------------

NavMeshRenderer::NavMeshRenderer()
{
}

NavMeshRenderer::~NavMeshRenderer()
{
	CleanupObjects();

	// Destruction might occur before Shutdown() is called from the plugin if
	// the application exits unexpectedly.
	Shutdown();
}

void NavMeshRenderer::Initialize()
{
	m_navMesh = g_mq2Nav->Get<NavMesh>();

	m_meshConn = m_navMesh->OnNavMeshChanged.Connect(
		[this]() { UpdateNavMesh(); });

	g_renderHandler->AddRenderable(this);

	m_initialized = true;
}

void NavMeshRenderer::Shutdown()
{
	if (!m_initialized)
	{
		return;
	}

	g_renderHandler->RemoveRenderable(this);

	m_initialized = false;
}

//----------------------------------------------------------------------------

void NavMeshRenderer::CleanupObjects()
{
	StopLoad();

	if (m_primGroup)
	{
		m_primGroup->Reset();
		m_primGroup->InvalidateDeviceObjects();
	}
}

void NavMeshRenderer::InvalidateDeviceObjects()
{
	CleanupObjects();
}

bool NavMeshRenderer::CreateDeviceObjects()
{
	if (m_primGroup)
	{
		return m_primGroup->CreateDeviceObjects();
	}

	return true;
}

void NavMeshRenderer::Render()
{
	if (m_enabled != m_loaded)
	{
		if (m_enabled)
		{
			UpdateNavMesh();
		}
		else
		{
			CleanupObjects();
			InvalidateDeviceObjects();
		}

		m_loaded = m_enabled;
	}

	if (!m_loaded || m_loading || !m_primGroup || !gpD3D9Device)
		return;

	m_primGroup->Render();
}

//----------------------------------------------------------------------------

void NavMeshRenderer::StopLoad()
{
	if (m_loadThread.joinable())
	{
		m_stopLoading = true;
		m_loadThread.join();
	}
}

void NavMeshRenderer::StartLoad()
{
	m_stopLoading = false;
	m_loading = true;
	int tilesToLoad = 0;

	if (!m_navMesh->IsNavMeshLoaded() || !gpD3D9Device)
		return;

	std::shared_ptr<const dtNavMesh> navMesh =
		std::static_pointer_cast<const dtNavMesh>(m_navMesh->GetNavMesh());

	m_primGroup = std::make_unique<RenderGroup>();

	std::shared_ptr<NavMeshDebugDraw> dd = std::make_shared<NavMeshDebugDraw>(
		this, m_primGroup.get());

	for (int i = 0; i < navMesh->getMaxTiles(); ++i)
	{
		const dtMeshTile* tile = navMesh->getTile(i);
		if (!tile->header) continue;

		tilesToLoad++;
	}

	auto loadingThread = [this, dd, tilesToLoad, navMesh]() mutable
	{
		int currentTile = 0;

		for (int i = 0; i < navMesh->getMaxTiles() && !m_stopLoading; ++i)
		{
			const dtMeshTile* tile = navMesh->getTile(i);
			if (!tile->header) continue;

			drawMeshTile(dd.get(), *navMesh, 0, tile, DU_DRAWNAVMESH_OFFMESHCONS | DU_DRAWNAVMESH_CLOSEDLIST
				/* | DU_DRAWNAVMESH_COLOR_TILES*/);

			++currentTile;
			m_progress = static_cast<float>(currentTile) / static_cast<float>(tilesToLoad);
		}

		m_loading = false;
	};

	m_loadThread = std::thread(loadingThread);
}

void NavMeshRenderer::UpdateNavMesh()
{
	StopLoad();

	// if not enabled, don't do any work
	if (!m_enabled)
		return;

	if (m_primGroup)
	{
		m_primGroup->Reset();
	}

	// if we don't have a navmesh, don't build the geometry
	if (!m_navMesh->IsNavMeshLoaded())
	{
		InvalidateDeviceObjects();
		return;
	}

	StartLoad();
}

void NavMeshRenderer::OnUpdateUI()
{
	if (!m_navMesh->IsNavMeshLoaded())
		ImGui::TextColored(ImColor(255, 255, 0), "No navmesh loaded");
	else
		ImGui::TextColored(ImColor(0, 255, 0), "Navmesh loaded");

	ImGui::SameLine();

	if (ImGui::Button("Reload"))
	{
		g_mq2Nav->Get<NavMeshLoader>()->LoadNavMesh();
	}

	ImGui::Checkbox("Render Navigation Mesh", &m_enabled);

	if (m_enabled && m_loading)
	{
		ImGui::ProgressBar(m_progress);
	}

#if 0
	ImGui::Columns(2);
	ImGui::Checkbox("Points", &m_primGroup->GetPrimsEnabled()[RenderList::Prim_Points]);
	ImGui::NextColumn();
	ImGui::Checkbox("Lines", &m_primGroup->GetPrimsEnabled()[RenderList::Prim_Lines]);
	ImGui::NextColumn();
	ImGui::Checkbox("Triangles", &m_primGroup->GetPrimsEnabled()[RenderList::Prim_Triangles]);
	ImGui::NextColumn();
	ImGui::Checkbox("Quads", &m_primGroup->GetPrimsEnabled()[RenderList::Prim_Quads]);
	ImGui::NextColumn();
	ImGui::Columns(1);
#endif
}

unsigned int NavMeshRenderer::GetColorForPolyArea(uint8_t areaType)
{
	if (m_navMesh)
	{
		return m_navMesh->GetPolyArea(areaType).color;
	}

	return 0;
}
