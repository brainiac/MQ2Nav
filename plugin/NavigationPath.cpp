//
// NavigationPath.cpp
//

#include "NavigationPath.h"

#include "common/NavMesh.h"
#include "common/NavMeshData.h"
#include "common/Utilities.h"
#include "plugin/DebugDrawDX.h"
#include "plugin/EmbedResources.h"
#include "plugin/MQ2Navigation.h"
#include "plugin/PluginSettings.h"
#include "plugin/NavMeshLoader.h"
#include "plugin/RenderHandler.h"

#include <DetourNavMesh.h>
#include <DetourCommon.h>
#include <glm/gtc/type_ptr.hpp>

//----------------------------------------------------------------------------
// constants

const int MAX_STRAIGHT_PATH_LENGTH = 16384;

//----------------------------------------------------------------------------

void StraightPath::Reset(int size_)
{
	cursor = 0;
	length = 0;

	if (alloc == size_)
		return;

	alloc = size_;

	if (alloc == 0)
	{
		verts.reset();
		flags.reset();
		polys.reset();
	}
	else
	{
		verts = std::make_unique<glm::vec3[]>(alloc);
		flags = std::make_unique<uint8_t[]>(alloc);
		polys = std::make_unique<dtPolyRef[]>(alloc);
	}
}

//----------------------------------------------------------------------------

NavigationPath::NavigationPath(const std::shared_ptr<DestinationInfo>& dest)
	: m_currentPath(std::make_unique<StraightPath>())
{
	auto* mesh = g_mq2Nav->Get<NavMesh>();
	m_navMeshConn = mesh->OnNavMeshChanged.Connect(
		[this, mesh]() { SetNavMesh(mesh->GetNavMesh()); });
	
	SetNavMesh(mesh->GetNavMesh(), false);

	SetDestination(dest);
}

NavigationPath::~NavigationPath()
{
	SetShowNavigationPaths(false);
}

//----------------------------------------------------------------------------

void NavigationPath::SetShowNavigationPaths(bool renderPaths)
{
	if (m_line)
		m_line->SetVisible(renderPaths);

	if (m_renderPaths == renderPaths)
		return;

	m_renderPaths = renderPaths;

	if (m_renderPaths)
	{
		m_line = std::make_shared<NavigationLine>(this);
		m_line->SetVisible(nav::GetSettings().show_nav_path);
		g_renderHandler->AddRenderable(m_line.get());

		m_debugDrawGrp = std::make_unique<RenderGroup>(g_pDevice);
		g_renderHandler->AddRenderable(m_debugDrawGrp.get());
	}
	else
	{
		if (m_debugDrawGrp)
			g_renderHandler->RemoveRenderable(m_debugDrawGrp.get());

		if (m_line)
			g_renderHandler->RemoveRenderable(m_line.get());

		m_line.reset();
		m_debugDrawGrp.reset();
	}
}

void NavigationPath::SetDestination(const std::shared_ptr<DestinationInfo>& info)
{
	m_destinationInfo = info;

	// do this here because we don't want to keep calculating this every time an update is called
	auto* mesh = g_mq2Nav->Get<NavMesh>();
	if (mesh && m_destinationInfo && m_destinationInfo->heightType == HeightType::Nearest)
	{
		glm::vec3 transformed = {
			m_destinationInfo->eqDestinationPos.x,
			m_destinationInfo->eqDestinationPos.z,
			m_destinationInfo->eqDestinationPos.y
		};

		auto heights = mesh->GetHeights(transformed);

		// we don't actually want to calculate the path at the current height since there
		// is no guarantee it is on the mesh
		float current_distance = std::numeric_limits<float>().max();

		// create a destination location out of the given x/y coordinate and each z coordinate
		// that was hit at that location. Calculate the length of each and take the z coordinate that
		// creates the shortest path.
		for (auto height : heights)
		{
			float current_height = m_destinationInfo->eqDestinationPos.z;
			m_destinationInfo->eqDestinationPos.z = height;

			if (!FindPath() || GetPathTraversalDistance() > current_distance)
			{
				m_destinationInfo->eqDestinationPos.z = current_height;
			} // else leave it, it's a better height
		}
	}
}

bool NavigationPath::FindPath()
{
	if (!m_navMesh)
		return false;

	if (!m_destinationInfo || !m_destinationInfo->valid)
		return false;

	// WriteChatf("MQ2Navigation::FindPath - %.1f %.1f %.1f", X, Y, Z);

	// Reset the path in case of failure
	m_currentPath->Reset(0);

	UpdatePath(true);

	return m_currentPath->length > 0;
}

bool NavigationPath::CanSeeDestination() const
{
	if (!m_query)
	{
		return false;
	}

	// if less than two nodes, we're basically already on the destination.
	if (m_currentPath->length < 2)
	{
		return true;
	}

	// Get current and last poly/verts
	auto curr = m_currentPath->GetNode(0);
	auto last = m_currentPath->GetNode(m_currentPath->length - 1);

	dtRaycastHit hit;

	dtStatus result = m_query->raycast(
		std::get<dtPolyRef>(curr),
		glm::value_ptr(std::get<glm::vec3>(curr)),
		glm::value_ptr(std::get<glm::vec3>(last)),
		&m_filter,
		0,
		&hit);

	// line of sight if no hit
	return dtStatusSucceed(result) && hit.t == FLT_MAX;
}

void NavigationPath::SetNavMesh(const std::shared_ptr<dtNavMesh>& navMesh,
	bool updatePath)
{
	m_navMesh = navMesh;

	m_query.reset();

	m_filter = dtQueryFilter{};
	m_filter.setIncludeFlags(+PolyFlags::All);
	m_filter.setExcludeFlags(+PolyFlags::Disabled);
	if (auto* mesh = g_mq2Nav->Get<NavMesh>())
	{
		mesh->FillFilterAreaCosts(m_filter);
	}

	if (updatePath && m_navMesh)
	{
		UpdatePath();
	}
}

void NavigationPath::UpdatePath(bool force, bool incremental)
{
	if (m_navMesh == nullptr || m_destinationInfo == nullptr)
		return;

	if (m_query == nullptr)
	{
		m_query = deleting_unique_ptr<dtNavMeshQuery>(dtAllocNavMeshQuery(),
			[](dtNavMeshQuery* ptr) { dtFreeNavMeshQuery(ptr); });

		m_query->init(m_navMesh.get(), NAVMESH_QUERY_MAX_NODES);
	}

	PSPAWNINFO me = GetCharInfo()->pSpawn;
	if (me == nullptr)
		return;

	m_destination = m_destinationInfo->eqDestinationPos;

	// current position in mesh coordinates
	glm::vec3 thisPos{ me->X, me->FloorHeight, me->Y };

	if (thisPos == m_lastPos && !force)
		return;
	m_lastPos = thisPos;

	// convert destination to mesh coordinates
	glm::vec3 dest = m_destination;
	std::swap(dest.y, dest.z);

	std::unique_ptr<StraightPath> newPath = RecomputePath(thisPos, dest, force, incremental);
	bool changed = false;

	if (newPath)
	{
		m_currentPath = std::move(newPath);
		changed = true;
	}
	else if (!incremental)
	{
		// not an incremental update, and no new path, so make sure there is no current path.
		m_currentPath->Reset(0);
		changed = true;
	}

	if (changed)
	{
		PathUpdated();
	}

	// Give the path debug an update
	if (m_debugDrawGrp)
	{
		m_debugDrawGrp->Reset();

		auto& settings = nav::GetSettings();

		if (m_debugDrawGrp && settings.debug_render_pathing)
		{
			DebugDrawDX dd(m_debugDrawGrp.get());

			// draw current position
			duDebugDrawCross(&dd, me->X, me->FloorHeight, me->Y, 0.5, DXColor(51, 255, 255), 1);

			// Draw the waypoints. Green is next point
			for (int i = 0; i < m_currentPath->length; ++i)
			{
				int color = DXColor(255, 255, 255);
				if (i < m_currentPath->cursor)
					color = DXColor(0, 102, 204);
				if (i == m_currentPath->cursor)
					color = DXColor(0, 255, 0);
				if (i == m_currentPath->length - 1)
					color = DXColor(255, 0, 0);

				const glm::vec3& pos = m_currentPath->verts[i];

				duDebugDrawCross(&dd, pos.x, pos.y, pos.z, 0.5, color, 1);
			}
		}
	}

	// update the pathing line
	if (m_line)
	{
		// Update render line initial position
		m_line->SetCurrentPos(m_lastPos);
		m_line->Update();
	}
}

std::unique_ptr<StraightPath> NavigationPath::RecomputePath(
	const glm::vec3& startPos, const glm::vec3& endPos, bool force, bool incremental)
{
	auto& settings = nav::GetSettings();

	glm::vec3 extents = m_extents;
	if (settings.use_find_polygon_extents)
	{
		extents = settings.find_polygon_extents;
	}

	dtPolyRef startRef;
	glm::vec3 spos;

	// TODO: Cache the last known valid starting position to detect when moving off the mesh

	m_query->findNearestPoly(
		glm::value_ptr(startPos),
		glm::value_ptr(extents),
		&m_filter, &startRef, glm::value_ptr(spos));

	if (!startRef)
	{
		if (!incremental)
		{
			WriteChatf(PLUGIN_MSG "\arCould not locate starting point on navmesh: %.2f %.2f %.2f",
				startPos.z, startPos.x, startPos.y);
		}
		return {};
	}

	glm::vec3 epos;
	dtPolyRef endRef;

	m_query->findNearestPoly(
		glm::value_ptr(endPos),
		glm::value_ptr(extents),
		&m_filter, &endRef, glm::value_ptr(epos));

	if (!endRef)
	{
		if (!incremental)
		{
			WriteChatf(PLUGIN_MSG "Could not locate destination on navmesh: %.2f %.2f %.2f",
				endPos.z, endPos.x, endPos.y);
		}

		return {};
	}

	dtPolyRef polys[MAX_STRAIGHT_PATH_LENGTH];
	int numPolys = 0;

	dtStatus status = m_query->findPath(
		startRef, endRef,
		glm::value_ptr(spos),
		glm::value_ptr(epos), &m_filter, polys, &numPolys, MAX_STRAIGHT_PATH_LENGTH);

	if (dtStatusFailed(status))
	{
		DebugSpewAlways("findPath from %.2f,%.2f,%.2f to %.2f,%.2f,%.2f failed",
			startPos.x, startPos.y, startPos.z,
			endPos.x, endPos.y, endPos.z);

		return {};
	}

	if (dtStatusDetail(status, DT_OUT_OF_NODES)
		|| dtStatusDetail(status, DT_BUFFER_TOO_SMALL))
	{
		DebugSpewAlways("findPath from %.2f,%.2f,%.2f to %.2f,%.2f,%.2f failed: incomplete result (%x)",
			startPos.x, startPos.y, startPos.z,
			endPos.x, endPos.y, endPos.z,
			(status & DT_STATUS_DETAIL_MASK));

		if (!incremental)
		{
			WriteChatf(PLUGIN_MSG "\arCould not reach destination (too far away): %.2f %.2f %.2f",
				endPos.z, endPos.x, endPos.y);
		}
		return {};
	}

	if ((numPolys > 0 && (polys[numPolys - 1] != endRef))
		|| dtStatusDetail(status, DT_PARTIAL_RESULT))
	{
		// Partial path, did not find path to target
		DebugSpewAlways("findPath from %.2f,%.2f,%.2f to %.2f,%.2f,%.2f returned a partial result.",
			startPos.x, startPos.y, startPos.z,
			endPos.x, endPos.y, endPos.z);

		if (!incremental)
		{
			WriteChatf(PLUGIN_MSG "\arCould not find path to destination: %.2f %.2f %.2f",
				endPos.z, endPos.x, endPos.y);
		}

		return {};
	}

	auto path = std::make_unique<StraightPath>(MAX_STRAIGHT_PATH_LENGTH);

	if (numPolys > 0)
	{
		m_query->findStraightPath(
			glm::value_ptr(spos),
			glm::value_ptr(epos), polys, numPolys,
			glm::value_ptr(path->verts[0]),
			path->flags.get(),
			path->polys.get(),
			&path->length,
			MAX_STRAIGHT_PATH_LENGTH,
			DT_STRAIGHTPATH_AREA_CROSSINGS);

		// The 0th index is the starting point. Begin by trying to reach the
		// 2nd point...
		if (path->length > 1)
			path->cursor = 1;
	}

	return std::move(path);
}

float NavigationPath::GetPathTraversalDistance() const
{
	float result = 0.f;

	if (!m_destinationInfo || !m_destinationInfo->valid)
		return -1.f;

	for (int i = 0; i < m_currentPath->length - 1; ++i)
	{
		const float* first = GetRawPosition(i);
		const float* second = GetRawPosition(i + 1);

		// accumulate the distance
		result += dtVdist(first, second);
	}
	
	return result;
}

void NavigationPath::RenderUI()
{
#if DEBUG_NAVIGATION_LINES
	if (m_line)
	{
		m_line->RenderUI();
	}
#endif
}

glm::vec3 NavigationPath::GetDestination() const
{
	return m_destinationInfo ? m_destinationInfo->eqDestinationPos : glm::vec3{};
}

//----------------------------------------------------------------------------

NavigationLine::NavigationLine(NavigationPath* path)
	: m_path(path)
{
	m_renderPasses.emplace_back(ImColor(0, 0, 0, 200), 0.9f);
	m_renderPasses.emplace_back(ImColor(52, 152, 219, 200), 0.5f);
	m_renderPasses.emplace_back(ImColor(241, 196, 15, 200), 0.5f);
}

NavigationLine::~NavigationLine()
{
	InvalidateDeviceObjects();
}

bool NavigationLine::CreateDeviceObjects()
{
	if (!m_visible)
		return true;

	auto shaderFile = LoadResource(IDR_VOLUMELINES_FX);
	if (shaderFile.empty())
		return false;

	ID3DXBuffer* errors = nullptr;

	HRESULT hr = D3DXCreateEffect(
		g_pDevice,
		shaderFile.data(),
		shaderFile.length(),
		nullptr,
		nullptr,
		0,
		nullptr,
		&m_effect,
		&errors);

	if (FAILED(hr))
	{
		if (errors)
		{
			DebugSpewAlways("Effect error: %s", errors->GetBufferPointer());

			errors->Release();
			errors = nullptr;
		}

		InvalidateDeviceObjects();
		return false;
	}

	D3DVERTEXELEMENT9 vertexElements[] =
	{
		{ 0,  0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  0 },
		{ 0, 12,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,  0 },
		{ 0, 24,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
		{ 0, 36,  D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  1 },
		{ 0, 40,  D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  2 },
		D3DDECL_END()
	};

	hr = g_pDevice->CreateVertexDeclaration(vertexElements, &m_vDeclaration);
	if (FAILED(hr))
	{
		InvalidateDeviceObjects();
		return false;
	}

	m_loaded = true;
	return true;
}

void NavigationLine::InvalidateDeviceObjects()
{
	if (m_effect)
	{
		m_effect->Release();
		m_effect = nullptr;
	}

	if (m_vertexBuffer)
	{
		m_vertexBuffer->Release();
		m_vertexBuffer = nullptr;
	}

	if (m_vDeclaration)
	{
		m_vDeclaration->Release();
		m_vDeclaration = nullptr;
	}

	m_loaded = false;
}

void NavigationLine::Render(RenderPhase phase)
{
	if (phase != Renderable::Render_Geometry)
		return;
	if (!m_loaded)
		return;
	if (m_needsUpdate)
		GenerateBuffers();
	if (!m_vertexBuffer)
		return;

	D3DXMATRIX world;
	D3DXMatrixIdentity(&world);
	g_pDevice->SetTransform(D3DTS_WORLD, &world);

	D3DXMATRIX view, proj, mWVP, mWV;
	g_pDevice->GetTransform(D3DTS_VIEW, &view);
	g_pDevice->GetTransform(D3DTS_PROJECTION, &proj);

	mWV = world * view;
	mWVP = world * view * proj;

	// We will not be using a viewing transformation, so the view matrix will be identity.
	m_effect->SetMatrix("mWV", &mWV);
	m_effect->SetMatrix("mWVP", &mWVP);

	DWORD depthTest = 0, depthFunc = 0;
	g_pDevice->GetRenderState(D3DRS_ZENABLE, &depthTest);
	g_pDevice->GetRenderState(D3DRS_ZFUNC, &depthFunc);

	g_pDevice->SetVertexDeclaration(m_vDeclaration);
	g_pDevice->SetStreamSource(0, m_vertexBuffer, 0, sizeof(TVertex));

	UINT passes = 0;
	m_effect->Begin(&passes, 0);

	for (int iPass = 0; iPass < passes; iPass++)
	{
		RenderStyle style;
		if (iPass < m_renderPasses.size())
			style = m_renderPasses[iPass];

		if (!style.enabled)
			continue;

		m_effect->BeginPass(iPass);

		m_effect->SetVector("lineColor", (D3DXVECTOR4*)&style.render_color);
		m_effect->SetFloat("lineWidth", style.width);
		m_effect->CommitChanges();

		// render
		for (auto& cmd : m_commands)
		{
			g_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP,
				cmd.StartVertex, cmd.PrimitiveCount);
		}

		m_effect->EndPass();
	}

	m_effect->End();

	g_pDevice->SetRenderState(D3DRS_ZENABLE, depthTest);
	g_pDevice->SetRenderState(D3DRS_ZFUNC, depthFunc);
}

void NavigationLine::GenerateBuffers()
{
	int size = m_path->m_currentPath->length;

	if (size == m_lastSize)
	{
		// check if the path actually changed.
	}

	if (size <= 0)
		return;

	int bufferLength = (size - 1) * 4;

	if (!m_vertexBuffer || size > m_lastSize)
	{
		if (m_vertexBuffer)
			m_vertexBuffer->Release();

		HRESULT hr = g_pDevice->CreateVertexBuffer(bufferLength * sizeof(TVertex),
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, TVertex::FVF, D3DPOOL_DEFAULT,
			&m_vertexBuffer, nullptr);

		if (FAILED(hr))
		{
			m_vertexBuffer = nullptr;
			return;
		}
	}

	m_lastSize = size;
	TVertex* vertexDest = nullptr;

	if (m_vertexBuffer->Lock(0, bufferLength * sizeof(TVertex), (void**)&vertexDest, D3DLOCK_DISCARD) < 0)
		return;

	// get the path as if it were a series of 3d points
	D3DXVECTOR3* pt = (D3DXVECTOR3*)&m_path->m_currentPath->verts[0];

	int index = 0;
	m_commands.resize(size - 1);

	for (int i = 0; i < size - 1; ++i, ++pt)
	{
		// this point
		D3DXVECTOR3 v0;
		if (i == 0) // use starting pos for first point
		{
			v0.x = m_startPos.z;
			v0.y = m_startPos.x;
			v0.z = m_startPos.y + 1;
		}
		else
		{
			v0.x = pt[0].z;
			v0.y = pt[0].x;
			v0.z = pt[0].y + 1;
		}

		// next point
		D3DXVECTOR3 v1;
		v1.x = pt[1].z;
		v1.y = pt[1].x;
		v1.z = pt[1].y + 1;

		// following point
		D3DXVECTOR3 nextPos;
		int nextIdx = i < size - 2 ? 2 : 1;
		nextPos.x = pt[nextIdx].z;
		nextPos.y = pt[nextIdx].x;
		nextPos.z = pt[nextIdx].y + 1;

		// previous point
		D3DXVECTOR3 prevPos;
		int prevIdx = i > 0 ? -1 : 0;
		prevPos.x = pt[prevIdx].z;
		prevPos.y = pt[prevIdx].x;
		prevPos.z = pt[prevIdx].y + 1;

		vertexDest[index + 0].pos = v0;
		vertexDest[index + 0].otherPos = v1;
		vertexDest[index + 0].thickness = -m_thickness;
		vertexDest[index + 0].adjPos = prevPos;
		vertexDest[index + 0].adjHint = prevIdx;

		vertexDest[index + 1].pos = v1;
		vertexDest[index + 1].otherPos = v0;
		vertexDest[index + 1].thickness = m_thickness;
		vertexDest[index + 1].adjPos = nextPos;
		vertexDest[index + 1].adjHint = nextIdx - 1;

		vertexDest[index + 2].pos = v0;
		vertexDest[index + 2].otherPos = v1;
		vertexDest[index + 2].thickness = m_thickness;
		vertexDest[index + 2].adjPos = prevPos;
		vertexDest[index + 2].adjHint = prevIdx;

		vertexDest[index + 3].pos = v1;
		vertexDest[index + 3].otherPos = v0;
		vertexDest[index + 3].thickness = -m_thickness;
		vertexDest[index + 3].adjPos = nextPos;
		vertexDest[index + 3].adjHint = nextIdx - 1;

		m_commands[i].StartVertex = index;
		m_commands[i].PrimitiveCount = 2; // 2 triangles in a strip
		index += 4;
	}

	m_vertexBuffer->Unlock();
	m_needsUpdate = false;
}

void NavigationLine::SetThickness(float thickness)
{
	if (thickness != m_thickness)
	{
		m_thickness = thickness;
		Update();
	}
}

void NavigationLine::SetVisible(bool visible)
{
	if (m_visible != visible)
	{
		m_visible = visible;

		if (m_visible) {
			m_needsUpdate = true;
			CreateDeviceObjects();
		}
		else
			InvalidateDeviceObjects();
	}
}

void NavigationLine::SetCurrentPos(const glm::vec3& currentPos)
{
	m_startPos = currentPos;
}

void NavigationLine::Update()
{
	if (nav::GetSettings().show_nav_path)
	{
		m_needsUpdate = true;
	}
}

#if DEBUG_NAVIGATION_LINES
void NavigationLine::RenderUI()
{
	ImGui::PushID(this);
	if (ImGui::CollapsingHeader("Navigation Line Debug"))
	{
		int size = m_renderPasses.size();
		if (ImGui::InputInt("Render Passes", &size))
			m_renderPasses.resize(size);

		for (int i = 0; i < m_renderPasses.size(); ++i)
		{
			ImGui::PushID(i);
			auto& config = m_renderPasses[i];
			ImGui::Checkbox("Enabled", &config.enabled);
			ImGui::ColorEdit4("Color", (float*)&config.render_color,
				ImGuiColorEditFlags_RGB);
			ImGui::InputFloat("Width", &config.width);

			ImGui::PopID();
		}
	}

	ImGui::PopID();
}
#endif

//----------------------------------------------------------------------------
