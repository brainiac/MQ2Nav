//
// NavigationPath.cpp
//

#include "pch.h"
#include "NavigationPath.h"

#include "common/Logging.h"
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
#include <spdlog/spdlog.h>

//----------------------------------------------------------------------------
// constants

const int MAX_STRAIGHT_PATH_LENGTH = 16384;

NavigationLine::LineStyle gNavigationLineStyle;

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

	m_renderPaths = nav::GetSettings().show_nav_path;
	
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
	{
		m_line->SetVisible(renderPaths);
	}

	if (m_renderPaths == renderPaths && (m_renderPaths == (m_line != nullptr)))
		return;

	m_renderPaths = renderPaths;

	if (m_renderPaths)
	{
		m_line = g_mq2Nav->GetGameLine();
		m_line->SetVisible(m_renderPaths);
		m_line->SetCurrentPos(m_lastPos);

		m_debugDrawGrp = std::make_unique<RenderGroup>(g_pDevice);
		g_renderHandler->AddRenderable(m_debugDrawGrp.get());
	}
	else
	{
		if (m_debugDrawGrp)
			g_renderHandler->RemoveRenderable(m_debugDrawGrp.get());

		if (m_line)
		{
			m_line->SetVisible(false);
			m_line.reset();
		}
		m_debugDrawGrp.reset();
	}
}

void NavigationPath::SetDestination(const std::shared_ptr<DestinationInfo>& info)
{
	m_destinationInfo = info;
}

bool NavigationPath::FindPath()
{
	if (!m_navMesh)
		return false;

	if (!m_destinationInfo || !m_destinationInfo->valid)
		return false;

	// Reset the path in case of failure
	m_currentPath->Reset(0);

	UpdatePath(true);

	return m_currentPath->length > 0;
}

void NavigationPath::Increment()
{
	++m_currentPath->cursor;

	UpdatePathProperties();
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

	// don't perform incremental update if updates are disabled
	if (incremental && !nav::GetSettings().poll_navigation_path)
		return;

	// don't perform incremental update if we're following an off-mesh link
	if (incremental && m_followingLink)
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

	// current position in mesh coordinates
	glm::vec3 thisPos{ me->X, me->FloorHeight, me->Y };

	if (thisPos == m_lastPos && m_destination == m_destinationInfo->eqDestinationPos  && !force)
		return;

	m_destination = m_destinationInfo->eqDestinationPos;
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

	UpdatePathProperties();

	if (changed)
	{
		PathUpdated();
	}

	// Update render path
	int renderSize = 1 + m_currentPath->length - m_currentPath->cursor;
	std::vector<int> renderPath;
	renderPath.resize(renderSize);

	renderPath[0] = -1;
	if (renderSize > 1)
	{
		for (int i = 0, node = m_currentPath->cursor; node != m_currentPath->length; ++node, ++i)
			renderPath[i + 1] = node;
	}
	m_renderPath = renderPath;

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
			SPDLOG_ERROR("Could not locate starting point on navmesh: {}", startPos.zxy());
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
			SPDLOG_ERROR("Could not locate destination on navmesh: {}", endPos.zxy());
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
		SPDLOG_DEBUG("findPath from {} to {} failed.", startPos, endPos);
		return {};
	}

	if (dtStatusDetail(status, DT_OUT_OF_NODES)
		|| dtStatusDetail(status, DT_BUFFER_TOO_SMALL))
	{
		SPDLOG_DEBUG("findPath from {} to {} failed: incomplete result ({0:#x})",
			startPos, endPos, (status & DT_STATUS_DETAIL_MASK));

		if (!incremental)
		{
			SPDLOG_ERROR("Could not reach destination (too far away): {}", endPos.zxy());
		}
		return {};
	}

	if ((numPolys > 0 && (polys[numPolys - 1] != endRef))
		|| dtStatusDetail(status, DT_PARTIAL_RESULT))
	{
		// Partial path, did not find path to target
		SPDLOG_DEBUG("findPath from {} to {} returned a partial result.", startPos, endPos);

		if (!incremental)
		{
			SPDLOG_ERROR("Could not find path to destination: {}", endPos.zxy());
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

void NavigationPath::UpdatePathProperties()
{
	if (!m_currentPath)
	{
		m_followingLink = false;
		return;
	}

	// Check the current path node. Cursor points at the current dest,
	// so the previous point will be our current node.
	auto node = m_currentPath->GetNode(std::max(0, m_currentPath->cursor - 1));

	// we following a link if the current node is an off-mesh connection
	m_followingLink = (std::get<uint8_t>(node) == DT_STRAIGHTPATH_OFFMESH_CONNECTION);
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

glm::vec3 NavigationPath::GetDestination() const
{
	return m_destinationInfo ? m_destinationInfo->eqDestinationPos : glm::vec3{};
}

//----------------------------------------------------------------------------

NavigationLine::NavigationLine()
{
}

NavigationLine::~NavigationLine()
{
	InvalidateDeviceObjects();
}

void NavigationLine::SetNavigationPath(NavigationPath* path)
{
	m_path = path;

	if (m_visible && m_path)
	{
		GenerateBuffers();
	}
	else
	{
		ReleasePath();
	}
}

bool NavigationLine::CreateDeviceObjects()
{
	if (!m_visible)
		return true;
	if (m_loaded)
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
			SPDLOG_ERROR("Error loading navigation line shader: {}",
				std::string_view{ (const char*)errors->GetBufferPointer(), errors->GetBufferSize() });

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
		{ 0, 44,  D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  3 },
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

	ReleasePath();

	if (m_vDeclaration)
	{
		m_vDeclaration->Release();
		m_vDeclaration = nullptr;
	}

	m_loaded = false;
}

void NavigationLine::ReleasePath()
{
	if (m_vertexBuffer)
	{
		m_vertexBuffer->Release();
		m_vertexBuffer = nullptr;
	}
}

void NavigationLine::Render()
{
	if (!m_loaded)
		return;
	if (m_needsUpdate && m_path)
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

	g_pDevice->Clear(0, 0, D3DCLEAR_STENCIL, 0, 0, 0);

	UINT passes = 0;
	m_effect->Begin(&passes, 0);

	for (int iPass = 0; iPass < passes; iPass++)
	{
		m_effect->BeginPass(iPass);

		switch (iPass)
		{
		case 0: // hidden
			if (gNavigationLineStyle.hiddenOpacity == 0)
			{
				m_effect->EndPass();
				continue;
			}
			g_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATER);
			m_effect->SetFloat("opacity", gNavigationLineStyle.hiddenOpacity);
			m_effect->SetFloat("lineWidth", gNavigationLineStyle.lineWidth - (2 * gNavigationLineStyle.borderWidth));
			m_effect->SetVector("lineColor", (D3DXVECTOR4*)&gNavigationLineStyle.hiddenColor);
			m_effect->SetVector("linkColor", (D3DXVECTOR4*)&gNavigationLineStyle.hiddenColor);
			break;

		case 1: // visible
			g_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
			m_effect->SetFloat("opacity", gNavigationLineStyle.opacity);
			m_effect->SetFloat("lineWidth", gNavigationLineStyle.lineWidth - (2 * gNavigationLineStyle.borderWidth));
			m_effect->SetVector("lineColor", (D3DXVECTOR4*)&gNavigationLineStyle.visibleColor);
			m_effect->SetVector("linkColor", (D3DXVECTOR4*)&gNavigationLineStyle.linkColor);
			break;

		case 2: // border - hidden
			if (gNavigationLineStyle.hiddenOpacity == 0)
			{
				m_effect->EndPass();
				continue;
			}
			g_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATER);
			m_effect->SetFloat("opacity", gNavigationLineStyle.hiddenOpacity);
			m_effect->SetFloat("lineWidth", gNavigationLineStyle.lineWidth);
			m_effect->SetVector("lineColor", (D3DXVECTOR4*)&gNavigationLineStyle.borderColor);
			m_effect->SetVector("linkColor", (D3DXVECTOR4*)&gNavigationLineStyle.borderColor);
			break;

		case 3: // border - visible
			g_pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
			m_effect->SetFloat("opacity", gNavigationLineStyle.opacity);
			m_effect->SetFloat("lineWidth", gNavigationLineStyle.lineWidth);
			m_effect->SetVector("lineColor", (D3DXVECTOR4*)&gNavigationLineStyle.borderColor);
			m_effect->SetVector("linkColor", (D3DXVECTOR4*)&gNavigationLineStyle.borderColor);
			break;
		}

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
	const std::vector<int>& renderPath = m_path->GetRenderPath();
	int size = renderPath.size();

	if (size <= 0)
		return;

	int bufferLength = (size - 1) * 4;

	if (!m_vertexBuffer || size > m_lastSize)
	{
		if (m_vertexBuffer) {
			m_vertexBuffer->Release();
		}

		HRESULT hr = g_pDevice->CreateVertexBuffer(bufferLength * sizeof(TVertex),
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT,
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

	D3DXVECTOR3* pt = (D3DXVECTOR3*)&m_path->m_currentPath->verts[0];
	uint8_t* flags = &m_path->m_currentPath->flags[0];

	int index = 0;
	m_commands.resize(size - 1);

	for (int node = 0; node < size - 1; ++node, ++pt)
	{
		int i = renderPath[node];
		float type = 0.f;

		// this point
		D3DXVECTOR3 v0;
		if (i == -1) // use starting pos for first point
		{
			v0.x = m_startPos.z;
			v0.y = m_startPos.x;
			v0.z = m_startPos.y + 1;

			if (m_path->m_followingLink)
				type = 1.0f;
		}
		else
		{
			v0.x = pt[0].z;
			v0.y = pt[0].x;
			v0.z = pt[0].y + 1;

			if (flags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
				type = 1.0f;
		}

		// next point
		D3DXVECTOR3 v1;
		v1.x = pt[1].z;
		v1.y = pt[1].x;
		v1.z = pt[1].y + 1;

		// following point
		D3DXVECTOR3 nextPos;
		int nextIdx = node < size - 2 ? 2 : 1;
		nextPos.x = pt[nextIdx].z;
		nextPos.y = pt[nextIdx].x;
		nextPos.z = pt[nextIdx].y + 1;

		// previous point
		D3DXVECTOR3 prevPos;
		int prevIdx = node > 0 ? -1 : 0;
		prevPos.x = pt[prevIdx].z;
		prevPos.y = pt[prevIdx].x;
		prevPos.z = pt[prevIdx].y + 1;

		vertexDest[index + 0].pos = v0;
		vertexDest[index + 0].otherPos = v1;
		vertexDest[index + 0].thickness = -m_thickness;
		vertexDest[index + 0].adjPos = prevPos;
		vertexDest[index + 0].adjHint = prevIdx;
		vertexDest[index + 0].type = type;

		vertexDest[index + 1].pos = v1;
		vertexDest[index + 1].otherPos = v0;
		vertexDest[index + 1].thickness = m_thickness;
		vertexDest[index + 1].adjPos = nextPos;
		vertexDest[index + 1].adjHint = nextIdx - 1;
		vertexDest[index + 1].type = type;

		vertexDest[index + 2].pos = v0;
		vertexDest[index + 2].otherPos = v1;
		vertexDest[index + 2].thickness = m_thickness;
		vertexDest[index + 2].adjPos = prevPos;
		vertexDest[index + 2].adjHint = prevIdx;
		vertexDest[index + 2].type = type;

		vertexDest[index + 3].pos = v1;
		vertexDest[index + 3].otherPos = v0;
		vertexDest[index + 3].thickness = -m_thickness;
		vertexDest[index + 3].adjPos = nextPos;
		vertexDest[index + 3].adjHint = nextIdx - 1;
		vertexDest[index + 3].type = type;

		m_commands[node].StartVertex = index;
		m_commands[node].PrimitiveCount = 2; // 2 triangles in a strip
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

		if (m_visible)
		{
			m_needsUpdate = true;
			CreateDeviceObjects();
		}
		else
		{
			ReleasePath();
		}
	}
}

void NavigationLine::SetCurrentPos(const glm::vec3& currentPos)
{
	if (m_startPos != currentPos)
	{
		m_startPos = currentPos;
		m_needsUpdate = true;
	}
}

void NavigationLine::Update()
{
	if (nav::GetSettings().show_nav_path)
	{
		m_needsUpdate = true;
	}
}

//----------------------------------------------------------------------------
