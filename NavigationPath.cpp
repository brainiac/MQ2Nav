//
// NavigationPath.cpp
//

#include "NavigationPath.h"
#include "MQ2Navigation.h"
#include "NavMeshLoader.h"
#include "RenderHandler.h"
#include "MQ2Nav_Settings.h"

#include "DebugDrawDX.h"
#include "DetourNavMesh.h"
#include "DetourCommon.h"


NavigationPath::NavigationPath(bool renderPaths)
	: m_renderPaths(renderPaths)
{
	m_filter.setIncludeFlags(0x01); // walkable surface

	m_useCorridor = mq2nav::GetSettings().debug_use_pathing_corridor;

	auto* loader = g_mq2Nav->Get<NavMeshLoader>();
	m_navMeshConn = loader->OnNavMeshChanged.Connect([this](dtNavMesh* navMesh) { SetNavMesh(navMesh); });
	SetNavMesh(loader->GetNavMesh());

	if (m_renderPaths)
	{
		m_line = std::make_shared<NavigationLine>(this);
		m_line->SetVisible(mq2nav::GetSettings().show_nav_path);
		g_renderHandler->AddRenderable(m_line.get());

		m_debugDrawGrp = std::make_unique<RenderGroup>(g_pDevice);
		g_renderHandler->AddRenderable(m_debugDrawGrp.get());
	}
}

NavigationPath::~NavigationPath()
{
	if (m_line)
	{
		g_renderHandler->RemoveRenderable(m_debugDrawGrp.get());
		g_renderHandler->RemoveRenderable(m_line.get());
	}
}

//----------------------------------------------------------------------------

bool NavigationPath::FindPath(const glm::vec3& pos)
{
	if (nullptr != m_navMesh)
	{
		// WriteChatf("MQ2Navigation::FindPath - %.1f %.1f %.1f", X, Y, Z);
		FindPathInternal(pos);

		if (m_currentPathSize > 0)
		{
			// DebugSpewAlways("FindPath - cursor = %d, size = %d", m_currentPathCursor , m_currentPathSize);
			return true;
		}
	}
	return false;
}

void NavigationPath::FindPathInternal(const glm::vec3& pos)
{
	if (nullptr == m_navMesh)
		return;

	m_currentPathCursor = 0;
	m_currentPathSize = 0;
	m_destination = pos;

	UpdatePath(true);
}

void NavigationPath::SetNavMesh(dtNavMesh* navMesh)
{
	m_navMesh = navMesh;

	m_query.reset();
	m_corridor.reset();

	if (m_navMesh)
	{
		UpdatePath();
	}
}

void NavigationPath::UpdatePath(bool force)
{
	if (m_navMesh == nullptr)
		return;
	if (m_query == nullptr)
	{
		m_query.reset(new dtNavMeshQuery);
		m_query->init(m_navMesh, 10000 /* MAX_NODES */);
	}

	PSPAWNINFO me = GetCharInfo()->pSpawn;
	if (me == nullptr)
		return;

	float startOffset[3] = { me->X, me->Feet, me->Y };
	float endOffset[3] = { m_destination.x, m_destination.z, m_destination.y };
	float spos[3];
	float epos[3];

	glm::vec3 thisPos(startOffset[0], startOffset[1], startOffset[2]);

	if (thisPos == m_lastPos && !force)
		return;
	m_lastPos = thisPos;

	m_currentPathCursor = 0;
	m_currentPathSize = 0;

	dtPolyRef startRef, endRef;
	m_query->findNearestPoly(startOffset, m_extents, &m_filter, &startRef, spos);

	if (!startRef)
	{
		WriteChatf(PLUGIN_MSG "No start reference");
		return;
	}

	dtPolyRef polys[MAX_POLYS];
	int numPolys = 0;

	if (!m_corridor)
	{
		if (m_useCorridor)
		{
			// initialize planning
			m_corridor.reset(new dtPathCorridor);
			m_corridor->init(MAX_PATH_SIZE);

			m_corridor->reset(startRef, startOffset);
		}

		m_query->findNearestPoly(endOffset, m_extents, &m_filter, &endRef, epos);

		if (!endRef)
		{
			WriteChatf(PLUGIN_MSG "No end reference");
			return;
		}

		dtStatus status = m_query->findPath(startRef, endRef, spos, epos, &m_filter, polys, &numPolys, MAX_POLYS);
		if (status & DT_OUT_OF_NODES)
			DebugSpewAlways("findPath from %.2f,%.2f,%.2f to %.2f,%.2f,%.2f failed: out of nodes",
				startOffset[0], startOffset[1], startOffset[2],
				endOffset[0], endOffset[1], endOffset[2]);
		if (status & DT_PARTIAL_RESULT)
			DebugSpewAlways("findPath from %.2f,%.2f,%.2f to %.2f,%.2f,%.2f returned a partial result.",
				startOffset[0], startOffset[1], startOffset[2],
				endOffset[0], endOffset[1], endOffset[2]);

		if (m_corridor)
		{
			m_corridor->setCorridor(endOffset, polys, numPolys);
		}
	}
	else
	{
		// this is an update
		m_corridor->movePosition(startOffset, m_query.get(), &m_filter);
	}

	if (m_corridor)
	{
		m_corridor->optimizePathTopology(m_query.get(), &m_filter);

		m_currentPathSize = m_corridor->findCorners(m_currentPath,
			m_cornerFlags, polys, 10, m_query.get(), &m_filter);
	}

	if (m_debugDrawGrp)
		m_debugDrawGrp->Reset();

	if (numPolys > 0)
	{
		m_query->findStraightPath(spos, epos, polys, numPolys, m_currentPath,
			0, 0, &m_currentPathSize, MAX_POLYS, 0);

		// The 0th index is the starting point. Begin by trying to reach the
		// 2nd point...
		if (m_currentPathSize > 1)
			m_currentPathCursor = 1;

		if (m_debugDrawGrp && mq2nav::GetSettings().debug_render_pathing)
		{
			DebugDrawDX dd(m_debugDrawGrp.get());

			// draw current position
			duDebugDrawCross(&dd, me->X, me->Feet, me->Y, 0.5, DXColor(51, 255, 255), 1);

			// Draw the waypoints. Green is next point
			for (int i = 0; i < m_currentPathSize; ++i)
			{
				int color = DXColor(255, 255, 255);
				if (i < m_currentPathCursor)
					color = DXColor(0, 102, 204);
				if (i == m_currentPathCursor)
					color = DXColor(0, 255, 0);
				if (i == m_currentPathSize - 1)
					color = DXColor(255, 0, 0);

				duDebugDrawCross(&dd, m_currentPath[i * 3],
					m_currentPath[i * 3 + 1],
					m_currentPath[i * 3 + 2],
					0.5, color, 1);
			}
		}
	}

	if (m_line && mq2nav::GetSettings().show_nav_path)
	{
		m_line->Update();
	}
}

void NavigationPath::OnUpdateUI()
{
	bool showPath = mq2nav::GetSettings().show_nav_path;
	if (ImGui::Checkbox("Show navigation path", &showPath))
	{
		mq2nav::GetSettings().show_nav_path = showPath;
		mq2nav::SaveSettings(false);

		m_line->SetVisible(showPath);
	}

	//float thickness = m_line->GetThickness();
	//if (ImGui::DragFloat("Path Thickness", &thickness, 0.01f, 0.0f, FLT_MAX))
	//	m_line->SetThickness(thickness);
}

//----------------------------------------------------------------------------

NavigationLine::NavigationLine(NavigationPath* path)
	: m_path(path)
{
	// temp!
	m_shaderFile = std::string(gszINIPath) + "\\MQ2Nav\\VolumeLines.fx";
	m_textureFile = std::string(gszINIPath) + "\\MQ2Nav\\LineTexture.png";

	ZeroMemory(&m_material, sizeof(m_material));
	m_material.Diffuse.r = 1.0f;
	m_material.Diffuse.g = 1.0f;
	m_material.Diffuse.b = 1.0f;
	m_material.Diffuse.a = 1.0f;
}

NavigationLine::~NavigationLine()
{
	InvalidateDeviceObjects();
}

bool NavigationLine::CreateDeviceObjects()
{
	if (!m_visible)
		return true;

	ID3DXBuffer* errors = 0;
	HRESULT hr;

	hr = D3DXCreateTextureFromFileA(g_pDevice, m_textureFile.c_str(), &m_lineTexture);
	if (FAILED(hr))
	{
		DebugSpewAlways("Failed to load texture!");

		InvalidateDeviceObjects();
		return false;
	}

	hr = D3DXCreateEffectFromFileA(g_pDevice, m_shaderFile.c_str(),
		NULL, NULL, 0, NULL, &m_effect, &errors);
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
		{ 0, 24,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
		{ 0, 40,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  1 },
		D3DDECL_END()
	};

	hr = g_pDevice->CreateVertexDeclaration(vertexElements, &m_vDeclaration);
	if (FAILED(hr))
	{
		InvalidateDeviceObjects();
		return false;
	}

	m_effect->SetTexture("lineTexture", m_lineTexture);
	m_loaded = true;
}

void NavigationLine::InvalidateDeviceObjects()
{
	if (m_effect)
	{
		m_effect->Release();
		m_effect = nullptr;
	}

	if (m_lineTexture)
	{
		m_lineTexture->Release();
		m_lineTexture = nullptr;
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

	UINT passes = 0;
	m_effect->Begin(&passes, 0);

	for (int iPass = 0; iPass < passes; iPass++)
	{
		m_effect->BeginPass(iPass);

		g_pDevice->SetVertexDeclaration(m_vDeclaration);
		g_pDevice->SetStreamSource(0, m_vertexBuffer, 0, sizeof(TVertex));

		// render
		for (auto& cmd : m_commands)
		{
			g_pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP,
				cmd.StartVertex, cmd.PrimitiveCount);
		}

		m_effect->EndPass();
	}
	m_effect->End();
}

void NavigationLine::GenerateBuffers()
{
	int size = m_path->m_currentPathSize;

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
	D3DXVECTOR3* pt = (D3DXVECTOR3*)&m_path->m_currentPath[0];

	int index = 0;
	m_commands.resize(size - 1);

	for (int i = 0; i < size - 1; ++i, ++pt)
	{
		// this point
		D3DXVECTOR3 v0;
		v0.x = pt[0].z;
		v0.y = pt[0].x;
		v0.z = pt[0].y + 1;

		// next point
		D3DXVECTOR3 v1;
		v1.x = pt[1].z;
		v1.y = pt[1].x;
		v1.z = pt[1].y + 1;

		vertexDest[index + 0].pos = v0;
		vertexDest[index + 1].pos = v1;
		vertexDest[index + 2].pos = v0;
		vertexDest[index + 3].pos = v1;

		vertexDest[index + 0].otherPos = v1;
		vertexDest[index + 1].otherPos = v0;
		vertexDest[index + 2].otherPos = v1;
		vertexDest[index + 3].otherPos = v0;

		vertexDest[index + 0].thickness = D3DXVECTOR3(-m_thickness, 0.f, m_thickness * 0.5f);
		vertexDest[index + 1].thickness = D3DXVECTOR3(m_thickness, 0.f, m_thickness * 0.5f);
		vertexDest[index + 2].thickness = D3DXVECTOR3(m_thickness, 0.f, m_thickness * 0.5f);
		vertexDest[index + 3].thickness = D3DXVECTOR3(-m_thickness, 0.f, m_thickness * 0.5f);

		vertexDest[index + 0].texOffset = D3DXVECTOR4(m_thickness, m_thickness, 0.f, 0.f);
		vertexDest[index + 1].texOffset = D3DXVECTOR4(m_thickness, m_thickness, 0.25f, 0.f);
		vertexDest[index + 2].texOffset = D3DXVECTOR4(m_thickness, m_thickness, 0.f, 0.25f);
		vertexDest[index + 3].texOffset = D3DXVECTOR4(m_thickness, m_thickness, 0.25f, 0.25f);

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

//----------------------------------------------------------------------------
