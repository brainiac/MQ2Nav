//
// NavigationPath.cpp
//

#include "NavigationPath.h"
#include "MQ2Navigation.h"
#include "NavMeshLoader.h"
#include "RenderHandler.h"
#include "MQ2Nav_Settings.h"
#include "common/NavMeshData.h"

#include "DebugDrawDX.h"
#include "DetourNavMesh.h"
#include "DetourCommon.h"

//----------------------------------------------------------------------------
// constants

const int MAX_POLYS = 4028 * 4;

const int MAX_NODES = 2048 * 4;

const int MAX_PATH_SIZE = 2048 * 4;

//----------------------------------------------------------------------------

NavigationPath::NavigationPath(const std::shared_ptr<DestinationInfo>& dest)
	: m_renderPaths(false)
{
	auto* loader = g_mq2Nav->Get<NavMeshLoader>();
	m_navMeshConn = loader->OnNavMeshChanged.Connect([this](dtNavMesh* navMesh) { SetNavMesh(navMesh); });
	m_navMesh = loader->GetNavMesh();

	m_filter.setIncludeFlags(+PolyFlags::All);
	m_filter.setExcludeFlags(+PolyFlags::Disabled);

	m_useCorridor = mq2nav::GetSettings().debug_use_pathing_corridor;

	SetDestination(dest);
}

NavigationPath::~NavigationPath()
{
	SetShowNavigationPaths(false);
}

//----------------------------------------------------------------------------

void NavigationPath::SetShowNavigationPaths(bool renderPaths)
{
	if (m_renderPaths == renderPaths)
		return;

	m_renderPaths = renderPaths;

	if (m_renderPaths)
	{
		m_line = std::make_shared<NavigationLine>(this);
		m_line->SetVisible(mq2nav::GetSettings().show_nav_path);
		g_renderHandler->AddRenderable(m_line.get());

		m_debugDrawGrp = std::make_unique<RenderGroup>(g_pDevice);
		g_renderHandler->AddRenderable(m_debugDrawGrp.get());
	}
	else
	{
		g_renderHandler->RemoveRenderable(m_debugDrawGrp.get());
		g_renderHandler->RemoveRenderable(m_line.get());

		m_line.reset();
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

	// WriteChatf("MQ2Navigation::FindPath - %.1f %.1f %.1f", X, Y, Z);
	m_currentPathCursor = 0;
	m_currentPathSize = 0;

	UpdatePath(true);

	return m_currentPathSize > 0;
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
	if (m_navMesh == nullptr || m_destinationInfo == nullptr)
		return;
	if (m_query == nullptr)
	{
		m_query.reset(new dtNavMeshQuery);
		m_query->init(m_navMesh, 10000 /* MAX_NODES */);
	}

	PSPAWNINFO me = GetCharInfo()->pSpawn;
	if (me == nullptr)
		return;

	m_destination = m_destinationInfo->eqDestinationPos;

	float startOffset[3] = { me->X, me->FloorHeight, me->Y };
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

		if (!m_currentPath)
		{
			m_currentPath = std::unique_ptr<float[]>(new float[MAX_POLYS * 3]);
		}
		if (!m_cornerFlags)
		{
			m_cornerFlags = std::unique_ptr<uint8_t[]>(new uint8_t[MAX_POLYS]);
		}

		m_currentPathSize = m_corridor->findCorners(m_currentPath.get(),
			m_cornerFlags.get(), polys, 10, m_query.get(), &m_filter);
	}

	if (m_debugDrawGrp)
		m_debugDrawGrp->Reset();

	if (numPolys > 0)
	{
		if (!m_currentPath)
		{
			m_currentPath = std::unique_ptr<float[]>(new float[MAX_POLYS * 3]);
		}

		m_query->findStraightPath(spos, epos, polys, numPolys, m_currentPath.get(),
			0, 0, &m_currentPathSize, MAX_POLYS, 0);

		// The 0th index is the starting point. Begin by trying to reach the
		// 2nd point...
		if (m_currentPathSize > 1)
			m_currentPathCursor = 1;

		if (m_debugDrawGrp && mq2nav::GetSettings().debug_render_pathing)
		{
			DebugDrawDX dd(m_debugDrawGrp.get());

			// draw current position
			duDebugDrawCross(&dd, me->X, me->FloorHeight, me->Y, 0.5, DXColor(51, 255, 255), 1);

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

float NavigationPath::GetPathTraversalDistance() const
{
	float result = 0.f;

	if (!m_destinationInfo || !m_destinationInfo->valid)
		return -1.f;

	for (int i = 0; i < m_currentPathSize - 1; ++i)
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
	bool showPath = mq2nav::GetSettings().show_nav_path;
	if (ImGui::Checkbox("Show navigation path", &showPath))
	{
		SetShowNavigationPaths(showPath);

		mq2nav::GetSettings().show_nav_path = showPath;
		mq2nav::SaveSettings(false);

		if (m_line)
			m_line->SetVisible(showPath);
	}

#if DEBUG_NAVIGATION_LINES
	if (m_line)
		m_line->RenderUI();
#endif
}

glm::vec3 NavigationPath::GetDestination() const
{
	return m_destinationInfo ? m_destinationInfo->eqDestinationPos : glm::vec3();
}

//----------------------------------------------------------------------------

NavigationLine::NavigationLine(NavigationPath* path)
	: m_path(path)
{
	// temp!
	m_shaderFile = std::string(gszINIPath) + "\\MQ2Nav\\VolumeLines.fx";

	m_renderPasses.push_back(RenderStyle{ ImColor(0, 0, 0, 200), 0.9f });
	m_renderPasses.push_back(RenderStyle{ ImColor(52, 152, 219, 200), 0.5f });
	m_renderPasses.push_back(RenderStyle{ ImColor(241, 196, 15, 200), 0.5f });
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
			ImGui::ColorEditMode(ImGuiColorEditMode_RGB);
			ImGui::ColorEdit4("Color", (float*)&config.render_color);
			ImGui::InputFloat("Width", &config.width);

			ImGui::PopID();
		}
	}

	ImGui::PopID();
}
#endif

//----------------------------------------------------------------------------
